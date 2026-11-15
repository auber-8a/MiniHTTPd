#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "server.h"

int main(int argc, char *argv[]) {
    const char *puerto = (argc >= 2) ? argv[1] : "8080";

    signal(SIGPIPE, SIG_IGN);

    int listen_fd = crear_socket_servidor(puerto, 128);
    if (listen_fd < 0) {
        return 1;
    }

    printf("[minihttpd] Servidor epoll escuchando en http://localhost:%s\n", puerto);
    ejecutar_bucle_epoll(listen_fd);

    close(listen_fd);
    return 0;
}