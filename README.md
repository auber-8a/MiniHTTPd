# MiniHTTPd

MiniHTTPd es un servidor HTTP pequeño desarrollado en C para Linux. Sirve archivos desde una carpeta local y atiende solicitudes HTTP con sockets y epoll.

## Objetivo

El programa permite abrir una página web desde el navegador sin usar bibliotecas HTTP externas.

El servidor puede:

- Servir HTML, CSS, JavaScript e imágenes.
- Procesar solicitudes GET.
- Leer Host, Connection y User-Agent.
- Mantener varias conexiones abiertas.
- Responder con distintos códigos HTTP.
- Bloquear rutas que intenten salir de la carpeta pública.
- Evitar solicitudes excesivamente grandes.

## Herramientas utilizadas

- Linux.
- GCC.
- Sockets POSIX.
- epoll.
- HTTP/1.0 y HTTP/1.1.
- Archivos y directorios del sistema.

## Estructura del proyecto

```text
MiniHTTPd/
├── Makefile
├── README.md
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
    └── image.png
```

## Archivos del programa

### main.c

Es el punto de entrada del programa.

```c
int main(int argc, char *argv[])
{
    const char *puerto = "8080"

    if (argc >= 2)
        puerto = argv[1]

    crear socket
    iniciar bucle epoll
}
```

Si no se indica un puerto, se utiliza el 8080.

Ejemplos:

```bash
./minihttpd
./minihttpd 9000
```

---

### server.h

Contiene la estructura usada para representar a un cliente.

```c
typedef struct
{
    int fd
    char buffer[MAX_REQ_SIZE + 1]
    size_t bytes_leidos
    time_t ultima_actividad
} cliente_t
```

- `fd` es el identificador del socket.
- `buffer` guarda temporalmente la solicitud.
- `bytes_leidos` indica cuántos datos llegaron.
- `ultima_actividad` permite cerrar conexiones inactivas.

---

### server.c

Contiene el manejo de sockets y epoll.

Sus responsabilidades principales son:

- Crear el socket.
- Asociarlo con un puerto.
- Escuchar conexiones.
- Configurar los sockets como no bloqueantes.
- Aceptar nuevos clientes.
- Leer eventos producidos por epoll.
- Cerrar clientes inactivos.
- Manejar errores del socket.

Ejemplo de la creación del socket:

```c
crear socket TCP
configurar SO_REUSEADDR
convertir el socket a modo no bloqueante
asociarlo al puerto con bind
esperar conexiones con listen
```

---

### http.h

Declara las funciones relacionadas con solicitudes y respuestas HTTP.

Incluye:

- `escribir_todo`
- `enviar_error_http`
- `manejar_datos_cliente`

---

### http.c

Es el componente encargado de interpretar HTTP.

Una solicitud normal tiene esta forma:

```text
GET /index.html HTTP/1.1
Host: localhost
Connection: keep-alive
User-Agent: navegador
```

El programa debe:

- Encontrar el final de la línea de solicitud.
- Separar método, URI y versión.
- Comprobar que el formato sea válido.
- Buscar los encabezados.
- Rechazar métodos diferentes de GET.
- Resolver la ruta solicitada.
- Enviar el archivo o el error correspondiente.

Ejemplo conceptual:

```c
recibir datos
buscar "\r\n\r\n"
separar método, URI y versión
leer Host
leer Connection
leer User-Agent
validar la solicitud
procesar GET
```

---

### files.h

Declara las funciones encargadas de acceder a los archivos.

```c
int resolver_ruta_segura(...)
int servir_archivo_http(...)
```

---

### files.c

Contiene las funciones para buscar y enviar archivos.

El servidor utiliza `www` como carpeta pública.

Por ejemplo:

```text
GET /          -> www/index.html
GET /style.css -> www/style.css
GET /image.png -> www/image.png
```

Antes de devolver un archivo, el programa comprueba:

- Que exista.
- Que sea un archivo normal.
- Que pueda leerse.
- Que esté dentro de `www`.
- Que no haya utilizado Directory Traversal.

---

### mime.h

Declara la función que devuelve el tipo MIME.

```c
const char *obtener_tipo_mime(...)
```

---

### mime.c

Relaciona una extensión con su tipo de contenido.

```text
.html  -> text/html
.css   -> text/css
.js    -> application/javascript
.png   -> image/png
.jpg   -> image/jpeg
```

Si no reconoce la extensión:

```text
application/octet-stream
```

---

## Arquitectura del programa

El programa sigue una arquitectura event-driven.

Esto significa que no crea un hilo por cliente. En vez de eso, mantiene un solo bucle principal que pregunta a epoll qué conexiones tienen actividad.

