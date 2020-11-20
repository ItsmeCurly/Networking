#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h> 
#include <semaphore.h>
#include <stdbool.h>

#define client_IP "10.0.2.15"
#define client_PORT 45023

#define server_IP "130.111.46.105"
#define server_PORT 45022

#define NUM_BIND_TRIES 5
#define ARR_SIZE 10000

void *tcp_thread(void *ptr);
void *udp_thread(void *ptr);

struct msg {
    int chunkNum;
    int val;
};

struct sockaddr_in server;

pthread_mutex_t mutex1;
pthread_mutex_t mutex2; //figure out naming

bool all_sent = false, done = false;

int ack[ARR_SIZE];

int main(int argc, char *argv[]) {
    int udp_sock;
    char buf[12];
    struct sockaddr_in client;
    int num_bytes;
    
    //setup client sockaddr_in data
    client.sin_family = AF_INET;
    client.sin_port = htons(client_PORT);
    inet_pton(AF_INET, client_IP, &(client.sin_addr));

    //setup server sockaddr_in data
    server.sin_family = AF_INET;
    server.sin_port = htons(server_PORT);
    inet_pton(AF_INET, server_IP, &(server.sin_addr));

    //create and bind UDP socket

    if((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    //set socket to nonblocking mode

    int flags = fcntl(udp_sock, F_GETFL, 0);
    fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
    perror("SET NOBLOCK");

    int j = 1;

    printf("Binding UDP socket...\n");

    while (bind(udp_sock, (struct sockaddr *) &client, sizeof(client)) < 0) {
        if (j == 1) {
            perror("Error: Bind failed");
        }
        else {
            char* temp = "Attempt";
            char* m_b = malloc(strlen(temp) + 8);

            sprintf(m_b, "%s #%d", temp, j);
            
            perror(m_b);
        }
        sleep(2);   //attempt to bind sometimes fails, set it so that it waits 2 seconds after every failed bind, up to 5 attempts

        if (j >= NUM_BIND_TRIES) {
            exit(0);
        }
        j+=1;
    }

    printf("UDP Bind completed\n");

    //create and bind tcp socket

    int tcp_sock;

    if((tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    // printf("Binding TCP socket...\n");

    // j = 1;

    // while (bind(tcp_sock, (struct sockaddr *) &client, sizeof(client)) < 0) {
    //     if (j == 1) {
    //         perror("Error: Bind failed");
    //     }
    //     else {
    //         char* temp = "Attempt";
    //         char* m_b = malloc(strlen(temp) + 8);

    //         sprintf(m_b, "%s #%d", temp, j);
            
    //         perror(m_b);
    //     }
    //     sleep(2);   //attempt to bind sometimes fails, set it so that it waits 2 seconds after every failed bind, up to 5 attempts

    //     if (j >= NUM_BIND_TRIES) {
    //         exit(0);
    //     }
    //     j+=1;
    // }

    // printf("TCP Bind completed\n");
    
    //initialize mutex semaphores

    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&mutex2, NULL); //error checking

    //initialize and start threads

    pthread_t udp, tcp;
    int rc;
    rc = pthread_create(&udp, NULL, udp_thread, (void*) (intptr_t) udp_sock);

    if(rc) {
        perror("UDP failed to start\n");
    }

    rc = pthread_create(&tcp, NULL, tcp_thread, (void*) (intptr_t) tcp_sock);

    if(rc) {
        perror("TCP failed to start\n");
    }

    pthread_exit(NULL);
}

void *tcp_thread(void* sock) {
    printf("TCP: Start thread\n");
    int tcp_sock = (int) (intptr_t) sock;

    printf("TCP: Client connecting...\n");
    
    int x = connect(tcp_sock, (struct sockaddr *) &server, sizeof(server));

    printf("TCP: Successfully connected with %s at port %d\n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    while(!done) {
        printf("TCP: Attempting receive of all_sent message\n");
        if ((recv(tcp_sock, &all_sent, sizeof(bool), 0)) < 0) {
            printf("TCP: Error receiving message from server");
        }
        
        pthread_mutex_lock(&mutex1);

        printf("TCP: All_sent message received\n");

        printf("TCP: Sending back acknowledgement array\n");

        send(tcp_sock, ack, ARR_SIZE * sizeof(ack[0]), 0);

        pthread_mutex_unlock(&mutex1);
    }
}

void *udp_thread(void* sock) {
    printf("UDP: Start thread\n");
    int udp_sock = (int) (intptr_t) sock;

    socklen_t slen = sizeof(server);  //initialize sock address len
    char buf[12];

    printf("UDP: Sending initialization message to server...\n");

    if (sendto(udp_sock, buf, sizeof(buf), 0, (struct sockaddr*) &server, slen) < 0) { //send initialization message to server
        printf("UDP: Error sending message to server");
        exit(1);
    }

    printf("UDP: Message succesfully sent to %s at %d port \n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    //declare recv_arr and ack array

    int recv_arr[ARR_SIZE];
    memset(recv_arr, -1, ARR_SIZE * sizeof(recv_arr[0]));
    memset(ack, 0, ARR_SIZE * sizeof(ack[0]));

    int index = 0, received = 0, attempts = 0;

    struct msg m_msg, prev_msg;
    prev_msg.chunkNum = -1;

    printf("UDP: Receiving messages from server\n");

    while(!done) {
        int new_values[10000];
        memset(new_values, -1, ARR_SIZE * sizeof(new_values[0]));

        int new_val_index = 0;
        while(1) {
            slen = sizeof(struct sockaddr_in);

            recvfrom(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &server, &slen);

            if(prev_msg.chunkNum == m_msg.chunkNum) {
                continue;
            }

            index = m_msg.chunkNum;
            
            recv_arr[index] = m_msg.val;
            ack[index] = 1;

            new_values[new_val_index] = m_msg.val;
            new_val_index += 1;

            if(all_sent) {
                all_sent = false;
                attempts+=1;
                printf("New values:");

                for (int i = 0; i < ARR_SIZE; i++) {
                    if(new_values[i] != -1) {
                        printf(" %d", new_values[i]);
                    }
                }
                printf("\n");
                break;
            }

            prev_msg = m_msg;
        }
        pthread_mutex_lock(&mutex1);
        bool all_received = true;
        for (int i=0; i < ARR_SIZE; i++) {
            if (!ack[i]) {
                all_received = false;
            }
        }

        if (all_received) {
            done = true;
            printf("Transferral: All done. Attempts made at sending: %d\n", attempts);
        }

        pthread_mutex_unlock(&mutex1);
    }
}