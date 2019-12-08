// httpserver.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple http server
// RESOURCES: piazza, stack overflow, gnu.com, class, and man pages
// as well as referenced medium.com <- helped with setting up the socket
// it was also similar/same code from section. Also used hw1 pseudo
// used cityhash by google from google's github: https://github.com/google/cityhash/

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
#include <errno.h>
#include <ctype.h>

#define BUFFSIZE 16000
#define SIZE 8000
#define MAX 100

// logging variables
int offset = 0;
int log_enable;

// logging function
void logging(char *request, char *target_file, char *http_title, int cont_len, char *buff, char *stat_code, char *log_file_name);

// queue
int queue[MAX];
int front, back;

// queue functions
void queue_insert(int value);
int queue_pop();

// thread structs
struct dispatch_params 
{
    char* bindaddr;
    int port;
};

// thread functions
void * dispatcher(void *dparams);
void * worker(void *arg);

// global locks
pthread_mutex_t queue_lock;
pthread_mutex_t worker_lock;
pthread_mutex_t log_lock;
pthread_mutex_t mem_lock;
pthread_cond_t worker_signal;
pthread_cond_t log_signal;
pthread_cond_t mem_signal;

// worker thread index count
int w_count = 1;

// socket function with params: addr and port
// returns the socket
int create_socket(char* bindaddr, int port);
void get_function(int con_l, char *r_target, char *r_method, char *r_http);

// global variables for setting up a socket
int PORT_NUMBER;
char *SERVER_NAME_STRING, *LOG_FILE, *ALIAS_FILE;

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

/*----------------------------------  MAIN  ----------------------------------*/

int main(int argc, char **argv) 
{
    // default nthreads
    int nthreads = 4;
    LOG_FILE = NULL;
    ALIAS_FILE = NULL;
    log_enable = 0;
    int option, log, map, map_fd;
    char *map_buff[128];
    char map_line[1];
    char magic_num[1];

    // get args for -N <nthread> and -l <logfile>
    //  use ':' after flag to indicate a required arg after flag
    while ((option = getopt(argc, argv, "N:l:a:")) != -1) 
    { 
        switch(option)
        {
            case 'N':
                nthreads = atoi(optarg);
                // make sure nthreads is greater than 4
                if (nthreads < 1)
                {
                    perror("Error: Number of threads must be 4 or greater\n");
                    exit(EXIT_FAILURE); 
                }
                break;
            case 'l':
                LOG_FILE = optarg;
                log_enable = 1;
                log = open(LOG_FILE, O_RDWR | O_CREAT | O_APPEND, 0664);
                if (log == -1)
                {
                    perror("Log file cannot be opended");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'a':
                ALIAS_FILE = optarg;
                // only need one open
                map_fd = open(ALIAS_FILE, O_RDWR | O_CREAT | O_APPEND, 0664);
                    if (map_fd == -1)
                    {
                        perror("Mapping file cannot be created");
                        exit(EXIT_FAILURE);
                    }
                    map = read(map_fd, map_line, sizeof(map_line));
                    if (map == -1)
                    {
                        perror("Mapping file cannot be read");
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        sscanf(map_line, "%s", magic_num);
                        //printf("magic num: %s\n", magic_num);
                        if (atoi(magic_num) != 1)
                        {
                            perror("Magic Number invalid");
                            exit(EXIT_FAILURE);
                        }
                    }
                break;            
            default:
                if (ALIAS_FILE == NULL)
                {
                    perror("Alias file does not exist");
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_FAILURE);
            
        }
    }

    // make sure we have exactly 2 positional args passed
    if (argv[optind] == NULL || argv[optind + 1] == NULL) 
    {
        perror("Error: Missing server name and/or port args.\n");
        exit(EXIT_FAILURE);
    }

    SERVER_NAME_STRING = argv[optind];
    PORT_NUMBER = atoi(argv[optind+1]);

    front = back = 0; 
    // init threads
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&worker_lock, NULL); 
    pthread_mutex_init(&log_lock, NULL);
    pthread_mutex_init(&mem_lock, NULL);
    pthread_cond_init(&worker_signal, NULL); 
    pthread_cond_init(&log_signal, NULL);
    pthread_cond_init(&mem_signal, NULL);
    pthread_t   dispatch_thread;
    pthread_t   worker_id[nthreads];

    // start dispatch thread with struct param
    struct dispatch_params dp;
    dp.bindaddr = SERVER_NAME_STRING;
    dp.port = PORT_NUMBER;
    pthread_create(&dispatch_thread, NULL, &dispatcher, (void *)&dp);

    // start worker threads
    for (int i = 1 ; i <= nthreads; i++)
    {
        pthread_create(&worker_id[i], NULL, &worker, NULL);
    }
    // main runs until pthread_join completes (which is never)
    int d_rc = pthread_join(dispatch_thread, NULL);
    return d_rc;


    
    return 0;
}

