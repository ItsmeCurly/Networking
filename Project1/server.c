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
#include <math.h>

#define server_IP "10.0.2.15" //130.111.46.105 10.0.2.15
#define server_PORT 45022

#define NUM_BIND_TRIES 15
#define MESSAGE_SIZE 4096

void *tcp_thread(void *ptr);
void *udp_thread(void *ptr);

struct msg {
    int chunkNum;
    char val[MESSAGE_SIZE];
};

pthread_mutex_t mutex1;
pthread_mutex_t mutex2;
pthread_mutex_t mutex3;
pthread_mutex_t mutex4;

int* ack;

FILE *fp;
char* file_name = "BitMap.txt";
int file_size;
int sections;

bool done = false, all_sent = false;

int main(int argc, char *argv[]) {
    //setup udp socket
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

    //setup tcp socket

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

    //file stuff
    
    fp = fopen(file_name, "r");

    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);   //communicate size to other side
    rewind(fp);

    if(fp == NULL) {
        printf("File null\n");
    }

    //initialize mutexes

    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&mutex2, NULL); //error checking
    pthread_mutex_init(&mutex3, NULL);
    pthread_mutex_init(&mutex4, NULL);
    
    //initialize and start threads

    pthread_t udp, tcp;
    int rc;

    pthread_mutex_lock(&mutex4);
    printf("Main: Mutex4 locked\n");

    rc = pthread_create(&udp, NULL, udp_thread, (void*) (intptr_t) udp_sock);

    if(rc) {
        perror("UDP failed to start\n");
    }

    sleep(.01);
    rc = pthread_create(&tcp, NULL, tcp_thread, (void*) (intptr_t) tcp_sock);

    printf("Main: Mutex4 unlocked\n");
    pthread_mutex_unlock(&mutex4);

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

    pthread_mutex_lock(&mutex3);

    printf("TCP: Mutex3 locked\n");
    
    printf("TCP: Waiting for incoming connections...\n");

    listen(tcp_sock, 1);

    socklen_t addrlen = sizeof(client);

    client_sock = accept(tcp_sock, (struct sockaddr *) &client, (socklen_t *) &addrlen);

    printf("TCP: Connection received from %s at port %d \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    pthread_mutex_lock(&mutex1);
    printf("TCP: Mutex1 locked and unlocked\n");
    pthread_mutex_unlock(&mutex1);

    int send_size = send(client_sock, &file_size, sizeof(file_size), 0);

    printf("TCP: Sent file size to client: %d bytes\n", file_size);

    float _sections = file_size/MESSAGE_SIZE;

    if (!roundf(_sections) == _sections) {    //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;

    ack = malloc((int)sections * sizeof(int));

    // printf("Sections: %d, _sections: %f\n", sections, _sections);

    printf("TCP: Mutex3 unlocked\n");

    pthread_mutex_unlock(&mutex3);

    while (!done) {
        if (all_sent) {
            pthread_mutex_lock(&mutex2);
            printf("TCP: Mutex2 locked\n");

            printf("TCP: Sending all_sent message to client\n");
            send(client_sock, &all_sent, sizeof(bool), 0);

            all_sent = false;

            printf("TCP: Attempting receive of ack array from client\n");

            int eval = recv(client_sock, ack, sections * sizeof(ack[0]), MSG_WAITALL);
            printf("Ack array size: %d\n", eval);

            printf("TCP: Ack array received from client\n");

            printf("TCP: Mutex2 unlocked\n");
            pthread_mutex_unlock(&mutex2);
        }
    }
}

void *udp_thread(void* sock) {
    printf("UDP: Start thread\n");

    struct sockaddr_in client;
    
    pthread_mutex_lock(&mutex1);
    printf("UDP: Mutex1 locked\n");

    int udp_sock = (int) (intptr_t) sock;

    char temp[12];
    socklen_t addrLen = sizeof(client);

    printf("UDP: Waiting for response...\n");

    if (recvfrom(udp_sock, temp, sizeof(temp), 0, (struct sockaddr *) &client, &addrLen) < 0) { //receive initialization message from client
        printf("UDP: Error receiving message from client");
        exit(1);
    }

    printf("UDP: Initialization message received from %s at port %d \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    printf("UDP: Mutex1 unlocked\n");
    
    pthread_mutex_unlock(&mutex1);

    pthread_mutex_lock(&mutex3);
    printf("UDP: Mutex3 locked and unlocked\n");
    pthread_mutex_unlock(&mutex3);

    //can work with ack array after this

    memset(ack, 0, sections * sizeof(ack[0]));

    printf("Sections: %d\n", sections);
    
    while(1) {
        pthread_mutex_lock(&mutex2);
        
        printf("UDP: Mutex2 locked\n");
        
        printf("UDP: Sending data to client...\n");

        bool all_received = true;
        
        printf("UDP: Enter send loop\n");

        for (int i = 0; i < sections; i++) {
            if(!ack[i]) {
                all_received = false; //continue into loop body to send
            } else {
                continue;
            }

            struct msg m_msg;
            
            m_msg.chunkNum = i;

            fseek(fp, i * MESSAGE_SIZE, SEEK_SET);
            fread(m_msg.val, MESSAGE_SIZE, sizeof(char), fp);
            
            if (sendto(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen) < 0) {
                perror("UDP: A message was not sent correctly");
            }

            printf("Sent message %d\n", i);
        }
        all_sent = true;

        if (all_received) {
            done = true;
            pthread_mutex_unlock(&mutex2);

            pthread_mutex_destroy(&mutex1);
            pthread_mutex_destroy(&mutex2);

            sleep(2);   //sleep to let tcp thread finish, it needs this to flush the sending/receiving of the acknowledgement array

            printf("Transferral: All done\n");
            exit(1);
        }

        printf("UDP: Mutex2 unlocked\n");
        pthread_mutex_unlock(&mutex2);

        sleep(.001);
    }
}