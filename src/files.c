#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include "files.h"
#include "mime.h"
#include "http.h"

int resolver_ruta_segura(const char *uri, char *ruta_destino, size_t max_len) {
    char base_real[PATH_MAX];
    char ruta_candidata[PATH_MAX];
    char resuelta[PATH_MAX];

    if (!realpath("www", base_real)) {
        return -500;
    }

    char uri_limpia[1024];
    snprintf(uri_limpia, sizeof(uri_limpia), "%s", uri);
    char *query = strchr(uri_limpia, '?');
    if (query) *query = '\0';

    if (uri_limpia[0] != '/') {
        return -403;
    }

    if (strcmp(uri_limpia, "/") == 0) {
        snprintf(ruta_candidata, sizeof(ruta_candidata), "%s/index.html", base_real);
    } else {
        snprintf(ruta_candidata, sizeof(ruta_candidata), "%s%s", base_real, uri_limpia);
    }

    if (!realpath(ruta_candidata, resuelta)) {
        if (errno == EACCES || strstr(uri_limpia, "..") != NULL) return -403;
        if (errno == ENOENT || errno == ENOTDIR) return -404;
        return -500;
    }    


    size_t len_base = strlen(base_real);
    if (strncmp(resuelta, base_real, len_base) != 0 ||
        (resuelta[len_base] != '\0' && resuelta[len_base] != '/')) {
        return -403;
        }

        if (snprintf(ruta_destino, max_len, "%s", resuelta) >= (int)max_len) {
            return -500;
        }

        return 0;
}

int servir_archivo_http(int fd, const char *ruta_archivo, int mantener_viva) {
    struct stat st;
    if (stat(ruta_archivo, &st) < 0) {
        if (errno == ENOENT) return -404;
        if (errno == EACCES) return -403;
        return -500;
    }

    if (!S_ISREG(st.st_mode)) {
        return -403;
    }

    FILE *fp = fopen(ruta_archivo, "rb");
    if (!fp) {
        if (errno == EACCES) return -403;
        if (errno == ENOENT) return -404;
        return -500;
    }

    const char *mime = obtener_tipo_mime(ruta_archivo);
    char header[512];

    int head_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Length: %lld\r\n"
                            "Connection: %s\r\n"
                            "\r\n",
                            mime, (long long)st.st_size,
                            mantener_viva ? "keep-alive" : "close");

    if (head_len < 0 || head_len >= (int)sizeof(header)) {
        fclose(fp);
        return -500;
    }

    if (escribir_todo(fd, header, (size_t)head_len) < 0) {
        fclose(fp);
        return -1;
    }

    char buf[4096];
    size_t leidos;
    while ((leidos = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (escribir_todo(fd, buf, leidos) < 0) {
            fclose(fp);
            return -1;
        }
    }

    if (ferror(fp)) {
        fclose(fp);
        return -500;
    }

    fclose(fp);
    return 0;
}
