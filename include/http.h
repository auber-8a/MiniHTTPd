#ifndef HTTP_H
#define HTTP_H

#include <sys/types.h>

typedef struct {
    char method[16];
    char uri[2048];
    char version[16];
    int keep_alive;
} http_request_t;

int parse_http_request(const char* buffer, http_request_t* request);
void enviar_respuesta(int client_fd, int status_code, const char* status_text,
                      const char* content_type, const char* body, int body_len,
                      int keep_alive);

#endif
