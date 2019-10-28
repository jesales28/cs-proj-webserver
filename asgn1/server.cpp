// httpserver.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple http server

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>

int main (int argc, char *argv[]) {
    int PORT_NUMBER = 8080;
    char SERVER_NAME_STRING[] = "localhost";
    // Status codes and messages
    char *ok = "HTTP/1.1 200 OK\n";
    char *created = "HTTP/1.1 201 Created\n";
    char *bad_request = "HTTP/1.1 400 Bad Request\n";
    char *forbidden = "HTTP/1.1 403 Forbidden\n";
    char *file_not_found = "HTTP/1.1 404 Not Found\n";
    char *internal_server_error = "HTTP/1.1 500 Internal Server Error\n";

    // Create sockaddr_in
    struct hostent *hent = gethostbyname(SERVER_NAME_STRING);
    struct sockaddr_in addr;
    memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
    addr.sin_port = htons(PORT_NUMBER);
    addr.sin_family = AF_INET;

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == 0) {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    // Create socket for server
    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))<0) {
        perror("In bind");
        exit(EXIT_FAILURE);
    }
    // Listen to socket to see if there is a connection
    listen(sock, 0);

    const int buf_size = 32768;
    while (1){
        int cl = accept(sock, NULL, NULL);
        if (cl<0) {
            perror("In accept");
            exit(EXIT_FAILURE);
        }
        printf("------------------started server-------------------\n");
        char buffer[buf_size] = {0};
        char get_buff[buf_size];
        char file_buff[buf_size];
        char header1[3];            // contains request
        char header2[28];           // contains target
        char header3[15];           // contains HTTP/1.1
        char get[] = "GET";
        char put[] = "PUT";
        char slash[] = "/";
        int content_len;
        // Read to get header
        int valread = read(cl, buffer, buf_size);
    
        // Parse header
        sscanf(buffer, "%s %s %s", header1, header2, header3);   
        if (*header1 == *get) {
            int file_fetch;
            int file = open(header2, O_RDONLY);
            // If "/" then curl is called without target
            // Print 400 message because there is no target
            if (*header2 == *slash){
                write(cl, bad_request, strlen(bad_request));
                sprintf(get_buff, "Content-length: 0\n");
                write(cl, get_buff, strlen(get_buff));
            }
            // If file is not found, print 404 error message
            else if (file == -1){
                write(cl, file_not_found, strlen(file_not_found));
                sprintf(get_buff, "Content-length: 0\n");
                write(cl, get_buff, strlen(get_buff));
            }
            // If file is found, print 200 message
            else if (file != -1) {
                file_fetch = read(file, file_buff, buf_size);
                struct stat st;
                stat(header2, &st);
                content_len = st.st_size;
                write(cl, ok, strlen(ok));
                sprintf(get_buff, "Content-length: %d\n", content_len);
                write(cl, get_buff, strlen(get_buff));
                // If handling a large file
                while (file_fetch >= buf_size) {
                    write(cl, file_buff, file_fetch);
                    file_fetch = read(file, file_buff, file_fetch);
                }
                write(cl, file_buff, file_fetch);
            }
            else {
                write(cl, internal_server_error, strlen(internal_server_error));
                sprintf(get_buff, "Content-length: 0\n");
                write(cl, get_buff, strlen(get_buff));
            }
            close(file);
            close(file_fetch);
        }
        if (*header1 == *put) {
            //write(cl, buffer, strlen(buffer));
            sprintf(buffer, "got PUT\n");
            // writes out header to client
            write(cl , buffer , strlen(buffer));
        }

        printf("---------------------message sent----------------------\n");
        close(cl);
    }

    return 0;

}