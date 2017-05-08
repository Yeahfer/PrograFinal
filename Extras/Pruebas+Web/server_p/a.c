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
#include "listaMimeTypes.h"
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

static void parse_message(char *message);
static void readline(char *message, char *line, int *counter);

char *recuperarMimeType(char *extension){
	int i,j,x;
	char buffTipoMime[100];

    for(i=0;i<83;i++){
    	printf("indice %d tipoMime %s\n", i,mapaMimeTypes[i]);
    	if(strncmp(mapaMimeTypes[i],extension,strlen(extension))==0){
    		strncpy(buffTipoMime,mapaMimeTypes[i+1],strlen(mapaMimeTypes[i+1]));
    		break;
    	}
    }    
    return buffTipoMime;
}


int main(int argc, char *argv[]){

	char *buff="HEAD /asdf.asfa?asdf=asdf&asdf=asdf asdf";
	char buff_aux[2048];
    strncpy(buff_aux,buff,2048);

    char *token_header;
    //primer token=>GET 
    token_header = strtok(buff_aux," ");

    char * tipo_metodo=token_header;
    
    //segundo token=>URI PETICION
    token_header = strtok(NULL," ");    

    char *uri=token_header;

    printf("TOKEN HEADER: %s\n",uri);

    int metodo=0;

    //Si el uri de peticion contiene '?' debemos usar cgi para
    //procesar el
    if(strstr(uri, "?")>0){
        if(strcmp(tipo_metodo,"GET")==0){
            metodo=1;
        }else{
            if(strcmp(tipo_metodo,"POST")==0){
                metodo=2;
            }else{  
                metodo=3;
            }
        }
        printf("METODO %s\n",tipo_metodo);
        printf("MODO CGI: %d\n",metodo);
    }

    int x=execlp("php","php", "sheller.php", (char *)NULL);
    if(x<0){
        perror("execlp");
    }

    
    
    // char nombre_archivo_uri[500];

    // printf("TOKEN HEADER: %s LEN: %lu\n",token_header,strlen(token_header));

 //    if(strncmp(token_header,"/",strlen(token_header))==0){
 //        strncpy(nombre_archivo_uri, "/index.html", 12);
 //        nombre_archivo_uri[11]='\0';
 //    }else{
 //        strncpy(nombre_archivo_uri, token_header, strlen(token_header)+1);
 //        nombre_archivo_uri[strlen(token_header)]='\0';
 //    }

 //    // char ext_archivo[strlen(nombre_archivo_uri)];
 //    // strncpy(ext_archivo,nombre_archivo_uri,strlen(nombre_archivo_uri));
 //    char *token_extension;
 //    //primer token=>path del archivo
 //    token_extension = strtok(nombre_archivo_uri,".");
 //    //segundo token=>extension del archivo
 //    token_extension = strtok(NULL,".");

 //    char* tipoMime=recuperarMimeType(token_extension);

 //    printf("ARCHIVO URI: %s\n", nombre_archivo_uri);
 //    printf("EXTENSION ARCHIVO: %s\n", token_extension);
	// printf("TIPO MIME: %s\n", tipoMime);


    //test recuperar y calcular tamano de archivo
    // FILE *da;
    // int tamano;

  //   char *url_archivo="/hame/ec2-user/var/www/html";

  //   char url_completo[1024];
  //   strcat(url_completo,url_archivo);
  //   strcat(url_completo,nombre_archivo_uri);
  //   strcat(url_completo,".");
  //   strcat(url_completo,token_extension);

  //   printf("URL: %s\n",url_completo);
  //   printf("URL Archivo: %s\n",url_archivo);

  //   da=fopen(url_completo, "r");
  //   if(da==NULL){
  //       //mandar 404 aqui...
  //       printf("No existe tal archivo!!\n");
  //       exit(1);
  //   }else{
		// printf("SI EXISTE EL ARCHIVO YAY!!!\n");

		// openlog("logServidorPrueba", LOG_PID|LOG_CONS, LOG_USER);
  //       syslog(LOG_INFO, "Header de la peticion: %s\n","SI EXISTE EL ARCHIVO YAY!!!");
  //       closelog();
    // }
    
	return 0;
}


static void parse_message(char *message) {
    char *line = (char *) malloc (50 * sizeof(char));
    int counter = 0;
    char *startline = "Chat {";
    if(strncmp(message, startline, strlen(startline)) == 0){
        readline(message, line, &counter); //read in message
        printf("%s",message); //print message
        readline(message,line,&counter); //read in close bracket
    } else{
        printf("Error occurred\n");
    }
}


static void readline(char *message, char *line, int *counter){
    int index = 0;
    while(message[*counter] != '\n'){
        line[index] = message[*counter];
        (*counter)++;
        index ++;
    }
    line[index] = 0;
}