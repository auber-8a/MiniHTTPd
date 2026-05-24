# MiniHTTPd

Servidor HTTP/1.1 minimalista implementado en C como proyecto integrador. El servidor utiliza epoll para manejo de conexiones concurrentes, soporta conexiones persistentes (keep-alive), sirve archivos estáticos y maneja múltiples tipos MIME.

## Características

- Servidor de archivos estáticos (HTML, CSS, JavaScript, imagenes, texto plano)
- Metodo GET implementado
- epoll para multiplexacion de I/O (concurrencia de un solo hilo)
- Conexiones persistentes (Keep-Alive) con timeout configurable
- Tipos MIME soportados: text/html, text/css, application/javascript, image/png, image/jpeg, text/plain
- Codigos de estado HTTP: 200, 400, 403, 404, 405, 500
- Seguridad contra directory traversal (bloqueo de rutas como ../, /etc/, etc.)
- Manejo de señales SIGINT y SIGTERM para terminacion limpia
- Estructura modular organizada en headers y fuentes separados

## Requisitos del sistema

- Sistema operativo Linux (por uso de epoll)
- Compilador GCC
- Make

## Estructura del proyecto
minihttpd/
├── Makefile
├── README.md
├── .gitignore
├── include/
│   ├── files.h
│   ├── http.h
│   ├── mime.h
│   └── server.h
├── src/
│   ├── main.c
│   ├── server.c
│   ├── http.c
│   ├── files.c
│   └── mime.c
└── www/
    ├── index.html
    ├── style.css
    ├── script.js
    ├── image.png
    ├── linux.jpg
    └── ejemplo.txt



## Compilacion

```bash
make
make clean
make run