```text
Navegador
    |
    | Solicitud HTTP
    v
Socket principal
    |
    | Nueva conexión
    v
epoll
    |
    +-------------------------------+
    |                               |
    v                               v
Cliente 1                        Cliente 2
    |                               |
    | Datos disponibles             | Datos disponibles
    v                               v
Analizar solicitud               Analizar solicitud
    |                               |
    v                               v
Leer archivo / crear error       Leer archivo / crear error
    |                               |
    v                               v
Enviar respuesta                 Enviar respuesta
```

## Flujo de ejecución

### Inicio

```text
main
  |
  v
Elegir puerto
  |
  v
Crear socket
  |
  v
Asociar socket al puerto
  |
  v
Escuchar conexiones
  |
  v
Crear epoll
  |
  v
Entrar en el bucle de eventos
```

### Cuando llega una conexión

```text
epoll detecta el socket principal
  |
  v
accept
  |
  v
Crear estructura cliente_t
  |
  v
Registrar el socket en epoll
```

### Cuando llega una solicitud

```text
epoll detecta datos en el cliente
  |
  v
recv
  |
  v
Guardar datos en el búfer
  |
  v
Buscar "\r\n\r\n"
  |
  v
Separar la línea de solicitud
  |
  v
Separar los encabezados
  |
  v
Validar método, URI y versión
  |
  v
Resolver ruta
  |
  v
Enviar archivo o error
```

## Modelo de procesamiento

### Socket principal

El socket principal es el que recibe nuevas conexiones. Su tarea no es atender directamente a todas las solicitudes.

### Sockets de cliente

Después de aceptar una conexión, cada cliente obtiene su propio descriptor de socket.

### epoll

epoll vigila todos los sockets registrados y devuelve únicamente los que tienen actividad.

Ventajas:

- Una sola tarea principal.
- Menos gastos de memoria.
- Varias conexiones pueden atenderse sin esperar una a una.
- Es adecuado para servicios de red pequeños.

## Solicitudes HTTP

### Solicitud correcta

```text
GET /index.html HTTP/1.1
Host: localhost:8080
Connection: keep-alive
User-Agent: Mozilla
```

El servidor:

1. Comprueba que el método sea GET.
2. Comprueba que la versión sea válida.
3. Comprueba que exista Host.
4. Quita parámetros después de `?`.
5. Busca el archivo dentro de `www`.
6. Devuelve `200 OK`.

### Conexión persistente

HTTP/1.1 mantiene abierta la conexión por defecto.

Ejemplo:

```text
Connection: keep-alive
```

HTTP/1.0 puede cerrarla por defecto.

Ejemplo:

```text
Connection: close
```

Si el cliente no envía datos durante el tiempo límite, el servidor cierra la conexión.

## Códigos HTTP implementados

| Código | Mensaje | Cuando se utiliza |
|---|---|---|
| 200 | OK | Se encontró y pudo enviarse el archivo |
| 400 | Bad Request | La solicitud está mal formada o es demasiado grande |
| 403 | Forbidden | Se intenta salir de `www` o no hay permiso |
| 404 | Not Found | El archivo no existe |
| 405 | Method Not Allowed | El método no es GET |
| 500 | Internal Server Error | Ocurrió un error interno del servidor |

## Generación de respuestas

Una respuesta exitosa tiene esta estructura:

```text
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: tamaño
Connection: keep-alive

contenido del archivo
```

Una respuesta de error tiene:

```text
HTTP/1.1 404 Not Found
Content-Type: text/html
Content-Length: tamaño
Connection: keep-alive

página HTML con el error
```

## Protección contra Directory Traversal

Una solicitud como esta podría intentar salir de la carpeta pública:

```text
GET /../../etc/passwd HTTP/1.1
Host: localhost
```

Para evitarlo:

- `www` se convierte en una ruta absoluta real.
- La ruta solicitada se convierte en una ruta absoluta real.
- Se revisa si la ruta obtenida continúa dentro de `www`.
- Si sale de esa carpeta, se devuelve `403 Forbidden`.

Idea del proceso:

```c
base_real = ruta real de www
ruta_candidata = base_real + URI
ruta_real = realpath(ruta_candidata)
comprobar que ruta_real empieza dentro de base_real
```

No es suficiente revisar solamente si aparece `..` en el texto. `realpath` resuelve enlaces, rutas relativas y otros componentes antes de comprobar la ubicación final.

## Protección contra desbordamientos

No se utilizan técnicas inseguras como:

- `strcpy`
- `sprintf`
- Lecturas sin conocer el tamaño disponible

