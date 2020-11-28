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

bool DEBUG = false, TIME = false;

enum ack_t
{
    negative = 0,
    selective = 1
} ack_type;

const struct setting_s
{
    enum ack_t ack_type;
    bool DEBUG;
    bool TIME;
} default_settings = {selective, true, true};

typedef struct setting_s settings;

settings _settings;

struct msg {
    int chunkNum;
    char val[MESSAGE_SIZE];
};

struct stack {
    int top;
    unsigned int size;
    int* arr;
};

pthread_mutex_t mutex1;
pthread_mutex_t mutex2;
pthread_mutex_t mutex3;
pthread_mutex_t mutex4;

union _ack {
    int* sack;
    struct linked_list nack;
} ack;

FILE *fp;
char* file_name = "BitMap.txt";
int file_size;
int sections;

bool done = false, all_sent = false;

int main(int argc, char *argv[]) {
    //parse argv
    if (argc == 1)
    {
        printf("Program requires a specified acknowledgment type\nExiting...\n");
        sleep(1);
        exit(0);
    }

    for (int i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], "-acktype"))
        {
            if (strcmp(argv[i + 1], "neg"))
            {
                ack_type = negative;
            }
            else if (strcmp(argv[i + 1], "sel"))
            {
                ack_type = selective;
            }
        }
        else if (strcmp(argv[i], "-debug")) {
            DEBUG = true;
        }
        else if (strcmp(argv[i], "-time")) {
            TIME = true;
        }
    }

    _settings = (settings){ack_type, DEBUG, TIME};

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

    if(_settings.ack_type == selective) {
        rc = pthread_create(&udp, NULL, udp_thread_sack, (void*) (intptr_t) udp_sock);
    } else {
        rc = pthread_create(&udp, NULL, udp_thread_nack, (void*) (intptr_t) udp_sock);
    }
    
    rc = pthread_create(&udp, NULL, udp_thread, (void*) (intptr_t) udp_sock);

    if(rc) {
        perror("UDP failed to start\n");
    }

    sleep(.01);

    if(_settings.ack_type == selective) {
        rc = pthread_create(&udp, NULL, tcp_thread_sack, (void*) (intptr_t) udp_sock);
    } else {
        rc = pthread_create(&udp, NULL, tcp_thread_nack, (void*) (intptr_t) udp_sock);
    }

    printf("Main: Mutex4 unlocked\n");
    pthread_mutex_unlock(&mutex4);

    if(rc) {
        perror("TCP failed to start\n");
    }

    pthread_exit(NULL);
}

void *tcp_thread_nack(void* sock) {
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

    //get settings from client regarding ack type, keep debug and 
    //timing the same on from each side's specification

    settings local_settings;

    int sett_recv = recv(client_sock, &local_settings, sizeof(settings), MSG_WAITALL);

    if(sett_recv < 0) {
        perror("Settings receive\n");
    } else {
        printf("Received settings from client with size %d. Expected size: %ld\n", sett_recv, sizeof(settings));
    }

    // _settings.ack_type = local_settings.ack_type;

    //send file size over to client

    int fs_send = send(client_sock, &file_size, sizeof(file_size), 0);

    if(fs_send < 0) {
        perror("TCP: Settings send\n");
    } else {
        printf("TCP: Sent settings to server with size %d. Expected size: %ld\n", fs_send, sizeof(file_size));
    }

    if(DEBUG) {
        printf("TCP: Sent file size to client: %d bytes\n", file_size);
    }

    float _sections = file_size/MESSAGE_SIZE;

    if (!roundf(_sections) == _sections) {    //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;

    struct linked_list nack;
    // nack.head = NULL;
    nack.size = 0;

    ack.nack = nack;    //initialize nack linked list

    if(DEBUG){
        printf("Sections: %d, _sections: %f\n", sections, _sections);
    }

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

            int eval = recv(client_sock, &ack.nack, sizeof(ack.nack), MSG_WAITALL);  //TODO: change to NACK
            printf("Ack array size: %d\n", eval);

            printf("TCP: Ack array received from client\n");

            printf("TCP: Mutex2 unlocked\n");
            pthread_mutex_unlock(&mutex2);
        }
    }
}

void *udp_thread_nack(void* sock) {
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

    // struct node head

    // ack.nack = malloc
    
    while(1) {
        pthread_mutex_lock(&mutex2);
        
        printf("UDP: Mutex2 locked\n");
        
        printf("UDP: Sending data to client...\n");

        bool all_received = true;
        
        printf("UDP: Enter send loop\n");

        int loop_end;

        
        for (int i = 0; i < sections; i++) {
            if(!ack[i]) {   //TODO: change to NACK
                all_received = false; //continue into loop body to send
            } else {
                continue;
            }

            struct msg m_msg;
            
            m_msg.chunkNum = i;

            fseek(fp, i * MESSAGE_SIZE, SEEK_SET);
            fread(m_msg.val, MESSAGE_SIZE, sizeof(char), fp);
            
            int msg_send = sendto(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen);
            if(msg_send < 0) {
                perror("UDP: A message was not sent correctly");
            }
            
            if(DEBUG) {
                printf("UDP: Sent message %d with payload size %d\n", i, msg_send);
            }
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

void *tcp_thread_sack(void* sock) {
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

    //get settings from client regarding ack type, keep debug and 
    //timing the same on from each side's specification

    settings local_settings;

    int sett_recv = recv(client_sock, &local_settings, sizeof(settings), MSG_WAITALL);

    if(sett_recv < 0) {
        perror("Settings receive\n");
    } else {
        printf("Received settings from client with size %d. Expected size: %ld\n", sett_recv, sizeof(settings));
    }

    // _settings.ack_type = local_settings.ack_type;

    //send file size over to client

    int fs_send = send(client_sock, &file_size, sizeof(file_size), 0);

    if(fs_send < 0) {
        perror("TCP: Settings send\n");
    } else {
        printf("TCP: Sent settings to server with size %d. Expected size: %ld\n", fs_send, sizeof(file_size));
    }

    if(DEBUG) {
        printf("TCP: Sent file size to client: %d bytes\n", file_size);
    }

    float _sections = file_size/MESSAGE_SIZE;

    if (!roundf(_sections) == _sections) {    //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;
    ack = malloc((int)sections * sizeof(int));

    if(DEBUG){
        printf("Sections: %d, _sections: %f\n", sections, _sections);
    }

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

void *udp_thread_sack(void* sock) {
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
    
    while(1) {
        pthread_mutex_lock(&mutex2);
        
        printf("UDP: Mutex2 locked\n");
        
        printf("UDP: Sending data to client...\n");

        bool all_received = true;
        
        printf("UDP: Enter send loop\n");

        int loop_end;

        for (int i = 0; i < sections; i++) {
            if(_settings.ack_type == selective) {
                if(!ack[i]) {
                    all_received = false; //continue into loop body to send
                } else {
                    continue;
                }
            } else {

            }

            struct msg m_msg;
            
            m_msg.chunkNum = i;

            fseek(fp, i * MESSAGE_SIZE, SEEK_SET);
            fread(m_msg.val, MESSAGE_SIZE, sizeof(char), fp);
            
            int msg_send = sendto(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen);
            if(msg_send < 0) {
                perror("UDP: A message was not sent correctly");
            }
            
            if(DEBUG) {
                printf("UDP: Sent message %d with payload size %d\n", i, msg_send);
            }
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