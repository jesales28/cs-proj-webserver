// dog.c
// AUTHOR: Julia Sales
// ID: jesales
// DESCRIPTION: A simple program that replicates
// the cat function on termninal.

#include <stdio.h>

int main (int arg_count, char *arg[]) {
    int i;
    char *dash = "-";
    // If "dog" is just enterned into the command line
    if (arg_count == 1) {
        goto take_stdin;
    }
    // If more than one command is entered into command
    // line. can either be "-" or file name(s).
    else {
        for(i = 1;i <= arg_count; i++) {
            // If a dash is just entered
            if (*arg[1] == *dash) {
                goto take_stdin;
            }
            // If a file name(s) are entered
            else {

            }
        }
    }

    // This takes in command line input and then
    // writes it to standard output.
    take_stdin:
        char in[1];
        while(read(0, in, sizeof(in)) > 0) {
            write("in%c", buff, strlen(buff));
        }
    
}
