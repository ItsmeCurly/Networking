Had a bit of a hassle with the code a day before the due date, lots of issues in transmission across the aturing-vm line. Resolved them with the MSG_WAITALL which I debugged for about 3 hours before realizing (sigh).

My task then led into file writing and I had a bit of a struggle with that too, currently it seems like it is 100% working across the aturing line with both SACK and NACK (tested both around 10 times with no issue seen).

One notable thing of interest that I wasn't too sure about is that the console seems to lag on the NACK and sometimes on the SACK implementation, but there is no noticable issue in the actual execution. If you have any idea why this is I'd like to hear it.

Compilation: 
gcc -pthread -o server server.c -lm -std=gnu99 -lrt
gcc -pthread -o client client.c -lm -std=gnu99 -lrt

Execution(SACK):
./server -acktype sel -time
./client -acktype sel -time

Execution(NACK):
./server -acktype neg -time
./client -acktype neg -time

Server is for aturing/aws, client is for local use

For more information on execution, add -debug to the execution statement (not perfect debugging, just sends to console and doesn't have all variables listed)

I didn't have enough time for cumulative acknowledgements, sorry.
