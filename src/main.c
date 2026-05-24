#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "server.h"

#define MAX_EVENTS 64

volatile sig_atomic_t keep_running = 1;
int global_server_fd = -1;
int global_epoll_fd = -1;

void handle_signal(int sig) {
    (void)sig;
    printf("\n🔴 Cerrando servidor...\n");
    keep_running = 0;
    if (global_server_fd != -1) close(global_server_fd);
    if (global_epoll_fd != -1) close(global_epoll_fd);
}

int configurar_signals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) return -1;
    if (sigaction(SIGTERM, &sa, NULL) == -1) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    const char* puerto = argc > 1 ? argv[1] : "8080";
    
    printf("========================================\n");
    printf("   MiniHTTPd Server v1.0\n");
    printf("========================================\n");
    
    if (configurar_signals() == -1) return 1;
    
    int server_fd = crear_socket_servidor(puerto);
    if (server_fd == -1) return 1;
    global_server_fd = server_fd;
    
    int epoll_fd = configurar_epoll(server_fd);
    if (epoll_fd == -1) return 1;
    global_epoll_fd = epoll_fd;
    
    printf("🚀 Servidor en puerto %s\n", puerto);
    printf("📁 Sirviendo desde ./www/\n");
    printf("💡 Ctrl+C para salir\n\n");
    
    struct epoll_event events[MAX_EVENTS];
    
    while (keep_running) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        if (n == -1 && errno != EINTR) break;
        
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == server_fd)
                aceptar_conexion(server_fd, epoll_fd);
            else if (events[i].events & EPOLLIN)
                manejar_cliente(fd);
        }
    }
    
    printf("\n🛑 Apagando...\n");
    return 0;
}