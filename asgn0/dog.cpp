// dog.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple program that replicates
// the cat function on termninal.
// Resources used were piazza posts and man pages.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>

int main (int argc, char *argv[]) {
    int i, j, in, file_in, file;
    const int buf_size = 32768;  
    char dash[] = "-";
    char stdin_buf[1];
    // This takes in stdin
    // from command line
    // If "dog" is just enterned into the command line
    // SOURCE: read how to use read and write on manpages
    if (argc == 1) {
        in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
        while (in > 0) {                                 
            write (STDOUT_FILENO, stdin_buf, sizeof(stdin_buf));          
            in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
        }
    }
    // If more than one command is entered into command
    // line. can either be "-" or file name(s).
    else {
        for(i = 1; i < argc; i++) {
            // error if no file is found
            // If a dash is entered
            if (*argv[i] == *dash) {
                // This takes in stdin from command line
                in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
                while (in > 0) {                              
                    write (1, stdin_buf, sizeof(stdin_buf));
                    in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
                }
            }
            // If a file name(s) are entered
            else {
                if(argc > 1) {
                    // opens first file
                    // print errors if no file name is found
                    file = open (argv[i], O_RDONLY);
                    // SOURCE: Referenced from student on piazza
                    const int buf_size = 32768;
                    char file_buf[buf_size];       
                    // print error is file is not found
                    if ( file == -1 ){
                        warn ("%s", argv[i]);
                        exit (1);
                    }
                    else {
                        file_in = read (file, file_buf, buf_size);
                        // print error if file cannot be read
                        if (file_in == -1){
                            warn ("%s", argv[i]);
                            exit (1);
                        }

                            //while (file_in >= buf_size) {
                                //file_in = read (file, file_in, buf_size);
                                //write (1, file_in, buf_size);
                            //}
                            //file_buf[]; 
                            //end = read(file, file_buf, buf_size + file_in);
                            //printf ("%d\n", end);
                            //if ()
                        // write the contents of a file
                        else { 
                            while (file_in >= buf_size) {
                                write (1, file_buf, file_in);
                                file_in = read (file, file_buf, file_in);
                                
                            } 
                            write (1, file_buf, file_in); 
                        }
                    }
                }
            }
        }
        close(in);
        close (file_in);
        close (file); 
    }
}

