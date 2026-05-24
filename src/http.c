#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "http.h"

int parse_http_request(const char* buffer, http_request_t* request) {
    memset(request, 0, sizeof(http_request_t));
    request->keep_alive = 0;
    
    if (sscanf(buffer, "%s %s %s", request->method, request->uri, request->version) != 3) {
        return -1;
    }
    
    if (strstr(buffer, "Connection: keep-alive") || 
        strstr(buffer, "connection: keep-alive")) {
        request->keep_alive = 1;
    }
    
    return 0;
}

void enviar_respuesta(int client_fd, int status_code, const char* status_text,
                      const char* content_type, const char* body, int body_len,
                      int keep_alive) {
    char response[8192];
    int response_len;
    
    printf("📤 Enviando respuesta: %d %s\n", status_code, status_text);
    
    if (keep_alive) {
        response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "%s",
            status_code, status_text, content_type, body_len, body ? body : "");
    } else {
        response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            status_code, status_text, content_type, body_len, body ? body : "");
    }
    
    send(client_fd, response, response_len, 0);
}