// dog.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple program that replicates
// the cat function on termninal.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

int main (int argc, char *argv[]) {
    int i, j, in;
    char *dash = "-";
    char stdin_buf[1];
    // This takes in stdin
    // from command line
    // If "dog" is just enterned into the command line
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
            // If a dash is just entered
            if (*argv[1] == *dash) {
                in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
                while (in > 0) {                         // This takes in stdin
                    write (STDOUT_FILENO, stdin_buf, sizeof(stdin_buf));  // from command line
                    in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
                }
            }
            // If a file name(s) are entered
            else {
                if (argc > 1) {
                    for (j = 1; j < argc; j++ ) {
                        // opens first file
                        // print errors if no file name is found
                        int file = open (argv[j], O_RDONLY);
                        char file_buf[32768];   // sizeof(*arg[j])
                        //char *file_name = arg[j];
                        if ( file == -1 ){
                            // print errors
                            warn ("%s", argv[j]);
                            //printf ("dog: %s: No such file or directory", );
                            exit (1);
                            //warn ("dog: %s: No such file or directory", *file_name);
                        }
                        else {
                            int file_in = read (file, file_buf, sizeof(file_buf));
                            if (file_in < 0){
                                printf ("Error: cannot read file");
                                exit (1);
                            }
                            else {
                                write (STDOUT_FILENO, file_buf, sizeof(file_buf));
                                close(in);
                                close (file); 
                            } 
                            //printf ("%d\n", file_in);
                            //while (file > 0) {
                                //printf ("file\n");                       
                                //write (1, file_buf, sizeof(file_buf));  //
                                //file_in = read (*arg[j], file_buf, sizeof(file_buf));
                            //}
                        }
                    }
                }
                //else {
                    // opens first file
                    //int file = open (*arg[1], O_RDONLY); 
                    // print errors if no file name is found
                    //printf( "not programmed yet");
                //}
            }
        }
    }
}

