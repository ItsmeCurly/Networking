#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#define cl_IP "130.111.46.105"
#define cl_PORT 45023

#define s_IP "10.0.2.15"
#define s_PORT 45022

struct msg {
    int chunkNum;
    int val;
};

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
        sleep(2);   //retry in the next two seconds

        if (i >= NUM_TRIES) {
            exit(0);
        }
    }

    printf("Bind completed\n");
    
    server.sin_family = AF_INET;
    server.sin_port = htons(s_PORT);
    inet_pton(AF_INET, s_IP, &(server.sin_addr)); //server ip

    printf("Sending initialization message to server...\n");

    socklen_t slen = sizeof(client);  //initialize sock address len

    if (sendto(m_sock, buf, sizeof(buf), 0, (struct sockaddr*) &server, slen) < 0) { //send initialization message to server
        printf("Error sending message to server");
        exit(1);
    }

    printf("Message succesfully sent to server\n");

    //declare recv_arr and ack array

    #define ARR_SIZE 10000

    int recv_arr[ARR_SIZE];
    memset(recv_arr, -1, ARR_SIZE * sizeof(recv_arr[0]));

    int ack[ARR_SIZE];
    memset(ack, 0, ARR_SIZE * sizeof(ack[0]));

    int index = 0, received = 0;

    struct msg m_msg;

    printf("Receiving messages from server\n");

    while(1) {
        slen = sizeof(struct sockaddr_in);

        int recv_size = recvfrom(m_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &server, &slen);

        if (recv_size < 2) {
            perror("Receive size less than expected\n");
        }

        index = m_msg.chunkNum;
        
        recv_arr[index] = m_msg.val;
        ack[index] = 1;

        received += 1;

        if (received >= 3000) {
            printf("Received 3000 messages, breaking\n");
            break;
        }
    }

    printf("%d messages received\n", received);

    for (int i = 0; i < ARR_SIZE; i++) {
        if(recv_arr[i] == -1) {
            printf("First gap located at position %d\n", i);
            break;
        }
    }
}