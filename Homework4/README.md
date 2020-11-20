Requires -pthread to run along with -std=c99.

Server.c and client.c scheduling issue fixed with a sleep before the termination of the server program.
Allows TCP thread to finish sending/receiving ack array before exiting.