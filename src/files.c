#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "files.h"
#include "mime.h"
#include "http.h"

int es_ruta_segura(const char* uri) {
    // Detectar directory traversal
    if (strstr(uri, "..") != NULL) {
        printf("🔒 403 Forbidden - Directory traversal detectado: %s\n", uri);
        return 0;
    }
    
    // Detectar rutas absolutas del sistema
    const char* rutas_peligrosas[] = {"/etc/", "/bin/", "/usr/", "/home/", "/root/", "/var/", "/proc/", "/dev/", "/boot/", "/sys/"};
    for (int i = 0; i < 10; i++) {
        if (strstr(uri, rutas_peligrosas[i]) != NULL) {
            printf("🔒 403 Forbidden - Ruta peligrosa detectada: %s\n", uri);
            return 0;
        }
    }
    
    // Detectar caracteres peligrosos
    if (strchr(uri, '\\') != NULL) return 0;
    if (strchr(uri, ':') != NULL) return 0;
    
    return 1;
}

// FUERZA 500 PARA PRUEBA (eliminar después)

void servir_archivo(int client_fd, const char* uri, int keep_alive) {
    // Verificar seguridad PRIMERO
    if (!es_ruta_segura(uri)) {
        enviar_respuesta(client_fd, 403, "Forbidden", "text/html", 
                        "<html><body><h1>403 Forbidden</h1></body></html>", 45, keep_alive);
        return;
    }
    
    char filepath[512];
    if (strcmp(uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "www/index.html");
    } else {
        snprintf(filepath, sizeof(filepath), "www%s", uri);
    }
    
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        printf("❌ 404 Not Found - Archivo no existe: %s\n", filepath);
        char *error_body = "<html><body><h1>404 Not Found</h1></body></html>";
        enviar_respuesta(client_fd, 404, "Not Found", "text/html", error_body, strlen(error_body), keep_alive);
        return;
    }
    
    struct stat st;
    if (fstat(fd, &st) == -1) {
        printf("❌ 500 Internal Server Error - fstat falló: %s\n", filepath);
        close(fd);
        enviar_respuesta(client_fd, 500, "Internal Server Error", "text/html", "", 0, keep_alive);
        return;
    }
    
    int file_size = st.st_size;
    char *file_content = malloc(file_size);
    if (file_content == NULL) {
        printf("❌ 500 Internal Server Error - malloc falló\n");
        close(fd);
        enviar_respuesta(client_fd, 500, "Internal Server Error", "text/html", "", 0, keep_alive);
        return;
    }
    
    read(fd, file_content, file_size);
    close(fd);
    
    const char* mime = get_mime_type(filepath);
    
    char header[1024];
    int header_len;
    
    if (keep_alive) {
        header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: keep-alive\r\n"
            "\r\n",
            mime, file_size);
    } else {
        header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            mime, file_size);
    }
    
    send(client_fd, header, header_len, 0);
    send(client_fd, file_content, file_size, 0);
    
    printf("✅ 200 OK: %s (%d bytes, %s)\n", filepath, file_size, mime);
    
    free(file_content);
}