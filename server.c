
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "listaTiposArchivo.h"
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#define PORT 80
#define SIZE 8
#define MSGSIZE 1024
#define MAX_CONEXIONES 200

int sdo;

char buffFecha[1000];

void createDae(){
    pid_t daemon_process;

    daemon_process= fork();

//eliminar a los procesos padre
    if(daemon_process > 0){
        exit(0);
    }

//ignorar hijos
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    daemon_process= fork();

    if(daemon_process > 0){
        exit(0);
    }

//cambiar a root
    umask(0);

    int desc;
    //SC_OPEN_MAX es el limite de descriptores de archivo
    //que un proceso puede tener abiertos al mismo tiempo, en
    //este caso stdin, stdout y stderr
    for (desc = sysconf(_SC_OPEN_MAX); desc>0; desc--){
        close(desc);
    }

//mandar al log
    openlog ("CreacionDeDemonio", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "El proceso fue demonizado\n");
    closelog();
}

//metodo para calcular la fecha y hora actuales y mandarlas en el header de respuesta
void calcularFecha(){
    time_t esteInstante = time(0);
    struct tm tm = *gmtime(&esteInstante);
    strftime(buffFecha, sizeof buffFecha, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

//guardar en el log cuando el servidor se cae por x razon
void servidorCayo(){
    openlog("ServidorMurio", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "El servidor fue apagado o termino su proceso de forma inesperada...\n");
    closelog();
}

//recuperar el mime type del archivo en funcion de su extension
char *recuperarMimeType(char *extension){
    int i,j,x;
    char buffTipoMime[100];
    //recorre un simple arreglo con extensiones y tipos mime
    for(i=0;i<83;i++){
        if(strncmp(mapaMimeTypes[i],extension,strlen(extension))==0){
            strncpy(buffTipoMime,mapaMimeTypes[i+1],strlen(mapaMimeTypes[i+1]));
            break;
        }
    }
    return buffTipoMime;
}

int contSaltos=0;

char residuos[1024];
int longitudPost=0;

int banderaUbicacionContentLength=0;
int procesamientoPostTerminado=0;
int esPost=0;

int readLine(int s, char *line, int *result_size) {

    int acum=0, size;
    char buffer[SIZE];

    char buffer_linea[1024];

    while( (size=read(s, buffer, SIZE)) > 0) {
        if (size < 0) return -1;
        strncpy(line+acum, buffer, size);
        strncat(residuos,buffer,8);
        // printf("BUFFER: %s\n",buffer);
         printf("RESIDUOS: %s\n",residuos);

        if(strstr(buffer,"POST")>0){
            esPost=1;
            // printf("ES POST DESDE READLINE!!!\n");
        }else{
            if(strstr(buffer,"GET")>0){
                esPost=0;
            }
        }
        strcpy(buffer_linea,line);
        // printf("LINEA BUFFER: %s\n",buffer_linea);

        //Sacer el content length.....
        if(!banderaUbicacionContentLength){
            //ya que aparezca el numero completo (Cache-control es la linea que sigue)
            //de content length
            if(strstr(buffer_linea,"Cache-Control")>0){
                //buscar donde esta el content=length
                if(strstr(buffer_linea,"Length")>0){
                    char * aux;
                    //proceso tedioso para obtener content length para leer tantos bytes
                    //como sean necesarios al final del proceso
                    aux=strstr(buffer_linea,"Content-Length");
                    int posicion_substring=aux-buffer_linea;
                    char *xgh=buffer_linea+posicion_substring;
                    char *token_pos1;
                    char *token_pos2;
                    char *token_pos3;
                    token_pos1=strtok(xgh," ");
                    token_pos1=strtok(NULL," ");
                    token_pos2=strtok(token_pos1,"C");
                    char buff[50];
                    strncpy(buff,token_pos2,strlen(token_pos2)-2);
                    sscanf(buff, "%d", &longitudPost);
                }
                banderaUbicacionContentLength=1;
            }
        }

        acum += size;
        if(!esPost){
            if(line[acum-1] == '\n' && line[acum-2] == '\r' ) {
                break;
            }
        }else{
            //cuando hay una doble linea significa que abajo viene el body de todo el documento
            if(line[acum-1] == '\n' && line[acum-2] == '\r' && line[acum-3] == '\n' && line[acum-4] == '\r'){

                if(read(s, buffer, longitudPost)<0){
                    perror("read");
                }

                //funciono!!! para obtener todo el documento en POST
                printf("POR FAVAR FUNCIONA!: %s\n",buffer);
                memset(&residuos, 0, sizeof(residuos));
                strncpy(residuos,buffer,longitudPost);
                procesamientoPostTerminado=1;
                break;
            }
        }
    }

    *result_size = acum;
    return 0;
}

int writeLine(int s, char *line, int total_size) {

    int acum = 0, size;
    char buffer[SIZE];

    if(total_size > SIZE) {
        strncpy(buffer, line, SIZE);
        size = SIZE;
    } else  {
        strncpy(buffer, line, total_size);
        size = total_size;
    }

    while( (size = write(s, buffer, size)) > 0) {
        if(size<0){
            return size;
        }
        acum += size;
        if(acum >= total_size){
            break;
        }

        size = ((total_size-acum)>=SIZE)?SIZE:(total_size-acum)%SIZE;
        strncpy(buffer, line+acum, size);
    }
    return 0;
}

int serve(int s) {
    char command[MSGSIZE];
    int size, r, nlc = 0;
    char *archivo_peticion;
    //buffer masivo para ir guardando los comandos
    char buff[8192];

    char query[512];

    int esPost=0;
    int contadorLineaVaciaParaElPost=0;

    while(1) {
        r = readLine(s, command, &size);
         printf("COMMAND COMPLETO: %s\n",command);

        command[size-2] = 0;
        size-=2;

        //Guarda toda la informacion de las peticiones en el log ubicado en /var/log/messages
        openlog("Peticiones_al_servidor", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Header de la peticion: %s\n",command);
        closelog();

        printf("[%s]\n", command);

        char buff_query[512];

        if(strstr(command,"POST")>0){
            esPost=1;
            printf("ES POST desde serve!!!\n");
        }

        //Guardar todos los comandos para su manipulacion posterior
        strcat(buff,command);
        strcat(buff,"\n");

        if(!esPost){
            if(command[size-1] == '\n' && command[size-2] == '\r') {
                break;
            }
        }else{
            //para saber cuando se termino de leer el documento en el post
            if(procesamientoPostTerminado){
                break;
                procesamientoPostTerminado=0;
            }
        }

    }

    char buff_aux[8192];
    strncpy(buff_aux,buff,8192);

    char *token_header;

    //primer token=>TIPO DE ACCION (GET, POST, ETC...
    token_header = strtok(buff_aux," ");
    printf("TOKEN HEADER: %s\n", token_header);

    char *tipo_metodo = token_header;

    //segundo token=>URI PETICION
    token_header = strtok(NULL," ");

    char *uri = token_header;

    int metodo=0;

    //Si el uri de peticion contiene '?' o es de tipo POST
    //debemos usar cgi para procesar los datos de la forma
    //de lo contrario el metodo==0 indica servir un archivo estatico

    if(strcmp(tipo_metodo,"POST")==0){
        metodo=2;
    }else{
        if(strstr(uri, "?")>0){
            if(strcmp(tipo_metodo,"GET")==0){
                metodo=1;
            }else{
                //para head supongo...
                    metodo=3;
            }
        }
    }

    char nombre_archivo_uri[500];
    if(strncmp(token_header,"/",strlen(token_header))==0){
        strncpy(nombre_archivo_uri, "/saludos.html", 17);
        nombre_archivo_uri[17]='\0';
    }else{
        strncpy(nombre_archivo_uri, token_header, strlen(token_header)+1);
        nombre_archivo_uri[strlen(token_header)]='\0';
    }

    printf("ARCHIVO URI: %s LEN: %lu\n",nombre_archivo_uri,strlen(nombre_archivo_uri));
    printf("para el 403: %s\n",nombre_archivo_uri);
        //ERROR 403
    if(!(strstr(nombre_archivo_uri,"."))){

        int tamano=0;
        printf("intentando acceder a un directorio restringido omg4!");
            //Guardar en el log del sistema cada vez que alguien intento accesar a un directorio exista o no
        openlog("IntentoDeAccesoRestringidoDirectorios", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Alguien intento acceder al recurso: %s\n", nombre_archivo_uri);
        closelog();

        //ec2-user
        //FILE *error=fopen("/home/fernando/var/www/html/errores/error403.html", "r");
        FILE *error=fopen("/root/Desktop/PrograAvanzada/error404.html", "r");

        fseek(error, 0L, SEEK_END);
        tamano = ftell(error);
        fseek(error, 0L, SEEK_SET);

        char *archivo = malloc(tamano+1);
        fread(archivo, tamano, 1, error);
        fclose(error);

        //Mandar una respuesta con header 403, es un directorio por lo tanto esta prohibido
        sprintf(command, "HTTP/1.0 403 ACCESS FORBIDDEN\r\n");
        writeLine(s, command, strlen(command));
        calcularFecha();
        sprintf(command, "Date: %s\r\n",buffFecha);
        writeLine(s, command, strlen(command));
        sprintf(command, "Content-Type: text/html\r\n");
        writeLine(s, command, strlen(command));
        sprintf(command, "Content-Length: %d\r\n",tamano);
        writeLine(s, command, strlen(command));
        sprintf(command, "\r\n%s",archivo);
        writeLine(s, command, strlen(command));

        printf("No existe tal archivo!!\n");
        free(archivo);

    }else{

        char *token_extension;
        //primer token=>path del archivo
        char nombre_archivo_uri_copia[512];

        strncpy(nombre_archivo_uri_copia,nombre_archivo_uri,512);

        token_extension = strtok(nombre_archivo_uri,".");
        //segundo token=>extension del archivo
        token_extension = strtok(NULL,".");

        char* tipoMime;

        //recuperar el mime type del archivo, si es php
        //se hace un text/html por default para mostrar la respuesta del script
        if(strstr(token_extension,"php")>0) {
            strncpy(token_extension,"php",strlen(token_extension));
            tipoMime="text/html";
        }else{
            tipoMime=recuperarMimeType(token_extension);
        }

        printf("EXTENSION ARCHIVO: %s\n", token_extension);
        printf("TIPO MIME: %s\n", tipoMime);

            //test recuperar y calcular tamano de archivo
        FILE *da;
        int tamano;

        //path por default para la busqueda de archivos
        //misma estructura que en apache lol

        //ec2-user
        //char *url_archivo="/home/fernando/var/www/html";
        char *url_archivo = "/root/Desktop/PrograAvanzada";

        //construccion del path completo al archivo
        //que incluye el path por default
        char url_completo[1024];
        memset(&url_completo, 0, sizeof(url_completo));
        strcat(url_completo,url_archivo);
        strcat(url_completo,nombre_archivo_uri);
        strcat(url_completo,".");
        strcat(url_completo,token_extension);

        printf("URL COMPLETA: %s\n",url_completo);

        char *path_ejecutable= url_completo;

        da=fopen(url_completo, "r");

        if(da == NULL){
        //Guardar en el log del sistema cada vez que alguien intento accesar a un archivo que no existe
            openlog("ErrorArchivoNoEncontrado", LOG_PID | LOG_CONS, LOG_USER);
            syslog(LOG_INFO, "Error: El archivo %s no fue encontrado!\n", url_completo);
            closelog();

            //ec2-user
            //FILE *error=fopen("/home/fernando/var/www/html/errores/error404.html", "r");
            FILE *error = fopen("/root/Desktop/PrograAvanzada/error404.html", "r");

            //tamano del archivo
            fseek(error, 0L, SEEK_END);
            tamano = ftell(error);
            fseek(error, 0L, SEEK_SET);

            //es un archivo muy pequeno, da igual si lo cargamos todo en memoria
            char *archivo = malloc(tamano+1);
            fread(archivo, tamano, 1, error);
            fclose(error);

            //Mandar una respuesta con header 404, archivo no encontrado
            sprintf(command, "HTTP/1.0 404 NOT FOUND\r\n");
            writeLine(s, command, strlen(command));
            calcularFecha();
            sprintf(command, "Date: %s\r\n",buffFecha);
            writeLine(s, command, strlen(command));
            sprintf(command, "Content-Type: text/html\r\n");
            writeLine(s, command, strlen(command));
            sprintf(command, "Content-Length: %d\r\n",tamano);
            writeLine(s, command, strlen(command));
            //lo enviamos todo de jalon, es muy pequeno
            sprintf(command, "\r\n%s",archivo);
            writeLine(s, command, strlen(command));

            printf("No existe tal archivo!!\n");
            free(archivo);

        }else{
            printf("METODO zi: %d\n",metodo);
            if(metodo==0){
                //Si no hay datos que requieran procesamiento, solo regresa un archivo estatico
               printf("SI EXISTE EL ARCHIVO YAY!!!\n");

               //determinar tamano del archivo
               fseek(da, 0L, SEEK_END);
               tamano = ftell(da);
               fseek(da, 0L, SEEK_SET);

               //headers de la respuesta
                sprintf(command, "HTTP/1.0 200 OK\r\n");
                writeLine(s, command, strlen(command));
                calcularFecha();
                sprintf(command, "Date: %s\r\n",buffFecha);
                writeLine(s, command, strlen(command));
                sprintf(command, "Content-Type: %s\r\n",tipoMime);
                writeLine(s, command, strlen(command));
                sprintf(command, "Content-Length: %d\r\n",tamano);
                writeLine(s, command, strlen(command));
                sprintf(command, "\r\n");
                writeLine(s, command, strlen(command));

               char file[tamano];
               int suma=0;
               size=fread(file,1,tamano,da);
               printf("ARCHIVO: %d\n",size);
               //leer el archivo por secciones
               while((size=write(s,&file[suma],MSGSIZE))>0){
                    suma+=size;
                    if(suma>=tamano){
                        break;
                    }
                }
            //si no es un archivo estatico, requiere procesamiento, seleccionar get o post
            }else{

                //para debuggeo nada mas
                if(metodo==1){

                }else{
                    if(metodo==2){
                        printf("RESIUDOS: %s\n",residuos);
                        printf("Longitud: %d\n",longitudPost);
                    }
                }

                //convertir numero a string para el content length del post
                char content_length[100];
                char *ctt="CONTENT_LENGTH=";
                char nums[15];
                snprintf(nums,15,"%d",longitudPost);

                //separar las pipes para entender mejor que
                //esta pasando.. la sintaxis n_pipe[2][2]
                int pipe_salida[2];
                int pipe_entrada[2];
                pipe(pipe_salida);
                pipe(pipe_entrada);

                int i;

                char *token_a;
                char *token_b;

                token_a=strtok(buff," ");
                token_a=strtok(NULL," ");
                token_b=strtok(token_a,"?");
                token_b=strtok(NULL,"?");
                //auxilaires para los tokens del query
                char *xyz=token_b;
                char *zyx="QUERY_STRING=";
                //query string final
                char ggg[2048];
                memset(&ggg, 0, sizeof(ggg));
                strcat(ggg,zyx);

                if(metodo==1){
                    strcat(ggg,xyz);
                }
                if(metodo==2){
                    strcat(ggg,residuos);
                    strcat(content_length,ctt);
                    strcat(content_length,nums);
                }

                printf("GGG: %s\n",ggg);

                if(!fork()) {
                    close(pipe_salida[0]);
                    close(pipe_entrada[1]);

                    dup2(pipe_salida[1], 1);
                    dup2(pipe_entrada[0], 0);

                    //para el get
                    if(metodo==1){
                        putenv("REQUEST_METHOD=GET");
                        putenv(ggg);
                    }
                    //para el post
                    if(metodo==2){
                        putenv("REQUEST_METHOD=POST");
                        putenv(ggg);
                        putenv(content_length);
                    }

                    putenv("REDIRECT_STATUS=True");

                    if(metodo==1){
                        putenv("SCRIPT_FILENAME=test.php");
                    }
                    if(metodo==2){
                        putenv("SCRIPT_FILENAME=test2.php");
                    }

                    //ejecuta el script de php
                    if(execlp("php-cgi", "php-cgi",url_completo, 0) < 0 ){
                        openlog("ErrorEXECLP", LOG_PID | LOG_CONS, LOG_USER);
                        syslog(LOG_INFO, "Error: %s\n", strerror(errno));
                        closelog();
                        perror("execlp");
                    }
                }

                close(pipe_salida[1]);
                close(pipe_entrada[0]);

                //mandarle los datos al script del post
                if(metodo==2){
                    for (i = 0; i < longitudPost; i++){
                        write(pipe_entrada[1], &residuos[i], 1);
                    }
                }

                char c;
                int t=0;
                //leer del pipe de salida del php para pintarlo en pantalla
                char buffx[120000];

                while (read(pipe_salida[0], &c, 1) > 0){
                    buffx[t]=c;
                    t++;
                }

                char buffer[32];
                int size = 0;
                //headers de la respuesta da igual si es post o get
                sprintf(command, "HTTP/1.0 200 OK\r\n");
                writeLine(s, command, strlen(command));
                calcularFecha();
                sprintf(command, "Date: %s\r\n",buffFecha);
                writeLine(s, command, strlen(command));
                sprintf(command, "Content-Type: text/html\r\n");
                writeLine(s, command, strlen(command));
                sprintf(command, "Content-Length: %d\r\n",t-50);
                writeLine(s, command, strlen(command));
                sprintf(command, "\r\n");
                writeLine(s, command, strlen(command));

                //pintar en pantalla lo que proceso el php
                int aux=50;
                while(aux<t){
                    write(s,&buffx[aux],1);
                    aux++;
                }
            }
    }
    fclose(da);
}
return 0;
}

int main(int argc, char **argv) {

    int sd, addrlen, size;
    struct sockaddr_in sin, pin;
    int modo_ejecucion=0;
    int demonizar=0;

    if (argc != 3) {
        fprintf(stderr, "%s <1= multiprocess 2=select> <daemon_process: 1=yes 0=no>\n", argv[0]);
        exit(1);
    }else{
        modo_ejecucion= atoi(argv[1]);
        demonizar= atoi(argv[2]);
    }

    //si la persona indica que quiere demonziar el proceso
    if(demonizar == 1){
        printf("Demonizando...");
        createDae();
    }

        // 1. Crear el socket
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd<0){
        openlog("ErrorCreacionSocket", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Error: %s\n", strerror(errno));
        closelog();
        perror("socket");
    }

    int habilitar = 1;

    //SO_REUSEADOR permite reabrir la conexion inmediatamente despues de mapagar el servidor
    //de lo contrario tendriamos que esperar 20 sec - 2 mins
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &habilitar, sizeof(int)) < 0){
        openlog("ErrorSocketOpciones", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Error: %s\n", strerror(errno));
        closelog();
        perror("setsockopt");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(PORT);

        // 2. Asociar el socket a un IP/puerto
    if(bind(sd, (struct sockaddr *) &sin, sizeof(sin))<0){
        openlog("ErrorEnElBinding", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Error: %s\n", strerror(errno));
        closelog();
        perror("bind");
    }


        // 3. Configurar el backlog
    if(listen(sd, 5)<0){
        openlog("ErrorEnListen", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Error: %s\n", strerror(errno));
        closelog();
        perror("listen");

    }

    if(modo_ejecucion == 1){
        // 4. aceptar conexión
        pid_t pid;
            //al padre no le va a importar que suceda con el hijo mientras
            //este termine, por lo tanto los hijos nunca se transformaran en zombies
        signal(SIGCHLD, SIG_IGN);

        addrlen = sizeof(pin);

        while(1){

            sdo = accept(sd, (struct sockaddr *)  &pin, &addrlen);

            if (sdo == -1) {
                //En coso de que suceda algo raro en el socket y el cliente
                //no pueda conectarse, ingresar el error al log
                openlog("ErrorAceptarConexion", LOG_PID | LOG_CONS, LOG_USER);
                syslog(LOG_INFO, "Error: %s\n", strerror(errno));
                closelog();
                perror("accept");
                exit(1);
            }

            if(!fork()){
                printf("Conectado desde %s\n", inet_ntoa(pin.sin_addr));
                printf("Puerto %d\n", ntohs(pin.sin_port));
                serve(sdo);
                close(sdo);
                exit(0);
            }

            atexit(servidorCayo);
        }

    }else{

        //version de ejecucion del servidor
        //con varios sockets que realizan un multiplexing de
        //las peticiones en 1 solo proceso

        //aqui el socket principal fue inicializado antes de la disyuntiva
        //de ejecuciones
        int sd_hijo;
        int tam;
        struct sockaddr_in dir_cliente;
        char buf[1024];
        int arr_sockets[MAX_CONEXIONES];
        int max_conexiones=MAX_CONEXIONES;
        int cualfueElUltimoSocket;
        int socket_aux, i;

        //inicializar el arreglo donde estaran los sockets
        //que atenderan las siguientes conexiones
        for (i=0;i< MAX_CONEXIONES;i++) {
            arr_sockets[i] = 0;
        }

        //descriptores para el conjutno de sockets
        fd_set descriptor_sockets;
        tam = sizeof(dir_cliente);

        //ciclo principal de ejecucion
        while (1) {
            //creacion del conjunto de sockets
            FD_ZERO(&descriptor_sockets);

            //aqui se agrega el socket principal
            //al conjunto de sockets
            FD_SET(sd, &descriptor_sockets);
            //este representa el socket maximo
            //para llevar la cuenta de las conexiones
            cualfueElUltimoSocket = sd;


            for (i=0;i<MAX_CONEXIONES;i++) {
                socket_aux = arr_sockets[i];

                if(socket_aux > 0){
                    FD_SET( socket_aux , &descriptor_sockets);
                }
                if(socket_aux > cualfueElUltimoSocket){
                    cualfueElUltimoSocket = socket_aux;
                }
            }

            //detecta si hay alguna llamada de clientes, aka cambio de actividad
            //en el socket principal
            if (select(sd + 1, &descriptor_sockets, NULL, NULL, NULL) < 0) {
                openlog("ErrorEnSelect", LOG_PID | LOG_CONS, LOG_USER);
                syslog(LOG_INFO, "Error: %s\n", strerror(errno));
                closelog();
                perror("select");
                exit(1);
            }

            //checar si el nuevo socket se encuentra dentro del arreglo y
            //crear socket que le ayude a manejar la peticion
            if (FD_ISSET(sd, &descriptor_sockets)){

                //creacion de un nuevo socket hijo para recibir llamada de cliente
                sd_hijo = accept(sd, (struct sockaddr *) &dir_cliente, &tam);
                if(sd_hijo < 0){
                    openlog("ErrorEnAcceptSocketSelect", LOG_PID | LOG_CONS, LOG_USER);
                    syslog(LOG_INFO, "Error: %s\n", strerror(errno));
                    closelog();
                    perror("accept");
                    exit(1);
                }

                printf("Conectado desde %s\n", inet_ntoa(dir_cliente.sin_addr));
                printf("Puerto %d\n", ntohs(dir_cliente.sin_port));
                serve(sd_hijo);

                //volver a checar si hubo algun cambio en alguno de los sockets hijos

                for (i=0; i<MAX_CONEXIONES; i++){
                    if(arr_sockets[i]==0){
                        arr_sockets[i]= sd_hijo;
                        break;
                    }
                }
                //falla terriblemente falta la coordinacion entre sockets pero
                //no estoy seguro de su implementacion...lol
            }
        }
    }

    close(sd);

}
