Compilation: 
gcc -pthread -o server server.c -std=gnu99
gcc -pthread -o client client.c -std=gnu99

Execution:
./server
./client

Make sure IPs and ports are correct before execution

I believe there's no math library in this, so -lm is not necessary