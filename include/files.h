#ifndef FILES_H
#define FILES_H

// Declaraciones públicas
int es_ruta_segura(const char* uri);
void servir_archivo(int client_fd, const char* uri, int keep_alive);

#endif