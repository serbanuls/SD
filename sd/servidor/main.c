//  Proyecto Servidor HTTP para Sistemas Distribuidos
//	Compilación: gcc main.c -o mi_httpd -D DEBUG

#include <stdio.h>	
#include <errno.h>  
#include <stdlib.h>	
#include <string.h>	
#include <ctype.h>	
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

#define SERWEB_VERSION_NAME                 "beta"

#define DEFAULT_PORT                        8080
#define DEFAULT_ROOT_DIRECTORY              "./html"
#define DEFAULT_DIRECTORY_INDEX             "index.html"
#define DEFAULT_MAX_CLIENTS                 15

#define HTTP_BAD_REQUEST    "HTTP/1.1 400 Bad Request\r\nContent-Length: 206\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>\n<head>\n<title>400 Bad Request. Que lo sepas</title>\n</head>\n<body>\n<h1>Bad Request</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body>\n</html>\n"

#define MAX_REQUEST_SIZE                    4048
#define MAX_PATH_SIZE                       256
#define BUFSIZE                             8096

#ifdef DEBUG
#define log(fmt, args...)   fprintf(stderr, fmt "\n", ## args)
#else
#define log(fmt, args...)
#endif


int AtenderProtocoloHTTP(int sd)
{
    char buffer[MAX_REQUEST_SIZE] = "\0";
    long bLeidos = 0;
    int  result;

    log("atendiendo la solicitud HTTP...");

    // HTTP REQUEST: leemos la cabecera (debe ser siempre menor que MAX_REQUEST_SIZE)
    bLeidos = read(sd, buffer, sizeof(buffer)-1);
    if (bLeidos<0) {
        fprintf(stderr, "ERROR: [%s] leyendo del socket\n", strerror(errno));
        return -1;
    }
    buffer[bLeidos] = '\0';
    log("Recibidos %ld bytes\n%s", bLeidos, buffer);

    // Tratamiento de la solicitud
    // aquí analizaríamos la solicitud

    // HTTP RESPONSE: respondemos con un mensaje
    log("Enviando request (%ld bytes):\n%s", strlen(HTTP_BAD_REQUEST), HTTP_BAD_REQUEST);
    result = write(sd, HTTP_BAD_REQUEST, strlen(HTTP_BAD_REQUEST));

    return result;
}   /* de AtenderProtocoloHTTP */


int ObtenerSocket(int puerto)
{
    int                 sfdServidor = -1;   // socket para esperar peticiones
    struct sockaddr_in  addr;
    int                 error;

    // SOCKET: Abrimos un socket donde esperar peticiones de servicio
    log("SOCKET: abriendo un socket...");
    sfdServidor = socket(AF_INET, SOCK_STREAM, 0);
    if(sfdServidor==-1) {
        fprintf(stderr, "ERROR [%s] abriendo socket\n", strerror(errno));
        return -1;
    }

    // BIND: Asociamos el puerto en el que deseamos esperar con el socket obtenido
    log("BIND: asociando el socket con el puerto de escucha...");
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons((u_short)puerto);

    error = bind(sfdServidor, (struct sockaddr *)&addr, sizeof(addr));
    if (error < 0) {
        fprintf(stderr, "ERROR: [%s] en bind\n", strerror(errno));
        close(sfdServidor);
        return -1;
    }

    // LISTEN: Nos ponemos a la escucha en el socket, indicando cuántas solicitudes atendemos a la vez
    log("LISTEN: indicamos que atienda hasta %d conexiones simultáneas", DEFAULT_MAX_CLIENTS);
    error = listen(sfdServidor, DEFAULT_MAX_CLIENTS);
    if (error < 0) {
        fprintf(stderr, "ERROR: [%s] en listen\n", strerror(errno));
        close(sfdServidor);
        return -1;
    }

    return sfdServidor;
}   // de ObtenerSocket


int ConectarConCliente(int sfdServidor)
{
    int                 sfdCliente = -1;    // socket del hijo. Atiende cada nueva petición
    struct sockaddr_in  cliente;
           socklen_t    tamCliente = sizeof (cliente);
    struct hostent     *host;

    // ACCEPT: Esperamos hasta que llegue una comunicación, para la que nos dan un nuevo socket
    log("ACCEPT: esperrando neuva conexión...");
    sfdCliente = accept(sfdServidor, (struct sockaddr *)&cliente, &tamCliente);

    log("Comprobando conexión entrante... (resolución inversa del dns)");
    host = gethostbyaddr((char *)&cliente.sin_addr, sizeof(cliente.sin_addr), cliente.sin_family);
    log("Conexión aceptada a [%s] %s:%u", host->h_name, inet_ntoa(cliente.sin_addr), cliente.sin_port);    

    return sfdCliente;
}   // de ConectarConCliente


void LanzarServicio(int puerto)
{
    int sfdServidor = -1;   // socket del padre. Escucha nuevas peticiones
    int sfdCliente  = -1;   // socket del hijo. Atiende cada nueva petición
    int result;

    sfdServidor = ObtenerSocket(puerto);
    if (sfdServidor == -1) {
        fprintf(stderr, "ERROR: no se ha podido establecer la conexión de red\n");
        exit(-1);
    }

    while (1) {
        sfdCliente = ConectarConCliente(sfdServidor);
        if (sfdCliente == -1) {
            fprintf(stderr, "ERROR: no se ha podido conectar con el cliente\n");
            exit(-1);
        }

        switch (fork()) {
            case -1:    // Error
                fprintf(stderr, "ERROR : [%s] ejecutando fork\n", strerror(errno));
                close(sfdCliente);
                close(sfdServidor);
                exit (-1);

            case  0:    // El hijo atiende la nueva solicitud
                log("PROCESO HIJO %d: atendiendo protocolo de aplicación en socket %d", (int)getpid(), sfdCliente);
                close(sfdServidor);
                result = AtenderProtocoloHTTP(sfdCliente);      // Atendemos el PROTOCOLO concreto
                close(sfdCliente);                              
                log("PROCESO HIJO %d: terminando proceso", (int)getpid());
                exit (result);

            default:    // El padre se queda a la espera de nuevas conexiones
                log("PROCESO PADRE %d: buscando una nueva conexión", (int)getpid());
                close(sfdCliente);
        }
    }   /* de while infinito */

}   // de LanzarServicio


int main()
{
    // Mostrando información de configuración
    log("CONFIG: Puerto de escucha: %d", DEFAULT_PORT);
    log("CONFIG: Max Clientes: %d", DEFAULT_MAX_CLIENTS);
    log("CONFIG: Recurso por defecto: %s", DEFAULT_DIRECTORY_INDEX);
    log("CONFIG: Directorio Raíz: %s", DEFAULT_ROOT_DIRECTORY);

    // Lanzando el servicio que atiende el protocolo (puerto indicado)
    log("Lanzamos servicio en puerto %d", DEFAULT_PORT);
    LanzarServicio(DEFAULT_PORT);

    // En realidad nunca llegaremos aquí
    return 0;
}	/* de main */