/*----------------------------  CREATE SOCKET  -------------------------------*/

int create_socket(char* bindaddr, int port)
{
    // create sockaddr_in
    struct hostent *hent = gethostbyname(bindaddr);
    struct sockaddr_in addr;
    memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == 0)
    {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    // bind socket to server
    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))<0) 
    {
        perror("In bind");
        exit(EXIT_FAILURE);
    }

    return sock;
}

/*------------------------------  GET FUNCTION  ------------------------------*/

void get_function(int con_l, char *r_target, char *r_method, char *r_http)
{
    char resp_buff[80];
    char file_buff[BUFFSIZE];
    int file_read;
    struct stat st;
    int fd = open(r_target, O_RDONLY);

    if (fd == -1)
    {
        write(con_l, file_not_found, strlen(file_not_found));
        //logging(r_method, r_target, r_http, 0, NULL, file_not_found, LOG_FILE);
    }
    else if (fd != -1)
    {
        file_read = read(fd, file_buff, BUFFSIZE);
        stat(r_target, &st);
        sprintf(resp_buff, "HTTP/1.1 200 OK\nContent-length: %lld\r\n\r\n", st.st_size);
        write(con_l, resp_buff, strlen(resp_buff));
        // if handling a large file
        while (file_read >= BUFFSIZE)
        {
            write(con_l, file_buff, file_read);
            file_read = read(fd, file_buff, BUFFSIZE);
        }
        write(con_l, file_buff, file_read);
        //logging(r_method, r_target, r_http, st.st_size, file_buff, ok, LOG_FILE);
    }    
    else
    {
        write(con_l, &internal_server_error, strlen(internal_server_error));
        //logging(r_method, r_target, r_http, 0, NULL, internal_server_error, LOG_FILE);
    }
    close(fd);

}

/*--------------------------  LOGGING FUNCTION  ------------------------------*/

