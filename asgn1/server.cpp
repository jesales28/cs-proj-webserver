// httpserver.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple http server
// RESOURCES: piazza, stack overflow, class, man pages, and logic from asgn0

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
    const char *ok = "HTTP/1.1 200 OK\n";
    const char *created = "HTTP/1.1 201 Created\n";
    const char *bad_request = "HTTP/1.1 400 Bad request\n";
    const char *forbidden = "HTTP/1.1 403 Forbidden\n";
    const char *file_not_found = "HTTP/1.1 404 Not Found\n";
    const char *internal_server_error = "HTTP/1.1 500 Internal Server Error\n";
    const char *put_continue = "HTTP/1.1 100 Continue\n\n";

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
    //printf("------------------started server-------------------\n");
    while (1){
        //printf("------------------waiting for connection-------------------\n");
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
            //printf("DEBUG line: %s\n", line);
            // check for GET or PUT if first line in buffer 
            if (first_line == 1) {
                sscanf(line, "%s %s %s", header1, header2, header3);
                //printf("DEBUG: 1:%s 2:%s 3:%s\n", header1, header2, header3);
                first_line = 0;
            }
            if (strncmp(line, "Content-Length:", 15) == 0) {
                sscanf(line, "%s %d", header4, &content_len);
                //printf("line: %s %d\n", header4, content_len);
            } 
            // add other parsers here
            line = strtok(NULL, "\n");
        }

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
                write(cl, &internal_server_error, strlen(internal_server_error));
                sprintf(get_buff, "Content-length: 0\n");
                write(cl, get_buff, strlen(get_buff));
            }
            close(file);
            close(file_fetch);
        }

        if (*header1 == *put) {
            // If "/" then curl is called without target
            // Print 400 message because there is no target
            if (*header2 == *slash){
                //printf("header2: %s\n", header2);
                write(cl, &forbidden, strlen(forbidden));
                sprintf(get_buff, "Content-length: 0\n");
                write(cl, get_buff, strlen(get_buff));
            }
            // If file does not exist, print 403 message
            else {
                int counter, putread;
                int fd = open(header2, O_RDWR | O_TRUNC, 0664);
                // If file does not exist, create it and print 201 message
                if (fd == -1) {
                    close(fd);
                    fd = open(header2, O_RDWR | O_CREAT | O_TRUNC, 0664);
                    write(cl , put_continue , strlen(put_continue));
                    putread = read(cl, file_buff, content_len);
                    counter = content_len;
                     while(counter >= buf_size) {
                            write(fd, file_buff, putread);
                            putread = read(cl, file_buff, putread);
                            counter = counter - buf_size;
                        }
                    write(fd,file_buff, putread);
                    write(cl, created, strlen(created));
                    sprintf(get_buff, "Content-length: 0\n");
                    write(cl, get_buff, strlen(get_buff));
                    close(fd);
                    // fail to open file for creation
                    /*write(cl, bad_request, strlen(bad_request));
                    sprintf(get_buff, "Content-length: 0\n");
                    write(cl, get_buff, strlen(get_buff));
                    close(fd);*/
                }
                // If file exists, over write it and print 200 message
                else {
                    //close(fd);
                    //int fd = open(header2, O_RDWR | O_CREAT | O_TRUNC, 0664);
                    int write_to_file;
                    //printf("new file\n");
                    write(cl , put_continue , strlen(put_continue));
                    putread = read(cl, file_buff, buf_size);
                    counter = content_len;
                        //printf("DEBUG: file_buff = %s\n", file_buff);
                    while(counter >= buf_size) {
                        write(fd, file_buff, putread);
                        putread = read(cl, file_buff, putread);
                        counter = counter - buf_size;
                        //printf("DEBUG: file_buff = %s\n", file_buff);
                        //printf("loop\n");
                        //printf("putread: %d\n",putread);
                        //printf("counter: %d\n", counter);
                    } 
                    write(fd,file_buff, putread);
                    //printf("got to last write\n");
                    write(cl, ok, strlen(ok));
                    sprintf(get_buff, "Content-length: 0\n");
                    write(cl, get_buff, strlen(get_buff));
                    //printf("create message done\n");
                    close(fd);
                }
            }
        }

        //printf("---------------------message sent----------------------\n");
        close(cl);
    }

    return 0;
}