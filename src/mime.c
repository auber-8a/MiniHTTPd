#include <string.h>
#include <strings.h>
#include "mime.h"

const char *obtener_tipo_mime(const char *ruta_archivo) {
    const char *ext = strrchr(ruta_archivo, '.');
    if (!ext) return "application/octet-stream";

    if (strcasecmp(ext, ".html") == 0) return "text/html";
    if (strcasecmp(ext, ".css") == 0)  return "text/css";
    if (strcasecmp(ext, ".js") == 0)   return "application/javascript";
    if (strcasecmp(ext, ".png") == 0)  return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    }

    return "application/octet-stream";
}