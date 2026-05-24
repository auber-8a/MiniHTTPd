#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "server.h"
#include "http.h"
#include "files.h"

#define BUFFER_SIZE 4096

// Estructura para almacenar información de cada cliente
typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    int buffer_len;
    time_t last_activity;
    int keep_alive;
} client_t;

// Array de clientes activos (máximo 64)
static client_t* clients[64];

// Buscar cliente por file descriptor
static client_t* buscar_cliente(int fd) {
    for (int i = 0; i < 64; i++) {
        if (clients[i] && clients[i]->fd == fd) {
            return clients[i];
        }
    }
    return NULL;
}

// Agregar nuevo cliente al array
static void agregar_cliente(int fd) {
    for (int i = 0; i < 64; i++) {
        if (clients[i] == NULL) {
            clients[i] = malloc(sizeof(client_t));
            clients[i]->fd = fd;
            clients[i]->buffer_len = 0;
            clients[i]->last_activity = time(NULL);
            clients[i]->keep_alive = 0;
            memset(clients[i]->buffer, 0, BUFFER_SIZE);
            break;
        }
    }
}

// Eliminar cliente del array
static void eliminar_cliente(int fd) {
    for (int i = 0; i < 64; i++) {
        if (clients[i] && clients[i]->fd == fd) {
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }
}

// Crear socket del servidor
int crear_socket_servidor(const char* puerto) {
    struct addrinfo hints, *result, *rp;
    int server_fd = -1;
    int opt = 1;
    
    // Configurar hints para getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // Para bind()
    
    // Obtener lista de direcciones
    int s = getaddrinfo(NULL, puerto, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "Error en getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }
    
    // Intentar crear socket con cada dirección
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_fd == -1) continue;
        
        // Reutilizar puerto (evita "Address already in use")
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            close(server_fd);
            continue;
        }
        
        // Hacer socket no bloqueante (IMPORTANTE para epoll)
        int flags = fcntl(server_fd, F_GETFL, 0);
        if (flags == -1 || fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(server_fd);
            continue;
        }
        
        // Asociar socket a la dirección
        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        
        close(server_fd);
    }
    
    freeaddrinfo(result);
    
    if (rp == NULL) {
        fprintf(stderr, "No se pudo bindear a ninguna dirección\n");
        return -1;
    }
    
    // Poner socket en modo escucha
    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

// Configurar epoll
int configurar_epoll(int server_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) return -1;
    
    struct epoll_event ev;
    ev.events = EPOLLIN;          // Monitorear eventos de lectura
    ev.data.fd = server_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        close(epoll_fd);
        return -1;
    }
    
    return epoll_fd;
}

// Aceptar nueva conexión entrante
void aceptar_conexion(int server_fd, int epoll_fd) {
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd == -1) return;
    
    // Hacer socket del cliente no bloqueante
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Agregar cliente a epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
    
    // Guardar cliente en la lista
    agregar_cliente(client_fd);
    
    // Mostrar IP del cliente
    char ip_str[INET6_ADDRSTRLEN];
    if (client_addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
        inet_ntop(AF_INET6, &s->sin6_addr, ip_str, sizeof(ip_str));
    }
    printf("📱 Nueva conexión de %s (fd=%d)\n", ip_str, client_fd);
}

// Manejar datos recibidos del cliente
void manejar_cliente(int client_fd) {
    client_t* client = buscar_cliente(client_fd);
    if (!client) return;
    
    char temp[BUFFER_SIZE];
    ssize_t bytes = recv(client_fd, temp, sizeof(temp) - 1, 0);
    
    if (bytes <= 0) {
        if (bytes == 0) {
            printf("🔌 Cliente %d desconectado\n", client_fd);
        }
        close(client_fd);
        eliminar_cliente(client_fd);
        return;
    }
    
    temp[bytes] = '\0';
    
    // Acumular datos en el buffer del cliente
    if (client->buffer_len + bytes < BUFFER_SIZE - 1) {
        memcpy(client->buffer + client->buffer_len, temp, bytes);
        client->buffer_len += bytes;
        client->buffer[client->buffer_len] = '\0';
        client->last_activity = time(NULL);
    }
    
    // Parsear la petición HTTP
    http_request_t request;
    if (parse_http_request(client->buffer, &request) == 0) {
        printf("📝 %s %s %s\n", request.method, request.uri, request.version);
        
        // Verificar que sea método GET
        if (strcmp(request.method, "GET") != 0) {
            printf("❌ 405 Method Not Allowed - Método %s no soportado\n", request.method);
            enviar_respuesta(client_fd, 405, "Method Not Allowed", "text/html", "", 0, 0);
            close(client_fd);
            eliminar_cliente(client_fd);
        } else {
            // Servir el archivo solicitado
            servir_archivo(client_fd, request.uri, request.keep_alive);
            
            if (!request.keep_alive) {
                close(client_fd);
                eliminar_cliente(client_fd);
            } else {
                // Limpiar buffer para el siguiente request en la misma conexión
                client->buffer_len = 0;
                memset(client->buffer, 0, BUFFER_SIZE);
            }
        }
    } else {
        // Request mal formado
        printf("❌ 400 Bad Request - Solicitud mal formada\n");
        enviar_respuesta(client_fd, 400, "Bad Request", "text/html", "", 0, 0);
        close(client_fd);
        eliminar_cliente(client_fd);
    }
}