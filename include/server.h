#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <time.h>

#define MAX_REQ_SIZE 8192

typedef struct {
    int fd;
    char buffer[MAX_REQ_SIZE + 1];
    size_t bytes_leidos;
    time_t ultima_actividad;
} cliente_t;

int crear_socket_servidor(const char *puerto, int backlog);
void ejecutar_bucle_epoll(int listen_fd);

#endif