// httpserver.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple http server
// RESOURCES: piazza, stack overflow, class, man pages, and logic from asgn0
// as well as referenced medium.com <- helped with setting up the socket
// it was also similar/same code from section.

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
    int PORT_NUMBER;
    char *SERVER_NAME_STRING;
    if (argv[1] == NULL) {
        fprintf(stderr, "Request is missing required `Host` header\n");
        exit (1);
    }
    else {
        SERVER_NAME_STRING = argv[1];
    }
    if(argv[2] == NULL){
        fprintf(stderr, "Request is missing required `Port` header\n");
        exit(1);
    }
    else{
        PORT_NUMBER = atoi(argv[2]);
    }
    // Status codes and messages
    const char *ok = 
        "HTTP/1.1 200 OK\nContent-length: 0\r\n\r\n";
    const char *created = 
        "HTTP/1.1 201 Created\nContent-length: 0\r\n\r\n";
    const char *bad_request = 
        "HTTP/1.1 400 Bad Request\nContent-length: 0\r\n\r\n";
    const char *forbidden = 
        "HTTP/1.1 403 Forbidden\nContent-length: 0\r\n\r\n";
    const char *file_not_found = 
        "HTTP/1.1 404 Not Found\nContent-length: 0\r\n\r\n";
    const char *internal_server_error = 
        "HTTP/1.1 500 Internal Server Error\nContent-length: 0\r\n";
    const char *put_continue = 
        "HTTP/1.1 100 Continue\n\n";

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
        char buffer[buf_size] = {0};
        char get_buff[buf_size];
        char file_buff[buf_size];
        char header1[3];            // contains request
        char header2[28];           // contains target
        char header3[15];           // contains HTTP/1.1
        char header4[64];           // contains Content-length line
        char get[] = "GET";
        char put[] = "PUT";
        char slash[] = "/";
        int content_len = 0;
        // Read to get header
        int valread = read(cl, buffer, buf_size);
    
        // Parse header
        sscanf(buffer, "%s %s %s", header1, header2, header3);
        char * line = strtok(strdup(buffer), "\n");
        int first_line = 1;
        while (line != NULL) {
            // check for GET or PUT if first line in buffer 
            if (first_line == 1) {
                sscanf(line, "%s %s %s", header1, header2, header3);
                first_line = 0;
            }
            if (strncmp(line, "Content-Length:", 15) == 0) {
                sscanf(line, "%s %d", header4, &content_len);
            } 
            // add other parsers here
            line = strtok(NULL, "\n");
        }
        // GET
        if (*header1 == *get) {
            // If "/" then curl is called without target
            // Print 403 message because "/" are not allowed
            if (strstr(header2, slash) != NULL){
                write(cl, forbidden, strlen(forbidden));
                sprintf(get_buff, "Content-length: 0\r\n");
                write(cl, get_buff, strlen(get_buff));
            }
            // If target file name if not 27 ascii characters
            // Print 400 error message
            else if (strlen(&header2[0]) != 27){
                write(cl, bad_request, strlen(bad_request));
            }
            else {
                // If file is not found, print 404 error message
                int file_fetch;
                int file = open(header2, O_RDONLY);
                if (file == -1){
                    write(cl, file_not_found, strlen(file_not_found));
                }
                // If file is found, print 200 message
                else if (file != -1) {
                    file_fetch = read(file, file_buff, buf_size);
                    struct stat st;
                    stat(header2, &st);
                    content_len = st.st_size;
                    sprintf(get_buff, 
                        "HTTP/1.1 200 OK\nContent-length: %d\r\n\r\n",
                        content_len);
                    write(cl, get_buff, strlen(get_buff));
                    // If handling a large file
                    while (file_fetch >= buf_size) {
                        write(cl, file_buff, file_fetch);
                        file_fetch = read(file, file_buff, file_fetch);
                    }
                    write(cl, file_buff, file_fetch);
                }
                else {
                    write(cl, &internal_server_error, 
                        strlen(internal_server_error));
                }
                close(file);
                close(file_fetch);
            }
        }
        // PUT
        if (*header1 == *put) {
            // If "/" then curl is called without target
            // Print 403 message because "/" are not allowed
            if (strstr(header2, slash) != NULL){
                write(cl, forbidden, strlen(forbidden));
            }
            // If target file name if not 27 ascii characters
            // Print 400 error message
            else if (strlen(&header2[1]) != 26){
                write(cl, bad_request, strlen(bad_request));
            }
            else {
                int counter, putread;
                int fd = open(header2, O_RDWR | O_TRUNC, 0664);
                // If file does not exist, create it and print 201 message
                if (fd == -1) {
                    close(fd);
                    fd = open(header2, O_RDWR | O_CREAT | O_TRUNC, 0664);
                    write(cl , put_continue , strlen(put_continue));
                    putread = read(cl, file_buff, buf_size);
                    counter = content_len;
                    // Handling large files
                    if(counter >= buf_size){
                        write(fd,file_buff, putread);
                        while(counter >= buf_size) {
                            write(fd, file_buff, putread);
                            counter = counter - buf_size;
                            putread = read(cl, file_buff, putread);
                        }
                    }
                    write(fd,file_buff, putread);
                    write(cl, created, strlen(created));
                    close(fd);
                }
                // If file exists, over write it and print 200 message
                else if (fd != -1) {
                    close(fd);
                    fd = open(header2, O_RDWR | O_CREAT | O_TRUNC, 0664);
                    write(cl , put_continue , strlen(put_continue));
                    putread = read(cl, file_buff, buf_size);
                    counter = content_len;
                    // deals with large files
                    if(counter >= buf_size){
                        write(fd,file_buff, putread);
                        while(counter >= buf_size) {
                            write(fd, file_buff, putread);
                            counter = counter - buf_size;
                            putread = read(cl, file_buff, putread);
                        }
                    }
                    write(fd,file_buff, putread);
                    write(cl, ok, strlen(ok));
                    close(fd);
                }
                else {
                    write(cl, &internal_server_error, 
                        strlen(internal_server_error));
                }
            }
        }
        close(cl);
    }
    return 0;
}