En su lugar se usan límites como:

- `MAX_REQ_SIZE`
- `MAX_LINE_LEN`
- `MAX_URI_LEN`

Además se utilizan funciones de copia y escritura con tamaño máximo.

Ejemplo conceptual:

```c
comprobar la cantidad disponible
intentar leer solo esa cantidad
aumentar bytes_leidos
terminar el texto
validar los tamaños
```

## Protección contra métodos inválidos

El servidor únicamente acepta `GET`.

Ejemplo:

```c
if método es diferente de GET
    enviar 405 Method Not Allowed
    finalizar o mantener la conexión según corresponda
```

## Límites de solicitudes

El programa revisa:

- Que la línea de solicitud no sea demasiado larga.
- Que la URI no sea demasiado larga.
- Que un encabezado no sea demasiado largo.
- Que la solicitud completa no ocupe demasiado espacio.
- Que existan los delimitadores necesarios.

Si se supera un límite, se devuelve `400 Bad Request`.

## Compilar el proyecto

Ir a la carpeta del proyecto:

```bash
cd MiniHTTPd
```

Compilar:

```bash
make
```

El resultado será un archivo ejecutable llamado:

```text
minihttpd
```

Si se quieren eliminar los archivos objeto y el ejecutable:

```bash
make clean
```

## Ejecutar el servidor

Puerto predeterminado:

```bash
./minihttpd
```

Puerto personalizado:

```bash
./minihttpd 9000
```

El servidor mostrará un mensaje parecido a:

```text
[minihttpd] Servidor epoll escuchando en http://localhost:8080
```

Para detenerlo:

```text
Ctrl + C
```

## Probar desde el navegador

Abrir:

```text
http://localhost:8080
```

Para probar otros archivos:

```text
http://localhost:8080/style.css
http://localhost:8080/image.png
```

## Probar con curl

Probar la página principal:

```bash
curl -i http://localhost:8080/
```

Probar un archivo inexistente:

```bash
curl -i http://localhost:8080/no-existe.html
```

Probar Directory Traversal:

```bash
curl --path-as-is -i \
  http://localhost:8080/../../etc/passwd
```

La respuesta esperada es:

```text
403 Forbidden
```

Probar un método inválido:

```bash
curl -i -X POST http://localhost:8080/
```

La respuesta esperada es:

```text
405 Method Not Allowed
```

Probar encabezados básicos:

```bash
curl -i \
  -H "Host: localhost" \
  -H "Connection: keep-alive" \
  -H "User-Agent: alumno-sistemas" \
  http://localhost:8080/
```

## Pruebas HTTP manuales

Puede abrirse una conexión con netcat:

```bash
nc localhost 8080
```

Y escribir:

```text
GET / HTTP/1.1
Host: localhost
Connection: close

```

Debe enviar un salto de línea después de los encabezados.

## Archivos recomendados en www

Para probar HTML:

```html name=www/index.html
<!doctype html>
<html>
<head>
    <title>MiniHTTPd</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <h1>Servidor MiniHTTPd</h1>
</body>
</html>
```

Para probar CSS:

```css name=www/style.css
body
{
    margin: 40px;
    background: #f3f6f9;
    font-family: Arial, sans-serif;
}

h1
{
    color: #244a73;
}
```

## Posibles problemas

### Puerto ocupado

Si el puerto está ocupado:

```bash
./minihttpd 9000
```

También se puede revisar el puerto con:

```bash
sudo lsof -i :8080
```

### No encuentra archivos

Ejecutar el servidor desde el directorio del proyecto:

```bash
cd MiniHTTPd
./minihttpd
```

El directorio `www` se busca desde ese lugar.

### No funciona desde otro computador

El servidor puede estar limitado a la red local. Hay que comprobar:

- Firewall.
- Dirección IP.
- Puerto utilizado.
- Que `AI_PASSIVE` permita recibir conexiones.

### Error al compilar

Comprobar que estén instalados:

```bash
sudo apt update
sudo apt install build-essential
```

## Resumen

MiniHTTPd está dividido en módulos:

- `main.c`: inicia el programa.
- `server.c`: administra sockets y epoll.
- `http.c`: interpreta solicitudes HTTP.
- `files.c`: abre y valida archivos.
- `mime.c`: determina el tipo de contenido.

El funcionamiento general es:

```text
Recibir solicitud
      |
      v
Validar formato
      |
      v
Aceptar solamente GET
      |
      v
Comprobar ruta segura
      |
      v
Encontrar archivo
      |
      v
Enviar respuesta HTTP
```
Autor: Aubertin Ochoa
