#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define SIZE 32
#define PORT 80

int main() {
    int sd, size;
    struct addrinfo hints, *res;
    char buffer[SIZE];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo("127.0.0.1", "80", &hints, &res);
    
    sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    connect(sd, res->ai_addr, res->ai_addrlen);
    //while( (size=read(STDIN_FILENO, buffer, SIZE)) > 0) {
    //    write(sd, buffer, size);
    //}

    while( (size=read(sd, buffer, SIZE)) > 0) {
        write(1, buffer, size);
        printf("\n");
    }

    close(sd);
    freeaddrinfo(res);
    sleep(1);
}

