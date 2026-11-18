#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include "server.h"
#include "http.h"

#define MAX_EVENTOS 64
#define MAX_CLIENTES 1024
#define TIMEOUT_KEEPALIVE 15

static int hacer_no_bloqueante(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int crear_socket_servidor(const char *puerto, int backlog) {
    struct addrinfo hints, *res, *rp;
    int sfd = -1, optval = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(NULL, puerto, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd < 0) continue;

        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sfd);
        sfd = -1;
    }
    freeaddrinfo(res);

    if (sfd < 0) {
        perror("bind");
        return -1;
    }

    if (hacer_no_bloqueante(sfd) < 0 || listen(sfd, backlog) < 0) {
        perror("listen/nonblock");
        close(sfd);
        return -1;
    }

    return sfd;
}

static void cerrar_cliente(int epfd, cliente_t **clientes, int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    if (fd >= 0 && fd < MAX_CLIENTES && clientes[fd] != NULL) {
        free(clientes[fd]);
        clientes[fd] = NULL;
    }
}

void ejecutar_bucle_epoll(int listen_fd) {
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return;
    }

    struct epoll_event ev, eventos[MAX_EVENTOS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl listen_fd");
        close(epfd);
        return;
    }

    cliente_t *clientes[MAX_CLIENTES] = {0};

    while (1) {
        int n = epoll_wait(epfd, eventos, MAX_EVENTOS, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        time_t ahora = time(NULL);

        for (int i = 0; i < n; i++) {
            int fd = eventos[i].data.fd;

            if (fd == listen_fd) {
                while (1) {
                    int cfd = accept(listen_fd, NULL, NULL);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }

                    if (cfd >= MAX_CLIENTES || hacer_no_bloqueante(cfd) < 0) {
                        close(cfd);
                        continue;
                    }

                    cliente_t *cli = calloc(1, sizeof(cliente_t));
                    if (!cli) {
                        close(cfd);
                        continue;
                    }
                    cli->fd = cfd;
                    cli->ultima_actividad = ahora;
                    clientes[cfd] = cli;

                    struct epoll_event cev;
                    cev.events = EPOLLIN | EPOLLRDHUP;
                    cev.data.fd = cfd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev) < 0) {
                        cerrar_cliente(epfd, clientes, cfd);
                    }
                }
            } else {
                cliente_t *cli = (fd < MAX_CLIENTES) ? clientes[fd] : NULL;
                if (!cli) {
                    cerrar_cliente(epfd, clientes, fd);
                    continue;
                }

                if (eventos[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                    cerrar_cliente(epfd, clientes, fd);
                    continue;
                }

                if (eventos[i].events & EPOLLIN) {
                    int mantener = manejar_datos_cliente(cli);
                    cli->ultima_actividad = ahora;
                    if (!mantener) {
                        cerrar_cliente(epfd, clientes, fd);
                    }
                }
            }
        }

        for (int fd = 0; fd < MAX_CLIENTES; fd++) {
            if (clientes[fd] && (ahora - clientes[fd]->ultima_actividad) > TIMEOUT_KEEPALIVE) {
                cerrar_cliente(epfd, clientes, fd);
            }
        }
    }

    close(epfd);
}
