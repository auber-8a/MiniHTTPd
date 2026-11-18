#ifndef FILES_H
#define FILES_H

#include <stddef.h>

int resolver_ruta_segura(const char *uri, char *ruta_destino, size_t max_len);
int servir_archivo_http(int fd, const char *ruta_archivo, int mantener_viva);

#endif