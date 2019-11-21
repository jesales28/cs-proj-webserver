// httpserver.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple http server
// RESOURCES: piazza, stack overflow, class, man pages, and logic from asgn0,
// asgn1, and sandwich.cpp as well as referenced 
// medium.com <- helped with setting up the socket
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
#include <pthread.h>

// global locks

//global variables



int main (int argc, char *argv[]) {
    int PORT_NUMBER, WORKER_THREADS;
    char *SERVER_NAME_STRING, LOG_FILE;
    if (argv[2] == NULL) {
        fprintf(stderr, "Request is missing required `worker thread` header\n");
        exit (1);
    }
    else {
        WORKER_THREADS = *argv[2];
    }
    if (argv[4] == NULL) {
        fprintf(stderr, "Request is missing required `log file` header\n");
        exit (1);
    }
    else {
        LOG_FILE = *argv[4];
    }
    if (argv[5] == NULL) {
        fprintf(stderr, "Request is missing required `Host` header\n");
        exit (1);
    }
    else {
        SERVER_NAME_STRING = argv[5];
    }
    if(argv[6] == NULL){
        fprintf(stderr, "Request is missing required `Port` header\n");
        exit(1);
    }
    else{
        PORT_NUMBER = atoi(argv[6]);
    }

    // Status codes and messages
    char *ok = 
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

    // Create threads
    pthread_t tid[WORKER_THREADS];

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

    //int open_socket = 1;

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
        char put_file[buf_size];
        char header1[3];            // contains request
        char header2[28];           // contains target
        char header3[15];           // contains HTTP/1.1
        char header4[64];           // contains Content-length line
        char header5[64];          // Except: 100-continue
        char get[] = "GET";
        char put[] = "PUT";
        char slash[] = "/";
        char expect[] = "Expect: 100-continue";
        int content_len = 0;
        int content_len2 = 0;
        // Read to get header
        int valread = read(cl, buffer, buf_size);
    
        // Parse header
        //sscanf(buffer, "%s %s %s", header1, header2, header3);
        char * line = strtok(strdup(buffer), "\n");
        int first_line = 1;
        while (line != NULL) {
            // check for GET or PUT if first line in buffer
            // this only gets called once but if we change parameters 
            // then it will call all of it 
            if (first_line == 1) {
                sscanf(line, "%s %s %s", header1, header2, header3);
                if (header2[0] == '/') {
                    memmove(header2, header2 + 1, strlen(header2));
                }
                first_line = 0;
            }
            // will get the last content length
            if (strncmp(line, "Content-Length:", 15) == 0) {
                sscanf(line, "%s %d", header4, &content_len);
            }
            if (strncmp(line, "Expect: 100-continue", 25) == 0) {
                sscanf(line, "%s", header5);
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
            }
            // If target file name if not 27 ascii characters
            // Print 400 error message
            if (strlen(&header2[0]) != 27){
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
                    content_len2 = st.st_size;
                    sprintf(get_buff, 
                        "HTTP/1.1 200 OK\nContent-length: %d\r\n\r\n",
                        content_len2);
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
            // no content length
            if (strncmp(header4, "",1) == 0) {
                write(cl, bad_request, strlen(bad_request));
            }
            // If "/" then curl is called without target
            // Print 403 message because "/" are not allowed
            if (strstr(header2, slash) != NULL){
                write(cl, forbidden, strlen(forbidden));
            }
            // If target file name if not 27 ascii characters
            // Print 400 error message
            if (strlen(&header2[0]) != 27){
                write(cl, bad_request, strlen(bad_request));
            }
            else {
                int putread, bytes_left;
                int fd = open(header2, O_RDWR | O_CREAT | O_TRUNC, 0664);
                // Cannot get data or create file, print 500 message
                if (fd == -1) {
                    write(cl, internal_server_error, 
                                                strlen(internal_server_error));
                    close(fd);
                }
                else {
                    // send continue to accept data to write into fd
                    if (strstr(header5, expect) != NULL) {
                        write(cl, put_continue, strlen(put_continue));
                    }
                    // if content length fits in a single buffer
                    if (content_len < buf_size) {
                        // read then write 
                        putread = read(cl, file_buff, content_len);
                        write(fd, file_buff, putread);
                    }
                    else {
                        // do the first put read
                        putread = read(cl, file_buff, buf_size);
                        write(fd, file_buff, putread);
                        bytes_left = content_len - buf_size;
                        // write from full buffers
                        while(bytes_left >= buf_size) {
                            putread = read(cl, file_buff, buf_size);
                            write(fd, file_buff, putread);
                            bytes_left = bytes_left - buf_size;
                        }
                        // write the last bit
                        putread = read(cl, file_buff, buf_size);
                        write(fd, file_buff, putread);
                    }
                    write(cl, created, strlen(created));
                    close(fd);
                }
            }
        }
        close(cl);
    }
    return 0;
}