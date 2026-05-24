#ifndef SERVER_H
#define SERVER_H

int crear_socket_servidor(const char* puerto);
int configurar_epoll(int server_fd);
void aceptar_conexion(int server_fd, int epoll_fd);
void manejar_cliente(int client_fd);

#endif
