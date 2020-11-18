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

#define client_IP "10.0.2.15"
#define client_PORT 45023

#define server_IP "10.0.2.15"
#define server_PORT 45022

#define NUM_BIND_TRIES 5

void *tcp_thread(void *ptr);
void *udp_thread(void *ptr);

struct msg {
    int chunkNum;
    int val;
};

struct sockaddr_in server;

sem_t mutex1;
sem_t mutex2; //figure out naming

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

    printf("Binding TCP socket...\n");

    j = 1;

    while (bind(tcp_sock, (struct sockaddr *) &client, sizeof(client)) < 0) {
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

    printf("TCP Bind completed\n");
    
    //initialize mutex semaphores

    sem_init(&mutex1, 0, 1);
    sem_init(&mutex2, 0, 1);

    //initialize and start threads

    pthread_t udp, tcp;
    int rc;
    rc = pthread_create(&udp, NULL, udp_thread, (void*) (intptr_t) udp_sock);

    if(rc) {
        perror("UDP failed to start\n");
    }

    sleep(1);

    rc = pthread_create(&tcp, NULL, tcp_thread, (void*) (intptr_t) tcp_sock);

    if(rc) {
        perror("TCP failed to start\n");
    }

    pthread_exit(NULL);
}

void *tcp_thread(void* ptr) {
    printf("Start TCP thread\n");

    sem_wait(&mutex1);
    sem_post(&mutex1);

    printf("Should not reach here until thread is done sending?");
}

void *udp_thread(void* sock) {
    printf("Start UDP thread\n");
    
    // sem_wait(&mutex1);

    // printf("Entering critical area, blocking mutex1");

    // printf("Sending initialization message to server...\n");

    // socklen_t slen = sizeof(server);  //initialize sock address len

    // if (sendto(udp_sock, buf, sizeof(buf), 0, (struct sockaddr*) &server, slen) < 0) { //send initialization message to server
    //     printf("Error sending message to server");
    //     exit(1);
    // }

    // printf("Message succesfully sent to %s at %d port \n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    // //declare recv_arr and ack array

    // #define ARR_SIZE 10000

    // int recv_arr[ARR_SIZE];
    // memset(recv_arr, -1, ARR_SIZE * sizeof(recv_arr[0]));

    // int ack[ARR_SIZE];
    // memset(ack, 0, ARR_SIZE * sizeof(ack[0]));

    // int index = 0, received = 0;

    // struct msg m_msg;

    // printf("Receiving messages from server\n");

    // while(1) {
    //     slen = sizeof(struct sockaddr_in);

    //     int recv_size;
    //     if(recv_size = (recvfrom(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &server, &slen)) < 0){
    //         printf("Error receiving message from server");
    //         exit(1);
    //     }

    //     index = m_msg.chunkNum;
        
    //     recv_arr[index] = m_msg.val;
    //     ack[index] = 1;

    //     received += 1;

    //     if (received >= 3000) {
    //         printf("Received 3000 messages, breaking\n");
    //         break;
    //     }
    // }

    // sem_post(&mutex1);
}