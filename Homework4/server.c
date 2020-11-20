#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h> 
#include <semaphore.h> 
#include <stdbool.h>

#define server_IP "10.0.2.15"
#define server_PORT 45022
#define NUM_BIND_TRIES 5
#define ARR_SIZE 10000

char* concat(const char*, const char*);
void *tcp_thread(void *ptr);
void *udp_thread(void *ptr);

struct msg {
    int chunkNum;
    int val;
};

pthread_mutex_t mutex1;
pthread_mutex_t mutex2; //figure out naming

int ack[ARR_SIZE];

bool done = false, all_sent = false;

int main(int argc, char *argv[]) {
    int udp_sock;
    socklen_t addrLen;
    struct sockaddr_in server;
    
    if((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(server_PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    int j = 1;

    printf("Binding UDP socket...\n");

    while (bind(udp_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
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

    int tcp_sock;

    if((tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }
    
    j = 1;

    printf("Binding TCP socket...\n");

    while (bind(tcp_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
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

    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&mutex2, NULL); //error checking
    
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



void *tcp_thread(void* sock) {
    printf("TCP: Start thread\n");

    struct sockaddr_in client;

    int tcp_sock = (int) (intptr_t) sock;
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);

    listen(tcp_sock, 1);
    
    printf("TCP: Waiting for incoming connections...\n");

    socklen_t addrlen = sizeof(client);

    client_sock = accept(tcp_sock, (struct sockaddr *) &client, (socklen_t *) &addrlen);

    printf("TCP: Connection received from %s at port %d \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    pthread_mutex_lock(&mutex1);
    pthread_mutex_unlock(&mutex1);

    while (!done) {
        if (all_sent) {
            pthread_mutex_lock(&mutex2);
            printf("TCP: Blocking mutex2 in TCP thread\n");

            printf("TCP: Sending all_sent message to client\n");
            send(client_sock, &all_sent, sizeof(bool), 0);

            all_sent = false;

            printf("TCP: Attempting receive of ack array from client\n");
            recv(client_sock, ack, ARR_SIZE * sizeof(ack[0]), 0);
            printf("TCP: Ack array received from client\n");

            pthread_mutex_unlock(&mutex2);
            printf("TCP: Mutex2 freed in TCP thread\n");
        }
    }
}

void *udp_thread(void* sock) {
    printf("UDP: Start thread\n");

    struct sockaddr_in client;
    
    pthread_mutex_lock(&mutex1);

    printf("UDP: Blocking mutex1\n");

    socklen_t addrLen;
    int udp_sock = (int) (intptr_t) sock;
    printf("UDP: Waiting for response...\n");

    char temp[12];

    addrLen = sizeof(client);

    if (recvfrom(udp_sock, temp, sizeof(temp), 0, (struct sockaddr *) &client, &addrLen) < 0) { //receive initialization message from client
        printf("UDP: Error receiving message from client");
        exit(1);
    }

    printf("UDP: Initialization message received from %s at port %d \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    // create array of integers to send

    int arr[ARR_SIZE];

    for (int i = 0; i < ARR_SIZE; i++) {
        arr[i] = i;
    }

    memset(ack, 0, ARR_SIZE * sizeof(ack[0]));
    
    pthread_mutex_unlock(&mutex1);

    printf("UDP: Mutex1 freed from UDP thread\n");
    
    while(!done) {
        pthread_mutex_lock(&mutex2);
        
        printf("UDP: Blocking mutex2 in UDP thread\n");
        
        printf("UDP: Sending data to client...\n");

        bool done_sending = true;

        for (int i = 0; i < ARR_SIZE; i++) {
            if(!ack[i]) {
                done_sending = false; //continue into loop body to send
            } else {
                continue;
            }

            struct msg m_msg;
            
            m_msg.chunkNum = i;
            m_msg.val = arr[i];
            
            if (sendto(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen) < 0) {
                perror("UDP: A message was not sent correctly");
            }
        }

        all_sent = true;

        if (done_sending) {
            done = true;
        }

        pthread_mutex_unlock(&mutex2);

        printf("UDP: Mutex2 freed in UDP thread\n");
    }
}