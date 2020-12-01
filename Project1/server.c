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
#include <time.h>

// #define server_IP "10.0.2.15"
#define server_IP "130.111.46.105"
#define server_PORT 45022

#define NUM_BIND_TRIES 15
#define MESSAGE_SIZE 4096

void *tcp_thread_sack(void *ptr);
void *udp_thread_sack(void *ptr);
void *tcp_thread_nack(void *ptr);
void *udp_thread_nack(void *ptr);
bool check_nack();

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

struct _stack {
    int top_index;
    unsigned int size;
    int* arr;
};

typedef struct _stack stack;

//stack functions
stack* create_stack(unsigned int size);
int is_full(stack* stack);
int is_empty(stack* stack);
void push(stack* stack, int val, bool allow_increase);
int pop(stack* stack);
int peek(stack* stack);
void clear(stack* stack);
bool contains(stack* stack, int val);
void change_size(stack* stack, int amt_inc);
void set_size(stack* stack, int new_size);
int get_index(stack* stack, int val);

pthread_mutex_t mutex1;
pthread_mutex_t mutex2;
pthread_mutex_t mutex3;
pthread_mutex_t mutex4;

struct _ack {
    int* sack;
    stack* nack;
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

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-acktype") == 0)
        {
            if (strcmp(argv[i + 1], "neg") == 0)
            {
                ack_type = negative;
            }
            else if (strcmp(argv[i + 1], "sel") == 0)
            {
                ack_type = selective;
            }
        }
        else if (strcmp(argv[i], "-debug") == 0) {
            DEBUG = true;
        }
        else if (strcmp(argv[i], "-time") == 0) {
            TIME = true;
        }
    }

    _settings = (settings){ack_type, DEBUG, TIME};

    if(DEBUG) {
        printf("DEBUG: Ack type: %d\n", ack_type);
    }

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

        if(DEBUG) {
            printf("Started udp_thread_sack\n");
        }
    } else {
        rc = pthread_create(&udp, NULL, udp_thread_nack, (void*) (intptr_t) udp_sock);

        if(DEBUG) {
            printf("Started udp_thread_nack\n");
        }
    }

    if(rc) {
        perror("UDP failed to start\n");
    }
    else {
        printf("Main: UDP thread started\n");
    }

    sleep(.01);

    if(_settings.ack_type == selective) {
        rc = pthread_create(&tcp, NULL, tcp_thread_sack, (void*) (intptr_t) tcp_sock);

        if(DEBUG) {
            printf("Started tcp_thread_sack\n");
        }
    } else {
        rc = pthread_create(&tcp, NULL, tcp_thread_nack, (void*) (intptr_t) tcp_sock);

        if(DEBUG) {
            printf("Started tcp_thread_nack\n");
        }
    }

    printf("Main: Mutex4 unlocked\n");
    pthread_mutex_unlock(&mutex4);

    if(rc) {
        perror("TCP failed to start\n");
    }
    else {
        printf("Main: TCP thread started\n");
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

    bool mirrored_settings;

    if(local_settings.ack_type != _settings.ack_type) {
        mirrored_settings = false;
        send(client_sock, &mirrored_settings, sizeof(bool), 0);
        
        printf("Mismatched ACK types, exiting...\n");

        sleep(2);
        exit(0);
    } else {
        mirrored_settings = true;
        send(client_sock, &mirrored_settings, sizeof(bool), 0);
    }

    //send file size over to client

    int fs_send = send(client_sock, &file_size, sizeof(file_size), 0);

    if(fs_send < 0) {
        perror("TCP: File size send\n");
    } else {
        printf("TCP: Sent file size to client with size %d. Expected size: %ld\n", fs_send, sizeof(file_size));
    }

    if(DEBUG) {
        printf("TCP: Sent file size to client: %d bytes\n", file_size);
    }

    float _sections = file_size/MESSAGE_SIZE;

    if ((!roundf(_sections)) == _sections) {    //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;

    stack* nack = create_stack(100);
    ack.nack = nack;    //initialize nack stack

    for(int i = 0; i < sections; i++) {
        push(ack.nack, i, true);
    }

    if(DEBUG){
        printf("Sections: %d, _sections: %f\n", sections, _sections);
    }

    printf("TCP: Mutex3 unlocked\n");

    pthread_mutex_unlock(&mutex3);

    bool temp;

    int rdy_recv = recv(client_sock, &temp, sizeof(bool), 0);

    printf("Client is ready\n");

    while (!done) {
        if (all_sent) {
            pthread_mutex_lock(&mutex2);
            printf("TCP: Mutex2 locked\n");

            printf("TCP: Sending all_sent message to client\n");
            send(client_sock, &all_sent, sizeof(bool), 0);

            all_sent = false;

            printf("TCP: Attempting receive of NACK array from client\n");

            int size_recv = recv(client_sock, &ack.nack->size, sizeof(ack.nack->size), 0);
            if (size_recv < 0)
            {
                perror("TCP: Size receive");
                exit(0);
            }
            else
            {
                printf("TCP: Received NACK size from client\n");
            }
            if (DEBUG)
            {
                printf("TCP: NACK size %d | Expected size: %ld\n", size_recv, sizeof(ack.nack->size));
            }

            int top_recv = recv(client_sock, &ack.nack->top_index, sizeof(ack.nack->top_index), 0);
            if (top_recv < 0)
            {
                perror("TCP: top_index receive");
                exit(0);
            }
            else
            {
                printf("TCP: Received NACK top_index from client\n");
            }
            if (DEBUG)
            {
                printf("TCP: top_index size %d | Expected size: %ld\n", top_recv, sizeof(ack.nack->top_index));
            }

            int arr_recv = recv(client_sock, ack.nack->arr, ack.nack->size * sizeof(int), 0);
            
            if (arr_recv < 0)
            {
                perror("TCP: NACK arr receive");
                exit(0);
            }
            else
            {
                printf("TCP: Received NACK arr from client\n");
            }
            if (DEBUG)
            {
                printf("TCP: NACK arr size %d | Expected size: %ld\n", arr_recv, ack.nack->size * sizeof(int));
            }

            if(DEBUG) {
                printf("Values remaining: ");
                for(int i = 0; i <= ack.nack->top_index; i++) {
                    printf("%d ", ack.nack->arr[i]);
                }
                printf("\n");
            }
            

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

    clock_t start = clock();

    printf("UDP: Initialization message received from %s at port %d \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    printf("UDP: Mutex1 unlocked\n");
    
    pthread_mutex_unlock(&mutex1);

    pthread_mutex_lock(&mutex3);
    printf("UDP: Mutex3 locked and unlocked\n");
    pthread_mutex_unlock(&mutex3);

    //can work with ack array after this
    
    while(1) {
        pthread_mutex_lock(&mutex2);
        
        printf("UDP: Mutex2 locked\n");
        
        printf("UDP: Sending data to client...\n");

        bool all_received = true;
        
        printf("UDP: Enter send loop\n");
        
        // while(!is_empty(ack.nack)) {
        //     all_received = false;

        //     struct msg m_msg;
            
        //     m_msg.chunkNum = pop(ack.nack);

        //     fseek(fp, m_msg.chunkNum * MESSAGE_SIZE, SEEK_SET);
        //     fread(m_msg.val, MESSAGE_SIZE, sizeof(char), fp);
            
        //     int msg_send = sendto(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen);
        //     if(msg_send < 0) {
        //         perror("UDP: A message was not sent correctly");
        //     }
        // }
        // for(int i = 0; i < ack.nack->top_index; i++) {
        //     printf("%d ", ack.nack->arr[i]);
        // }

        // printf("\n");
        printf("%d\n", ack.nack->top_index);
        for (int i = 0; i <= ack.nack->top_index; i++) {
            all_received = false;

            struct msg m_msg;
            
            m_msg.chunkNum = ack.nack->arr[i];

            fseek(fp, m_msg.chunkNum * MESSAGE_SIZE, SEEK_SET);
            fread(m_msg.val, MESSAGE_SIZE, sizeof(char), fp);
            
            int msg_send = sendto(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen);
            if(msg_send < 0) {
                perror("UDP: A message was not sent correctly");
            }
            
            // if(DEBUG) {
            // }
        }
        all_sent = true;

        if (all_received) {
            done = true;
            pthread_mutex_unlock(&mutex2);

            pthread_mutex_destroy(&mutex1);
            pthread_mutex_destroy(&mutex2);

            clock_t end = clock();
            
            double execute_time = (end-start)/(float)CLOCKS_PER_SEC;

            if(TIME) {
                printf("NACK transferral took %f seconds on the server side\n", execute_time);
            }

            sleep(2);   //sleep to let tcp thread finish, it needs this to flush the sending/receiving of the acknowledgement array

            printf("Transferral: All done\n");
            exit(1);
        }

        printf("UDP: Mutex2 unlocked\n");
        pthread_mutex_unlock(&mutex2);

        while(all_sent) {
            sched_yield();
        }
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

    printf("Before mutex1\n");

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
    
    bool mirrored_settings;
    if(local_settings.ack_type != _settings.ack_type) {
        mirrored_settings = false;
        send(client_sock, &mirrored_settings, sizeof(bool), 0);
        printf("Mismatched ACK types, exiting...\n");
        sleep(2);
        exit(0);
    } else {
        mirrored_settings = true;
        send(client_sock, &mirrored_settings, sizeof(bool), 0);
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

    if ((!roundf(_sections)) == _sections) {    //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;
    ack.sack = malloc((int)sections * sizeof(int));

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

            int eval = recv(client_sock, ack.sack, sections * sizeof(ack.sack[0]), MSG_WAITALL);
            printf("TCP: Ack array size: %d\n", eval);

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
    clock_t start = clock();

    printf("UDP: Initialization message received from %s at port %d \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    printf("UDP: Mutex1 unlocked\n");
    
    pthread_mutex_unlock(&mutex1);

    pthread_mutex_lock(&mutex3);
    printf("UDP: Mutex3 locked and unlocked\n");
    pthread_mutex_unlock(&mutex3);

    //can work with ack array after this

    memset(ack.sack, 0, sections * sizeof(ack.sack[0]));
    
    while(1) {
        pthread_mutex_lock(&mutex2);
        
        printf("UDP: Mutex2 locked\n");
        
        printf("UDP: Sending data to client...\n");

        bool all_received = true;
        
        printf("UDP: Enter send loop\n");

        for (int i = 0; i < sections; i++) {
            if(!ack.sack[i]) {
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
            clock_t end = clock();

            double execute_time = (end-start)/(float)CLOCKS_PER_SEC;

            if(TIME) {
                printf("SACK transferral took %f seconds on the server side\n", execute_time);
            }

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

        // sleep(.001);

        while(all_sent) {
            sched_yield();
        }
    }
}

stack* create_stack(unsigned int size){
    stack* new_stack = (stack*) malloc(sizeof(stack*));

    new_stack->size = size;
    new_stack->top_index = -1;
    new_stack->arr = (int*) malloc(new_stack->size * sizeof(int));

    return new_stack;
}

int is_full(stack* stack) {
    return stack->top_index == stack->size - 1;
}

int is_empty(stack* stack) {
    return stack->top_index == -1;
}

void push(stack* stack, int val, bool allow_increase){
    if(is_full(stack)) {
        if(allow_increase) {
            change_size(stack, 20);
        } else return;
    }

    stack->arr[++stack->top_index] = val;
    // if(DEBUG) {
    //     printf("%d pushed to stack\n", val);
    // }
}

void change_size(stack* stack, int amt_inc) {
    if(stack->size + amt_inc < 0) {
        perror("Cannot set size less than 0\n");
        return;
    }

    unsigned int new_size = stack->size += amt_inc;

    int* new_arr = (int*) malloc(new_size * sizeof(int));

    for(int i = 0; i < stack->size; i++) {
        new_arr[i] = stack->arr[i];
    }

    stack->arr = new_arr;
    stack->size = new_size;
}

void set_size(stack* stack, int new_size) {
    change_size(stack, new_size - stack->size);
}

int pop(stack* stack) {
    if (is_empty(stack)) {
        return -1;
    }
    return stack->arr[stack->top_index--];
}

int peek(stack* stack) {
    if (is_empty(stack)) {
        return -1;
    }
    return stack->arr[stack->top_index];
}

// I know this isn't a standard for stacks, but 
// I need it for the implementation that I'm going for
int get_index(stack* stack, int val) {
    for(int i = 0; i < stack->size; i++) {
        if (val == stack->arr[i]) {
            return i;
        }
    }
    return -1;
}

bool contains(stack* stack, int val) {
    return get_index(stack, val) >= 0;
}

void clear(stack* stack) {
    free(stack->arr);

    stack->arr = (int*) malloc(stack->size * sizeof(int));
    stack->top_index = -1;
}