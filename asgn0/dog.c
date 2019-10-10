// dog.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple program that replicates
// the cat function on termninal.

#include <stdio.h>
#include <unistd.h>

int main (int arg_count, char *arg[]) {
    int i, j, in;
    char *dash = "-";
    char stdin_buf[1];
    // This takes in stdin
    // from command line
    // If "dog" is just enterned into the command line
    if (arg_count == 1) {
        in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
        while (in > 0) {                                 
            write (STDOUT_FILENO, stdin_buf, sizeof(stdin_buf));          
            in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
        }
    }
    // If more than one command is entered into command
    // line. can either be "-" or file name(s).
    else {
        for(i = 1; i < arg_count; i++) {
            // If a dash is just entered
            if (*arg[1] == *dash) {
                in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
                while (in > 0) {                         // This takes in stdin
                    write (1, stdin_buf, sizeof(stdin_buf));  // from command line
                    in = read (STDIN_FILENO, stdin_buf, sizeof(stdin_buf));
                }
            }
            // If a file name(s) are entered
            else {
                if (arg_count > 2) {
                    //for (j = 1; j < arg_count; j++ ) {
                        // opens first file
                        //int file = open (*arg[j], O_RDONLY);
                        // print errors if no file name is found
                    //}
                    printf( "not programmed yet");
                }
                else {
                    // opens first file
                    //int file = open (*arg[1], O_RDONLY); 
                    // print errors if no file name is found
                    printf( "not programmed yet");
                }
            }
        }
    }
}

