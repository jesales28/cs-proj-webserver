// httpserver.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple http server
// RESOURCES: piazza, stack overflow, class, man pages, and logic from asgn0
// as well as referenced medium.com <- helped with setting up the socket
// it was also similar/same code from section. Also used hw1 pseudo

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
#include <semaphore.h>
#include <errno.h>

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

// Global variables
const int buf_size = 32768;
char buffer[buf_size] = {0};
char get_buff[buf_size];
char file_buff[buf_size]; 
char header1[3];            // contains request
char header2[28];           // contains target
char header3[15];           // contains HTTP/1.1
char get[] = "GET";
char put[] = "PUT";
char slash[] = "/";
char expect[] = "Expect: 100-continue";
int content_len = 0;
int content_len2 = 0;
int valread, cl; 
size_t  size = 0;
size_t  buf_used = 0;

// global locks
pthread_mutex_t mutex;
pthread_cond_t full;
pthread_cond_t empty;
pthread_cond_t cl_req;
// variables going to be used for multithreading
const int n = 16000;
char request_buff[n] = {0};
int n_available[n] = {0};
int n_in_use = 0;
int waiting_threads = 0;
int req_written;


// from https://www.linuxquestions.org/questions/programming-9/extract-substring-from-string-in-c-432620/
//   not sure if works, haven't tried it yet
char* substring(const char* str, size_t begin, size_t len)
{
    if (str == 0 || strlen(str) == 0 || strlen(str) < begin 
                                                || strlen(str) < (begin+len)) {
        return 0;
    }
    return strndup(str + begin, len);
}

/*----------------------------  GET FUNCTION  --------------------------------*/

