#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <netdb.h>

struct sockaddr_in sa;
int main()
{
    FILE* fp = fopen("new.txt", "w");
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(10059);
    inet_pton(AF_INET, "10.0.2.15", &(sa.sin_addr));    //set ip address of server here

    printf("Client connecting...\n");
    int x = connect(sock, (struct sockaddr *)&sa, sizeof(sa));

    char filesize[100];

    recv(sock, filesize, 100, 0);

    printf("%s\n", filesize);   //print information regarding file size

    while(1) {
        char buf[1024]; //set buffer to 1024 bytes, as we won't receive more than that
        int recv_size = recv(sock, buf, 1024, 0);

        const char *ptr = strchr(buf, '\0');    //search buffer for null terminating character (array was initialized with \0)
        if(ptr) {
            int index = ptr - buf;  //get index of \0
            
            fwrite(buf, sizeof(char), index, fp);   //write up to the first null terminating character

            if (index < 1023) { //if null terminating character is not at end of buf then there is more to go
                break;
            }
        }
    }
    printf("File contents successfully written\n");
    fclose(fp);
    
    printf("\nEnd of message\n");
}