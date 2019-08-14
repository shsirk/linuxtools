/*
    gdb (edit) trick for working with large code base and vanila gdb.
    Opens visual studio code or reuse existing instance to open source file. 
    krishs.patil@gmail.com (twitter @shsirk)

    gcc -o editor editor.c
    $EDITOR=./editor gdb /path/to/program
    $EDITOR=./editor SRC_SRV_HOST=my.remote.host.ip SRC_SRV_PORT=port gdb /path/to/program
    
    I'm fond of visual studio code for C/C++ developement but not happy with gdb integration inside it. 
    when debugging big code bases, I want to keep codebase open in VS code and debug compiled binary in gdb. 
    This help me to take advantage of edit command to open source code in any external EDITOR and go to specific line 
    Why VSCode? - allows cross-ref symbols easy without any hassel  
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char** argv, char** env)
{
    if (argc != 3)
        return 1;

    int sockfd; 
    struct sockaddr_in servaddr; 

    char fileline[1024] = {0};
    snprintf(fileline, sizeof(fileline)-1, "%s:%s", argv[2], ++argv[1]);

    char *host = getenv("SRC_SRV_HOST"), 
         *port = getenv("SRC_SRV_PORT");
    if (host && port) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0); 
        bzero(&servaddr, sizeof(struct sockaddr_in));
        servaddr.sin_family = AF_INET; 
        servaddr.sin_addr.s_addr = inet_addr(host); 
        servaddr.sin_port = htons(atoi(port)); 
        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) { 
            printf("connection with the server failed.\n"); 
            return 1; 
        }
        write(sockfd, fileline, strlen(fileline));
        close(sockfd);
    } else {
        char *args[] = {
            "/usr/bin/code",
            "-r",
            "-g",
            fileline,
            0,
        };
        execve(args[0], args, env);
    }

    return 0;
}