int *get_function(int cl, const char *target) {
    // If file is not found, print 404 error message
    int file_fetch;
    int file = open(target, O_RDONLY);
    if (file == -1){
        write(cl, file_not_found, strlen(file_not_found));
    }
    // If file is found, print 200 message
    else if (file != -1) {
        file_fetch = read(file, file_buff, buf_size);
        struct stat st;
        stat(target, &st);
        content_len2 = st.st_size;
        sprintf(get_buff, "HTTP/1.1 200 OK\nContent-length: %d\r\n\r\n",
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
        write(cl, &internal_server_error, strlen(internal_server_error));
    }
    close(file);
    close(file_fetch);
    return 0;
}

/*-----------------------------  PUT FUNCTION  -------------------------------*/

int *put_function(int cl, int fd, int putread, int bytes_left, 
        const char *target, int content_len, int buf_size, char *file_buff) {
    if (fd == -1) {
        write(cl, internal_server_error, strlen(internal_server_error));
        close(fd);
    }
    else {
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
    return 0;
}

/*----------------------------  PARSE FUNCTION  ------------------------------*/

int *parse(int cl, char buffer) {
    //printf("got here\n");
    //5********
    // try with strchr from 
    // https://stackoverflow.com
    char *start, *end;
    char * line = malloc(buf_size);;
    char * putline = malloc(buf_size);;
    int line_size = 0;
        
    int filter_request = 1;
    start = end = &buffer;
    while ( (end = strchr(start, '\n')) ) {
        line_size = sprintf(line, "%.*s", (int)(end - start + 1), start);
        printf("DEBUG: ****** size: %d , line: %s", line_size, line); 

        // Do the processing here ( see if we can get PUT content )
        if ( filter_request == 1 ) {
            printf("DEBUG: looking for request\n");
            // reset headers to NULL with memset
            memset(header1, 0, sizeof header1);
            memset(header2, 0, sizeof header2);
            memset(header3, 0, sizeof header3);
            sscanf(line, "%s %s %s", header1, header2, header3);
            if (header2[0] == '/') {
                memmove(header2, header2 + 1, strlen(header2));
            }

            if (*header1 == *put) {
                printf("DEBUG: got put: %s", line);
                // look for content length
                int put_content_len;
                int len_found = 0;
                int put_linesize;
                int ERROR = 0;
                while ( (end = strchr(start, '\n')) && len_found == 0 ) {
                    line_size = sprintf(line, "%.*s", (int)(end - start + 1), start);
                    if (strncmp(line, "Content-Length:", 15) == 0) { 
                        sscanf(line, "Content-Length: %d", &content_len);
                        printf("DEBUG: put content len is: %d\n", content_len);
                        len_found = 1;    
                    }
                    start = end + 1;
                }
                // no content length
                if (len_found == 0) {
                    ERROR = 1;
                    write(cl, bad_request, strlen(bad_request));
                }
                // If "/" then curl is called without target
                // Print 403 message because "/" are not allowed
                if (strstr(header2, slash) != NULL){
                    ERROR = 1;
                    write(cl, forbidden, strlen(forbidden));
                }
                // If target file name if not 27 ascii characters
                // Print 400 error message
                if (strlen(&header2[0]) != 27){
                    ERROR = 1;
                    write(cl, bad_request, strlen(bad_request));
                }
                else {
                    if (ERROR == 0) {
                        // Proccess open file
                        int fd = open(header2, O_RDWR | O_CREAT | O_TRUNC, 0664);
                        int putread, bytes_left;;
                        // Add file errors and responses here

                        int found_expect_or_blank = 0;
                        // loop until expect 100 or blank line is found 
                        // or until end of buffer
                        while ( (end = strchr(start, '\n')) && 
                                                found_expect_or_blank == 0 ) {
                            line_size = sprintf(line, "%.*s", 
                                                (int)(end - start + 1), start); 
                            if (strncmp(line, "Expect: 100-continue", 20) == 0) {
                                printf("DEBUG: found put expect 100\n");
                                found_expect_or_blank = 1;
                                // send client 100-continue
                                write(cl, put_continue, strlen(put_continue));
                                // START Process client response from 100-eontinue
                                put_function(cl, fd, putread, bytes_left, 
                                header2, content_len, buf_size, file_buff);
                                // END Process client response from 100-eontinue
                            }
                            if (line[0] == '\n') {
                                // found blank line, next line is the put content
                                printf("DEBUG: found put blank line\n");
                                int found_expect_or_blank = 1;
                                // skips the blank line
                                end = strchr(start, '\n');
                                line_size = sprintf(line, "%.*s", (int)(end - start + 1), start);
                                start = end + 1;
                                // read in the next number of bytes: content_len
                                int put_size_in = 0;
                                char put_buffer[buf_size];
                                while (put_size_in < content_len) {
                                    end = strchr(start, '\n');
                                    line_size = sprintf(line, "%.*s", (int)(end - start + 1), start);
                                    printf("DEBUG: simulate write to file: %s", line); 
                                    write(fd, line, line_size + 1);
                                
                                    // add one byte to include \n
                                    put_size_in = put_size_in + line_size + 1;
                                    printf("DEBUG: put_size_in: %d\n", put_size_in);
                                    start = end + 1;
                                }
                            }
                                start = end + 1;
                        }
                    }
                }    
            }
            if (*header1 == *get) {
                //printf("DEBUG: got get: %s", line);
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
                    get_function(cl, header2);
                }
            }
            start = end + 1;
        }
    }
    return 0;
}

/*----------------------------  QUEUE FUNCTION  ------------------------------*/

void *queue(int cl, char buffer1[]){
    int in = 0;
    while(1){
        pthread_mutex_lock(&mutex);
        if (n_in_use == n){
            pthread_cond_wait(&empty, &mutex);
        }
        //if (&request_buff[in] == NULL) {
            //memmove(&request_buff[in], buffer1, strlen(buffer1));
            //printf("%s", &request_buff[in]);
        //     printf("%s", &request_buff[in]);
        //     n_in_use += 1;
        //     for (int i=0; i < waiting_threads; i++){
        //         pthread_cond_signal(&full);
        //     }
        //}
        // printf("%s\n", &request_buff[0]);
        while (req_written == 0){
            if (request_buff[in] != NULL) {
                printf("in here\n");
                in = (in + 1) % n;
            }
            //printf("%d\n%s\n", in, &request_buff[in]);
            memmove(&request_buff[in], buffer1, strlen(buffer1));
            printf("%d\n%s\n", in, &request_buff[in]);
            n_in_use += 1;
            for (int i=0; i < waiting_threads; i++){
                pthread_cond_signal(&full);
            }
            req_written = 1;
        }
        //printf("%d\n", in);
        //printf("%d\n%s\n", in, &request_buff[in]);
        // if (&request_buff[in] != NULL) {
        //     in = (in + 1) % n;
        // }
        // memmove(&request_buff[in], buffer1, strlen(buffer1));
        // n_in_use += 1;
        //pthread_cond_wait(&cl_req, &mutex);
        // for (int i=1; i < waiting_threads; i++){
        //     pthread_cond_signal(&full);
        // }
        pthread_mutex_unlock(&mutex);
    }
}

/*--------------------------  THREADING FUNCTION  ----------------------------*/

void *dispatcher(void *args){
    int out = 0;
    char *thrd_ptr = (char *)args;
    while(1){
        pthread_mutex_lock(&mutex);
        for (int i=0; i < n; i++){
            if (&request_buff[i] == NULL){
                waiting_threads += 1;
                pthread_cond_wait(&full, &mutex);
                waiting_threads -= 1;
            }
        } 
        while (&request_buff[out] == NULL){
            out = (out + 1) % n;
        }
        *thrd_ptr = request_buff[out];
        // while (thrd_ptr != NULL){
        //parse(cl, *thrd_ptr);
        // }
        n_in_use -= 1;
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}

/*----------------------------------  MAIN  ----------------------------------*/

int main (int argc, char *argv[]) {
    int PORT_NUMBER, nthread;
    char *SERVER_NAME_STRING, LOG_FILE;
    if (argv[2] == NULL) {
        fprintf(stderr, "Request is missing required `worker thread` header\n");
        exit (1);
    }
    else {
        nthread = atoi(argv[2]);
    }
    if (argv[4] == NULL) {
        fprintf(stderr, "Request is missing required `log file` header\n");
        exit (1);
    }
    /*else {
        LOG_FILE = *argv[4];
    }
    if (argv[5] == NULL) {
        fprintf(stderr, "Request is missing required `Host` header\n");
        exit (1);
    }*/
    else {
        SERVER_NAME_STRING = argv[3];
    }
    if(argv[6] == NULL){
        fprintf(stderr, "Request is missing required `Port` header\n");
        exit(1);
    }
    else{
        PORT_NUMBER = atoi(argv[4]);
    }

    //int n_available[nthreads] = {0};
    // Create threads
    pthread_t tid[nthread];

    int i = 0;
    int error;
    while(i<nthread) {
        error = pthread_create(&tid[i], NULL, &dispatcher, (void*)&tid[i]); 
        if (error != 0){
            printf("\nThread can't be created :[%s]\n", strerror(error)); 
        } 
        else {
	        printf("\nThread created\n");
        }
        i++; 
    }

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

    while (1){
        cl = accept(sock, NULL, NULL);
        if (cl<0) {
            perror("In accept");
            exit(EXIT_FAILURE);
        }

        // Read to get header
        //valread = read(cl, buffer, buf_size); 

        //****************
        //  try from https://stackoverflow.com
        if ((valread = read(cl, buffer, sizeof(buffer) - 1)) == -1) {
            perror("In read");
            exit(EXIT_FAILURE);
        }

        if (valread == 0){
            break;
        }
        buffer[valread] = '\0';

        char line_buffer[256];
       
        size_t  index = 0;
        //pthread_cond_signal(&cl_req);

        //printf("DEBUG: size of original buffer: %lu\n", strlen(buffer));
        //printf("%s\n", buffer);
        req_written = 0;
        if (req_written == 0){
           queue(cl, buffer); 
        }
        //parse(cl, *buffer);
        close(cl);
    }
    int j = 0;
    while (j < nthread){
        pthread_join(tid[j], NULL);
        j++;
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}
