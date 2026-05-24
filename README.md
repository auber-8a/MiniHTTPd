# MiniHTTPd

Servidor HTTP/1.1 minimalista desarrollado en C para entornos Linux. El proyecto implementa un servidor de archivos estáticos utilizando una arquitectura orientada a eventos (*event-driven*) mediante la llamada al sistema `epoll`, permitiendo gestionar múltiples conexiones concurrentes de forma eficiente en un solo hilo sin el *overhead* de hilos o procesos tradicionales.

## Características

- **Servidor de archivos estáticos:** Capacidad para servir archivos HTML, CSS, JavaScript, imágenes y texto plano.
- **Manejo del protocolo:** Procesamiento estricto del método `GET` y análisis de encabezados básicos (`Host`, `Connection`, `User-Agent`).
- **Arquitectura de alto rendimiento:** Multiplexación de I/O mediante `epoll` con soporte para conexiones persistentes (*Keep-Alive*).
- **Mapeo de contenido:** Implementación manual de tipos MIME.
- **Códigos de estado HTTP:** Soporte para respuestas detalladas ante aciertos y errores comunes (`200`, `400`, `403`, `404`, `405`, `500`).
- **Seguridad integrada:** Mitigación activa contra vulnerabilidades comunes como *Directory Traversal* y *Buffer Overflows*.
- **Terminación limpia:** Manejo de señales (`SIGINT` y `SIGTERM`) para liberar descriptores de archivo y sockets de manera segura.

---

## Estructura del Proyecto

El repositorio está organizado siguiendo la estructura modular especificada para el proyecto:

```text
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── files.h
│   ├── http.h
│   ├── mime.h
│   └── server.h
├── src/
│   ├── files.c
│   ├── http.c
│   ├── main.c
│   ├── mime.c
│   └── server.c
└── www/
    ├── index.html
    ├── style.css
    ├── script.js
    ├── image.png
    ├── linux.jpg
    └── ejemplo.txt
```

## Especificaciones Técnicas

### Códigos de Estado Implementados
El servidor responde con la semántica HTTP adecuada según el escenario:

| Código | Descripción | Condición de Activación |
| :--- | :--- | :--- |
| **200 OK** | OK | El archivo solicitado existe en `www/` y se transmite con éxito. |
| **400 Bad Request** | Bad Request | Solicitud mal formada, encabezados inválidos o URI que excede el límite seguro. |
| **403 Forbidden** | Forbidden | Intento de *Directory Traversal* detectado o restricciones de lectura en el archivo. |
| **404 Not Found** | Not Found | El recurso solicitado no se encuentra dentro del directorio raíz `www/`. |
| **405 Method Not Allowed** | Method Not Allowed | Se recibe un método HTTP diferente a `GET` (por ejemplo, `POST`, `PUT`). |
| **500 Internal Server Error** | Internal Server Error | Errores críticos internos del sistema (fallos en `malloc`, lectura de archivos, etc.). |

### Tipos MIME Soportados
El módulo `mime.c` se encarga de analizar las extensiones de los archivos para enviar el encabezado `Content-Type` correcto:

- `.html` -> `text/html` 
- `.css` -> `text/css` 
- `.js` -> `application/javascript` 
- `.png` -> `image/png` 
- `.jpg` / `.jpeg` -> `image/jpeg`
- `.txt` -> `text/plain`

---

## Seguridad

El desarrollo se diseñó bajo un enfoque de seguridad defensiva para mitigar riesgos comunes en la interacción con el sistema de archivos y redes:

1. **Directory Traversal (`403 Forbidden`):** Se evita que un atacante escape del directorio raíz `www/` mediante el uso de cadenas relativas (`../`). El servidor procesa y valida las rutas absolutas combinando la raíz del servidor con el recurso solicitado, aplicando funciones de sanitización o `realpath()` para asegurar que el archivo final resida estrictamente dentro del espacio permitido.
2. **Buffer Overflows:** Queda estrictamente prohibido el uso de funciones inseguras de la biblioteca estándar como `strcpy` o `sprintf`. En su lugar, se utilizan alternativas seguras con control estricto de límites (`strncpy`, `snprintf`).
3. **Restricción de Recursos:** Se definen buffers estáticos y límites estrictos para la longitud de la línea de solicitud y las cabeceras URI. Cualquier exceso es rechazado inmediatamente de forma segura con un estado `400 Bad Request`.

---

## Modelo de Concurrencia

El servidor utiliza una arquitectura orientada a eventos en un solo hilo (*I/O multiplexing*) mediante el uso de la API `epoll` del kernel de Linux[cite: 6, 13, 18]:

```c
while (running) {
    int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    for (int i = 0; i < n; i++) {
        if (events[i].data.fd == server_fd) {
            aceptar_conexion();
        } else {
            manejar_cliente(events[i].data.fd);
        }
    }
}
```

## Compilación y Ejecución

El proyecto incluye un `Makefile` para automatizar el ciclo de construcción:

### Compilar el servidor
```bash
make
```

### Limpiar los binarios generados
```bash
make clean
```

### Lanzar el servidor (Puerto por defecto u opcional)
```bash
./minihttpd
# O especificando un puerto alternativo
./minihttpd 9090
```

## Pruebas de Funcionamiento
### Desde la Terminal (utilizando `curl`)

**Petición exitosa de recurso raíz:**
```bash
curl -v http://localhost:8080/
```


Petición de recursos con diferentes tipos MIME:
```bash
curl -v http://localhost:8080/style.css
curl -v http://localhost:8080/script.js
curl -v http://localhost:8080/image.png
```

Prueba de recurso no existente (404):
```bash
curl -v http://localhost:8080/archivo_fantasma.html
```

Prueba de inyección/ataque Directory Traversal (403):
```bash
curl -v http://localhost:8080/../../etc/passwd
```

Prueba de método no soportado (405):

```bash
curl -X POST http://localhost:8080/ -v
```