#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include "http.h"
#include "files.h"

#define MAX_LINE_LEN 2048
#define MAX_URI_LEN 1024

int escribir_todo(int fd, const void *buf, size_t len) {
    const char *ptr = (const char *)buf;
    size_t total = 0;

    while (total < len) {
        ssize_t n = write(fd, ptr + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct timespec ts = {0, 1000000L};
		nanosleep(&ts, NULL);
                continue;
            }
            return -1;
        }
        if (n == 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

void enviar_error_http(int fd, int codigo, const char *estado, const char *mensaje, int mantener_viva) {
    char header[512];
    char body[512];

    int body_len = snprintf(body, sizeof(body),
                            "<!DOCTYPE html><html><head><title>%d %s</title></head>"
                            "<body><h1>%d %s</h1><p>%s</p></body></html>\n",
                            codigo, estado, codigo, estado, mensaje);

    int head_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 %d %s\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: %s\r\n"
                            "\r\n",
                            codigo, estado, body_len,
                            mantener_viva ? "keep-alive" : "close");

    if (head_len > 0 && head_len < (int)sizeof(header) &&
        body_len > 0 && body_len < (int)sizeof(body)) {
        escribir_todo(fd, header, (size_t)head_len);
    escribir_todo(fd, body, (size_t)body_len);
        }
}

static void limpiar_espacios(char *str) {
    char *inicio = str;
    while (*inicio == ' ' || *inicio == '\t') inicio++;
    if (inicio != str) memmove(str, inicio, strlen(inicio) + 1);

    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

static int procesar_peticion_completa(cliente_t *cli, char *raw_req) {
    char *fin_linea = strstr(raw_req, "\r\n");
    if (!fin_linea) {
        enviar_error_http(cli->fd, 400, "Bad Request", "Formato de linea invalido.", 0);
        return 0;
    }

    size_t len_req_line = (size_t)(fin_linea - raw_req);
    if (len_req_line == 0 || len_req_line >= MAX_LINE_LEN) {
        enviar_error_http(cli->fd, 400, "Bad Request", "Linea de solicitud excede el limite.", 0);
        return 0;
    }

    *fin_linea = '\0';

    /* Tokenizar Metodo, URI y Version sin desbordamiento */
    char *sp1 = strchr(raw_req, ' ');
    if (!sp1) {
        enviar_error_http(cli->fd, 400, "Bad Request", "Faltan componentes en la solicitud.", 0);
        return 0;
    }
    *sp1 = '\0';
    char *metodo = raw_req;

    char *uri = sp1 + 1;
    char *sp2 = strchr(uri, ' ');
    if (!sp2) {
        enviar_error_http(cli->fd, 400, "Bad Request", "Version HTTP faltante.", 0);
        return 0;
    }
    *sp2 = '\0';
    char *version = sp2 + 1;

    if (strchr(version, ' ') != NULL || strlen(uri) == 0 || strlen(uri) >= MAX_URI_LEN) {
        enviar_error_http(cli->fd, 400, "Bad Request", "URI o version HTTP invalida.", 0);
        return 0;
    }

    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0) {
        enviar_error_http(cli->fd, 400, "Bad Request", "Version de protocolo no soportada.", 0);
        return 0;
    }

    /* Parsear encabezados basicos: Host, Connection, User-Agent */
    int tiene_host = 0;
    int mantener_viva = (strcmp(version, "HTTP/1.1") == 0) ? 1 : 0;
    char user_agent[256] = "Desconocido";

    char *linea = fin_linea + 2;
    while (*linea != '\0') {
        char *sig = strstr(linea, "\r\n");
        if (!sig) break;
        if (sig == linea) break; /* Linea vacia: fin de encabezados */

            if ((size_t)(sig - linea) >= MAX_LINE_LEN) {
                enviar_error_http(cli->fd, 400, "Bad Request", "Encabezado demasiado largo.", 0);
                return 0;
            }

            *sig = '\0';
        char *dos_puntos = strchr(linea, ':');
        if (!dos_puntos) {
            enviar_error_http(cli->fd, 400, "Bad Request", "Encabezado mal formado.", 0);
            return 0;
        }

        *dos_puntos = '\0';
        char *nombre = linea;
        char *valor = dos_puntos + 1;
        limpiar_espacios(nombre);
        limpiar_espacios(valor);

        if (strcasecmp(nombre, "Host") == 0) {
            if (strlen(valor) > 0) tiene_host = 1;
        } else if (strcasecmp(nombre, "Connection") == 0) {
            if (strcasecmp(valor, "close") == 0) mantener_viva = 0;
            else if (strcasecmp(valor, "keep-alive") == 0) mantener_viva = 1;
        } else if (strcasecmp(nombre, "User-Agent") == 0) {
            snprintf(user_agent, sizeof(user_agent), "%s", valor);
        }

        linea = sig + 2;
    }

    /* En HTTP/1.1 el encabezado Host es obligatorio */
    if (strcmp(version, "HTTP/1.1") == 0 && !tiene_host) {
        enviar_error_http(cli->fd, 400, "Bad Request", "Encabezado Host obligatorio en HTTP/1.1.", 0);
        return 0;
    }

    /* Validar metodo GET */
    if (strcmp(metodo, "GET") != 0) {
        enviar_error_http(cli->fd, 405, "Method Not Allowed", "Solo se permite el metodo GET.", mantener_viva);
        return mantener_viva;
    }

    /* Resolver ruta con realpath() y servir archivo */
    char ruta_real[4096];
    int res_ruta = resolver_ruta_segura(uri, ruta_real, sizeof(ruta_real));
    if (res_ruta == -403) {
        enviar_error_http(cli->fd, 403, "Forbidden", "Acceso denegado (Directory Traversal bloqueado).", mantener_viva);
        return mantener_viva;
    } else if (res_ruta == -404) {
        enviar_error_http(cli->fd, 404, "Not Found", "El recurso solicitado no existe.", mantener_viva);
        return mantener_viva;
    } else if (res_ruta < 0) {
        enviar_error_http(cli->fd, 500, "Internal Server Error", "Error interno resolviendo la ruta.", 0);
        return 0;
    }

    int res_archivo = servir_archivo_http(cli->fd, ruta_real, mantener_viva);
    if (res_archivo == -403) {
        enviar_error_http(cli->fd, 403, "Forbidden", "No tiene permisos para leer el recurso.", mantener_viva);
    } else if (res_archivo == -404) {
        enviar_error_http(cli->fd, 404, "Not Found", "El recurso solicitado no existe.", mantener_viva);
    } else if (res_archivo < 0) {
        enviar_error_http(cli->fd, 500, "Internal Server Error", "Error al leer el archivo en el servidor.", 0);
        return 0;
    }

    return mantener_viva;
}

int manejar_datos_cliente(cliente_t *cli) {
    while (1) {
        size_t espacio = MAX_REQ_SIZE - cli->bytes_leidos;
        if (espacio == 0) {
            enviar_error_http(cli->fd, 400, "Bad Request", "Solicitud excede el tamano maximo permitido.", 0);
            return 0;
        }

        ssize_t n = recv(cli->fd, cli->buffer + cli->bytes_leidos, espacio, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return 0;
        }
        if (n == 0) {
            return 0; /* Cliente cerro conexion */
        }

        cli->bytes_leidos += (size_t)n;
        cli->buffer[cli->bytes_leidos] = '\0';

        /* Procesar todas las peticiones completas acumuladas en el buffer (pipelining) */
        char *fin_headers;
        while ((fin_headers = strstr(cli->buffer, "\r\n\r\n")) != NULL) {
            size_t len_peticion = (size_t)(fin_headers - cli->buffer) + 4;

            char copia_req[MAX_REQ_SIZE + 1];
            memcpy(copia_req, cli->buffer, len_peticion);
            copia_req[len_peticion] = '\0';

            /* Desplazar bytes restantes (si hubo pipelining) al inicio del buffer */
            size_t restantes = cli->bytes_leidos - len_peticion;
            memmove(cli->buffer, cli->buffer + len_peticion, restantes);
            cli->bytes_leidos = restantes;
            cli->buffer[cli->bytes_leidos] = '\0';

            if (!procesar_peticion_completa(cli, copia_req)) {
                return 0;
            }
        }
    }
    return 1;
}