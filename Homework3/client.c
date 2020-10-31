#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#define cl_IP "127.0.0.1"
#define cl_PORT 45023

#define s_IP "127.0.0.1"
#define s_PORT 45022

int main(int argc, char *argv[]) {
    int m_sock;
    char buf[12];
    struct sockaddr_in client, server;
    int num_bytes;

    if((m_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    client.sin_family = AF_INET;
    client.sin_port = htons(cl_PORT);
    inet_pton(AF_INET, cl_IP, &(client.sin_addr)); //setup client socket

    printf("Binding...\n");
    
    #define NUM_TRIES 5
    int i = 0;

    while(bind(m_sock, (struct sockaddr *)&client, sizeof(client)) < 0) {
        if (i == 0) {
            perror("Error: Bind failed\n");
        }
        else {
            char* temp = "Attempt";
            char* m_b = malloc(strlen(temp) + 8);

            sprintf(m_b, "%s #%d\n", temp, i);
            
            perror(m_b);
        }
        sleep(2);

        if (i >= NUM_TRIES) {
            exit(0);
        }
    }

    printf("Bind completed\n");
    
    server.sin_family = AF_INET;
    server.sin_port = htons(s_PORT);
    inet_pton(AF_INET, s_IP, &(server.sin_addr)); //server ip

    printf("Sending initialization message to server...\n");

    int slen = sizeof(client);

    if (sendto(m_sock, buf, sizeof(buf), 0, (struct sockaddr*) &server, slen) < 0) { //send initialization message to server
        printf("Error sending message to server");
        exit(1);
    }

    #define ARR_SIZE 10000

    int index = 0;

    int arr[ARR_SIZE];
    memset(arr, -1, ARR_SIZE * sizeof(arr[0]));

    int ack[ARR_SIZE];
    memset(ack, 0, ARR_SIZE * sizeof(ack[0]));

    int received = 0;

    while(1) {
        slen = sizeof(struct sockaddr_in);

        int recv_size = recvfrom(m_sock, buf, sizeof(buf), 0, (struct sockaddr *) &server, &slen);

        if (recv_size < 2) {
            perror("Receive size less than expected\n");
        }

        index = (int)(buf[0]);
        
        arr[index] = (int)(buf[1]);
        ack[index] = 1;

        received += 1;

        if (received >= 3000) {
            printf("Received 3000 messages, breaking\n");
            break;
        }
    }

    printf("%d messages received\n", received);
}