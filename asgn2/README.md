Name: Julia Sales
ID: jesales
Description: README.md for asgn2

To compile my code, type: make
To run my code, type: ./httpserver -N *number of threads* *server name* *port number*
Open a new tab to see client output and test on there.
Program was only able to pass 2 of the test cases. Multithreading does not work.
My dispatcher logic is completely wrong, therefore my code is wrong. I take in
requests instead of the sockets, and the dispatcher would assign the requests into
different threads, which is not correct. 
