#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

void *Handle_Comm(void *);

pthread_t tid[15];
char client_message[20000];
int send_size, recv_size;

int main(int argc, char *argv[])
{
    int socket_desc, client_sock, c, read_size;
    struct sockaddr_in server, client, my_addr;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    client_sock = socket(AF_INET, SOCK_STREAM, 0);

    printf("Sockets created\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(10059);
    inet_pton(AF_INET, "130.111.216.48", &(server.sin_addr));

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Bind failed. Error");
        return 1;
    }
    printf("bind completed \n");

    c = sizeof(struct sockaddr_in);

    listen(socket_desc, 15);

    while (1)
    {
        printf("Waiting for incoming connections...\n\n");
        fflush(stdout);

        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }

        printf("Connection accepted... Creating thread to handle communiction\n\n");
        fflush(stdout);
        pthread_create(&(tid[i]), NULL, handle_comm, &client_sock);
        i++;
        if (i == 15)
            i = 0;
    }
}

void* handle_comm(void *socket) 
{
    int client_sock = *((int *)socket);
    recv_size = recv(client_sock, client_message, 32, 0);

    printf("Received %d bytes. Msg is:\n %s \n",
           recv_size, client_message);

    sprintf(client_message, "We will get back to you shortly..\n");
    send_size = send(client_sock, client_message, 32, 0);
    printf("Thread Exiting!\n");
    fflush(stdout);
}