void logging(char *request, char *target_file, char *http_type, int cont_len, char *buff, char *stat_code, char *log_file_name)
{
    if (log_enable == 0)
    {
        return;
    }
    else
    {
        pthread_mutex_lock(&log_lock);
        char first_buff[100];
        int prev_offset = offset;
        pthread_mutex_lock(&mem_lock);
        pthread_cond_wait(&mem_signal, &mem_lock);
        memset(first_buff, 0, sizeof(first_buff));
        pthread_mutex_unlock(&mem_lock);

        // char *hello = "Hello world\n";
        // pwrite(*log_file_name, hello, strlen(hello), 15);
        // use locks to lock offset, need to change offset
        if (*stat_code == *ok)
        {
            sprintf(first_buff, "%s %s length %d", request, target_file, cont_len);
            pwrite(*log_file_name, first_buff, strlen(first_buff), prev_offset);
            // turn content from buff to hex
            for (int i=0; i<strlen(buff); i++)
            {
                if (i%20 == 0)
                {
                    sprintf(log_file_name + strlen(log_file_name), "\n%08d %s2x ", i, buff);
                }
                else
                {
                    sprintf(log_file_name + strlen(log_file_name), "%s2x ",buff);
                }
            }
            sprintf(log_file_name + strlen(log_file_name), "========\n");
        }
        if (*stat_code == *created)
        {
            sprintf(first_buff, "%s %s length %d", request, target_file, cont_len);
            pwrite(*log_file_name, first_buff, strlen(first_buff), prev_offset);
            // turn content from buff to hex
            for (int i=0; i<strlen(buff); i++)
            {
                if (i%20 == 0)
                {
                    sprintf(log_file_name + strlen(log_file_name), "\n%08d %s2x ", i, buff);
                }
                else
                {
                    sprintf(log_file_name + strlen(log_file_name), "%s2x ",buff);
                }
            }
            sprintf(log_file_name + strlen(log_file_name), "========\n");
        }
        if (strstr(stat_code, bad_request) == 0)
        {
            sprintf(first_buff, "FAIL: %s %s %s --- response 400\n", request, target_file, http_type);
            pwrite(*log_file_name, first_buff, strlen(first_buff), prev_offset);
            sprintf(log_file_name + strlen(log_file_name), "========\n");
        }
        if (*stat_code == *forbidden)
        {
            sprintf(first_buff, "FAIL: %s %s %s --- response 403\n", request, target_file, http_type);
            pwrite(*log_file_name, first_buff, strlen(first_buff), prev_offset);
            sprintf(log_file_name + strlen(log_file_name), "========\n");
        }
        if (*stat_code == *file_not_found)
        {
            sprintf(first_buff, "FAIL: %s %s %s --- response 404\n", request, target_file, http_type);
            pwrite(*log_file_name, first_buff, strlen(first_buff), prev_offset);
            sprintf(log_file_name + strlen(log_file_name), "========\n");
        }
        if (*stat_code == *internal_server_error)
        {
            sprintf(first_buff, "FAIL: %s %s %s --- response 500\n", request, target_file, http_type);
            pwrite(*log_file_name, first_buff, strlen(first_buff), prev_offset);
            sprintf(log_file_name + strlen(log_file_name), "========\n");
        }
    }
    pthread_mutex_unlock(&log_lock);
    pthread_cond_signal(&mem_signal);
}

/*------------------------  QUEUE INSERT FUNCTION  ---------------------------*/

// Queue function to inserts socket into stack
void queue_insert(int value)
{
    if (back == MAX)
    {
        // queue is full
        return;
    }
    else
    {
        //insert element at the rear
        queue[back] = value;
        back++;
    }
    return;
}

/*-------------------------  QUEUE POP FUNCTION  -----------------------------*/

// Queue function to pop socket from front of queue
int queue_pop()
{
    int pop_value;
    // if queue is empty
    if (front == back) {
    }
    else
    {
        pop_value = queue[front];
        // shift all elements over
        for (int i = 0; i < back - 1; i++) {
            queue[i] = queue[i + 1];
        } 
        back--;
    }
    return pop_value;
}

/*--------------------------  DISPATCHER FUNCTION  ---------------------------*/

// Listens on socket and accepts connections
//  when connections arrive, they are placed into the queue
void *dispatcher(void *d_params)
{
    struct dispatch_params *d_par = (struct dispatch_params*)d_params;

    int sock = create_socket(d_par->bindaddr, d_par->port); 
    listen(sock, 10);

    while(1)
    {
        // waits until a client connection is established
        int cl = accept(sock, NULL, NULL); 

        // add connection to the queue
        // lock the queue - so we can take control and insert
        pthread_mutex_lock(&queue_lock);
        queue_insert(cl); 
        // release queue lock
        pthread_mutex_unlock(&queue_lock);
         // send worker signal
        pthread_cond_signal(&worker_signal);
    } 
}

/*----------------------------  WORKER FUNCTION  -----------------------------*/

