#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

int getfilesize(char*);

int send_size, recv_size;
int main(int argc, char *argv[])
{
    int socket_desc, client_sock, c, read_size; //sockets and such
    struct sockaddr_in server, client;          //one for listening one for connection with client(s)
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    client_sock = socket(AF_INET, SOCK_STREAM, 0);

    char* filename = "file.txt";    //set file name to send here

    printf("Sockets created\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(10059);
    inet_pton(AF_INET, "10.0.2.15", &(server.sin_addr)); //set ip address of server here

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {

        perror("bind failed. Error"); //perror is a very helpful function for tracking down errors
        return 1;
    }
    printf("Bind completed \n");

    c = sizeof(struct sockaddr_in);

    listen(socket_desc, 1);

    printf("Waiting for incoming connections...\n");

    client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
    //if successful, client is filled in with address of connecting socket
    if (client_sock < 0)
    {
        perror("accept failed");
        return 1;
    }

    printf("Connection accepted\n");

    char st[100] = "The size of the file is ";

    int size = getfilesize(filename);
    char size_buf[50];
    sprintf(size_buf, "%d", size);

    strcat(st, size_buf);
    char* _b = " bytes.";
    strcat(st, _b); //convoluted strcat to get string i want

    send(client_sock, st, 100, 0);

    char buf[1024]; //set buf to 1024 bytes
    memset(buf, '\0', sizeof(char) * 1024);

    FILE *fp;

    fp = fopen(filename, "r");
    
    int count = 0;
    while(1) {
        char c = fgetc(fp);

        if (c == EOF) {
            buf[count+1] = '\0';

            send(client_sock, buf, 1024, 0);

            break;
        }

        buf[count] = c;
        count+= 1;

        if (count == 1023) {    //if reached buf size - 1, stop and send
            send(client_sock, buf, 1024, 0);

            count = 0;

            memset(buf, '\0', sizeof(char) * 1024);
        }
    }

    fclose(fp);

    close(socket_desc);
    close(client_sock);
    return 0;
}

int getfilesize(char* filename) {
    int count = 0;
    FILE* fp = fopen(filename, "r");

    while (1) {
        char c = fgetc(fp);

        if (c == EOF) {
            break;
        }
        count += 1;
    }
    fclose(fp);

    return count;
}