#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include "server.h"

int escribir_todo(int fd, const void *buf, size_t len);
void enviar_error_http(int fd, int codigo, const char *estado, const char *mensaje, int mantener_viva);
int manejar_datos_cliente(cliente_t *cli);

#endif