void *worker(void *arg)
{
    pthread_mutex_lock(&worker_lock);
    int myid = w_count;
    w_count++;
    pthread_mutex_unlock(&worker_lock);

    char buffer[BUFFSIZE];
    char put_buff[BUFFSIZE];

    char request_method[3];
    char request_uri[28];
    char request_type[15];
    char new_uri[100];
    int content_len = 0;

    char slash[] = "/";

    int putread;
    int putbytes_left;

    while (1)
    {
        // lock worker
        pthread_mutex_lock(&worker_lock);
        // wait for worker signal from dispatcher
        pthread_cond_wait(&worker_signal, &worker_lock);

        // after we get a signal and a lock, we need to lock the queue
        //  so we can pop_value
        pthread_mutex_lock(&queue_lock);
        int w_cl = queue_pop();
        // unlock queue
        pthread_mutex_unlock(&queue_lock);
        // unlock worker, so other threads can pickup items in the queue
        pthread_mutex_unlock(&worker_lock);

        //  process connection 
        int valread;
        if ((valread = read(w_cl, buffer, sizeof(buffer) -1)) == -1) 
        {
            perror("In read");
            continue;
        }

        char *line, *process_buffer, *line_to_write;

        int file_d, wrote;
        int write_line_mode = 0;
        while (1)
        { 
            int done = 0; 
            // keep reading until the end of stream
            while ( valread > 0 ) 
            {
                // use the process_buffer to process line by line
                process_buffer = strdup(buffer);

                while ( (line = strsep(&process_buffer,"\n")) != NULL ) 
                {  
                    // if in PUT WRITE_LINE mode, write line to fd
                    if ( write_line_mode == 1 )  // this is for put
                    {
                        if ( putbytes_left > 0 )
                        { 
                            // write if file_d is good, else just move on to next request
                            // this whole block writes the content to file line by line
                            if ( file_d > 0 )
                            {
                                line_to_write = strdup(line);
                                wrote = write(file_d, strcat(line_to_write, "\r\n"), strlen(line_to_write));
                                putbytes_left = putbytes_left - wrote;    
                            }
                            else
                            {
                                // still need to go through rest of buffer but still needs to count bytes so we don't write, just pass content
                                putbytes_left = putbytes_left - strlen(line) - 2;    
                            }
                        } 
                        else
                        {
                            // no more data to write, reset write_line_mode and close the file
                            write_line_mode = 0;
                            close(file_d);
                            write(w_cl, created, strlen(created)); 
                            content_len = 0;
                        }
                    }

                    /*__________________  GET REQUEST FOUND __________________*/

                    if ( write_line_mode == 0 && (strncmp(line, "GET", 3)) == 0 )
                    {
                        // GET request line found
                        sscanf(line, "%s %s %s", request_method, request_uri, request_type);
                        // strip any leading '/'
                        if ( request_uri[0] == '/' )
                        {
                            memmove(request_uri, request_uri+1, strlen(request_uri));
                        }
                        // If "/" then curl is called without target
                        // Print 403 message because "/" are not allowed
                        if ( strstr(request_uri, slash) != NULL )
                        {
                            write(w_cl, forbidden, strlen(forbidden));
                            //logging(request_method, request_uri, request_type, 0, NULL, forbidden, LOG_FILE);
                        }
                        // If target file name if not 27 ascii characters
                        // Print 400 error message
                        if ( strlen(&request_uri[0]) != 27 )
                        {
                            write(w_cl, bad_request, strlen(bad_request));
                            //logging(request_method, request_uri, request_type, 0, NULL, bad_request, LOG_FILE);
                        }
                        else
                        {         
                            get_function(w_cl, request_uri, request_method, request_type);
                        }
                    }

                    /*__________________  PUT REQUEST FOUND __________________*/

                    if ( write_line_mode == 0 && (strncmp(line, "PUT", 3)) == 0 )
                    {
                        // PUT request line found
                        sscanf(line, "%s %s %s", request_method, request_uri, request_type);
                        // strip any leading '/'
                        if ( request_uri[0] == '/' )
                        {
                            memmove(request_uri, request_uri+1, strlen(request_uri));
                        }
                        // look for the content length in the next 10 lines
                        for (int i = 0; i<10 && (content_len == 0); i++)
                        {
                            line = strsep(&process_buffer, "\n");
                            if ( (strncmp(line, "Content-Length:", 15)) == 0 )
                            {
                                sscanf(line, "Content-Length: %d", &content_len);
                            }
                        }     
        
                        // make sure we found content_len
                        if ( content_len < 0 ) 
                        {
                            write(w_cl, bad_request, strlen(bad_request));
                            //logging(request_method, request_uri, request_type, 0, NULL, bad_request, LOG_FILE);
                        }
                        // If "/" then curl is called without target
                        // Print 403 message because "/" are not allowed
                        if ( strstr(request_uri, slash ) != NULL )
                        {
                            write(w_cl, forbidden, strlen(forbidden));
                            //logging(request_method, request_uri, request_type, 0, NULL, forbidden, LOG_FILE);
                        }
                        // If target file name if not 27 ascii characters
                        // Print 400 error message
                        if ( strlen(&request_uri[0]) != 27 )
                        {
                            write(w_cl, bad_request, strlen(bad_request));
                            //logging(request_method, request_uri, request_type, 0, NULL, bad_request, LOG_FILE);
                        } 
                        else 
                        {
                            // create file descriptor
                            file_d = open(request_uri, O_RDWR | O_CREAT | O_TRUNC, 0664);
                            // Cannot get data or create file, print 500 message
                            if ( file_d == -1 ) 
                            {
                                write(w_cl, internal_server_error, 
                                                strlen(internal_server_error));
                                //logging(request_method, request_uri, request_type, 0, NULL, internal_server_error, LOG_FILE);
                            }
                            // keep track of how many bytes to write
                            putbytes_left = content_len;

                            // look for continue or blank with next line
                            line = strsep(&process_buffer, "\n");
                            if ( (strncmp(line, "Expect: 100-continue", 20)) == 0 )
                            {
                                // signal client to continue and write data
                                write(w_cl, put_continue, strlen(put_continue));
                                // read stream and write to file
                                putread = read(w_cl, put_buff, sizeof(put_buff)); 

                                // write to file
                                wrote = write(file_d, put_buff, putread);
                                putbytes_left = putbytes_left - wrote;
                                
                                // keep writing if stream is larger than put_buff
                                while (putbytes_left > 0)
                                {
                                    // clear the put_buff cache before filling
                                    memset(put_buff, 0, sizeof(put_buff));
                                    putread = read(w_cl, put_buff, sizeof(put_buff));    
                                    wrote = write(file_d, put_buff, putread);
                                    putbytes_left = putbytes_left - wrote;
                                }
                                close(file_d);
                                write(w_cl, created, strlen(created));
                                //logging(request_method, request_uri, request_type, content_len, put_buff, created, LOG_FILE); 
                                content_len = 0;

                            } 
                            // send in line
                            else  //if ( line[0] == '\n' || line[0] == 0x00 )
                            {
                                write_line_mode = 1;

                            } 

                        }
                    }
                    // put another else if for ALIAS
                    else if ( write_line_mode == 0 && (strncmp(line, "PATCH", 5)) == 0 )
                    {
                        // PATCH request line found
                        sscanf(line, "%s %s %s", request_method, new_uri, request_type);
                        
                        // look for the content length in the next 10 lines
                        for (int i = 0; i<10 && (content_len == 0); i++)
                        {
                            line = strsep(&process_buffer, "\n");
                            if ( (strncmp(line, "Content-Length:", 15)) == 0 )
                            {
                                sscanf(line, "Content-Length: %d", &content_len);
                            }
                        }
                        // make sure we found content_len
                        if ( content_len < 0 ) 
                        {
                            write(w_cl, bad_request, strlen(bad_request));
                            
                        }
                        else
                        {
                            // look for continue or blank with next line
                            line = strsep(&process_buffer, "\n");
                            if ( (strncmp(line, "Expect: 100-continue", 20)) == 0 )
                            {
                                write(w_cl, put_continue, strlen(put_continue));
                            }
                            else
                            {

                            }   
                        }


                    }
                }

                // we reached the end of buffer, make sure we are at the end of the stream
                // clear buffer to read next set of data
                memset(buffer, 0, sizeof(buffer));
                valread = read(w_cl, buffer, sizeof(buffer) -1); 
            }
    
            // session done
            //printf("-- session end --\n\n"); 

            // clear the buffer for sessions read
            memset(buffer, 0, sizeof(buffer));
            //close(w_cl);
        }
        close(w_cl);
    }
    return 0;
}