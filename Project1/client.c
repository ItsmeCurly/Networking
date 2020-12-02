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
#include <math.h>
#include <time.h>

#define client_IP "10.0.2.15"
#define client_PORT 45023

#define server_IP "10.0.2.15"
// #define server_IP "130.111.46.105"
#define server_PORT 45022

#define NUM_BIND_TRIES 5
#define MESSAGE_SIZE 4096

#define NANOSECONDS_PER_SECOND 1E9

void *tcp_thread_sack(void *ptr);
void *udp_thread_sack(void *ptr);
void *tcp_thread_nack(void *ptr);
void *udp_thread_nack(void *ptr);
bool check_sack();
int count_sack();
bool check_nack();

bool DEBUG = false, TIME = false;

enum ack_t
{
    negative = 0,
    selective = 1
} ack_type = {selective};

const struct setting_s
{
    enum ack_t ack_type;
    bool DEBUG;
    bool TIME;
} default_settings = {selective, true, true};

typedef struct setting_s settings;

struct msg
{
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

struct sockaddr_in server;

settings _settings;

pthread_mutex_t mutex1;
pthread_mutex_t mutex2; //figure out naming

bool all_sent = false, done = false;

int file_size, sections;

bool udp_ready = false, tcp_ready = false, client_receiving = false;

struct _ack {
    int* sack;
    int* nack;
} ack;

FILE* outfp;
char* file_name = "new.txt";

int main(int argc, char *argv[])
{

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
                printf("0\n");
            }
            else if (strcmp(argv[i + 1], "sel") == 0)
            {
                ack_type = selective;
                printf("1\n");
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

    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    //set socket to nonblocking mode

    int flags = fcntl(udp_sock, F_GETFL, 0);
    fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
    perror("SET NOBLOCK");

    int j = 1;

    printf("Binding UDP socket...\n");

    while (bind(udp_sock, (struct sockaddr *)&client, sizeof(client)) < 0)
    {
        if (j == 1)
        {
            perror("Error: Bind failed");
        }
        else
        {
            char *temp = "Attempt";
            char *m_b = malloc(strlen(temp) + 8);

            sprintf(m_b, "%s #%d", temp, j);

            perror(m_b);
        }
        sleep(2); //attempt to bind sometimes fails, set it so that it waits 2 seconds after every failed bind, up to 5 attempts

        if (j >= NUM_BIND_TRIES)
        {
            exit(0);
        }
        j += 1;
    }

    printf("UDP Bind completed\n");

    //create and bind tcp socket

    int tcp_sock;

    if ((tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    outfp = fopen(file_name, "w+");

    //initialize mutexes

    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&mutex2, NULL);

    //initialize and start threads

    pthread_t udp, tcp;
    int rc;

    //start udp thread and debug
    if (_settings.ack_type == selective)
    {
        rc = pthread_create(&udp, NULL, udp_thread_sack, (void *)(intptr_t)udp_sock);
    }
    else
    {
        rc = pthread_create(&udp, NULL, udp_thread_nack, (void *)(intptr_t)udp_sock);
    }

    if (rc)
    {
        perror("UDP failed to start\n");
    }

    //start tcp thread and debug
    if (_settings.ack_type == selective)
    {
        rc = pthread_create(&tcp, NULL, tcp_thread_sack, (void *)(intptr_t)tcp_sock);
    }
    else
    {
        rc = pthread_create(&tcp, NULL, tcp_thread_nack, (void *)(intptr_t)tcp_sock);
    }

    if (rc)
    {
        perror("TCP failed to start\n");
    }

    pthread_exit(NULL);
}

void *tcp_thread_nack(void *sock)
{
    printf("TCP: Start thread\n");
    int tcp_sock = (int)(intptr_t)sock;

    pthread_mutex_lock(&mutex1);
    printf("TCP: Mutex1 locked\n");

    printf("TCP: Client connecting...\n");

    int x = connect(tcp_sock, (struct sockaddr *)&server, sizeof(server));

    printf("TCP: Successfully connected with %s at port %d\n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    int sett_send = send(tcp_sock, &_settings, sizeof(settings), 0);

    if (sett_send < 0)
    {
        perror("TCP: Settings send");
    }
    else
    {
        printf("TCP: Sent settings to server with size %d. Expected size: %ld\n", sett_send, sizeof(settings));
    }
    bool mirrored_settings;
    int ms_recv = recv(tcp_sock, &mirrored_settings, sizeof(bool), MSG_WAITALL);

    if (!mirrored_settings) {
        printf("Mismatched ACK types, exiting...\n");
        sleep(2);
        exit(0);
    }

    int fs_recv = recv(tcp_sock, &file_size, sizeof(int), MSG_WAITALL);

    if (fs_recv < 0)
    {
        perror("TCP: File size receive");
    }
    else
    {
        printf("TCP: Received file size from client with size %d. Expected size: %ld\n", fs_recv, sizeof(file_size));
    }

    if (DEBUG)
    {
        printf("TCP: File size from server: %d bytes\n", file_size);
    }

    //use file_size to create ack array and determine sectioning of file

    float _sections = file_size/(float)MESSAGE_SIZE;

    _sections = ceilf(_sections);

    sections = (int)_sections;

    ack.nack = malloc((int)sections * sizeof(int));

    for(int i = 0; i < sections; i++) {
        ack.nack[i] = i;
    }

    printf("TCP: Mutex1 unlocked\n");
    pthread_mutex_unlock(&mutex1);

    tcp_ready = true;

    while(!udp_ready) {
        sched_yield();
    }

    send(tcp_sock, &tcp_ready, sizeof(bool), 0);

    printf("Client threads initialized\n");

    while (!done)
    {
        printf("TCP: Waiting on all_sent message\n");
        
        int as_recv = recv(tcp_sock, &all_sent, sizeof(bool), MSG_WAITALL);

        if (as_recv < 0)
        {
            perror("TCP: All sent receive\n");
        }
        else
        {
            printf("TCP: Received all_sent message from server\n");
        }
        if (DEBUG)
        {
            printf("TCP: All sent size from server: %d bytes | Expected: %ld\n", as_recv, sizeof(bool));
            printf("TCP: All sent value: %d | Expected: %d\n", all_sent, true);
        }

        for (int i = 0; i < sections; i++) {
            ack.nack[i] = -1;
        }

        int index = 0;

        for (int i = 0; i < sections; i++) {
            if(!ack.sack[i]) {
                ack.nack[index++] = i;
            }
        }

        printf("Values remaining: %d/%d\n", count_sack(), sections);

        if(DEBUG) {
            for (int i = 0; i < sections; i++) {
                printf("%d ", ack.nack[i]);
            }
            printf("\n");
        }

        printf("TCP: Sending back acknowledgement array\n");

        // send(tcp_sock, &ack.nack->size, sizeof(ack.nack->size), 0);
        // send(tcp_sock, &ack.nack->top_index, sizeof(ack.nack->top_index), 0);
        send(tcp_sock, ack.nack, sections * sizeof(int), 0);

        printf("TCP: Ack array sent\n");
        
        printf("TCP: Mutex2 unlocked\n");
        pthread_mutex_unlock(&mutex2);
    }
}

void *udp_thread_nack(void *sock)
{
    printf("UDP: Start thread\n");
    int udp_sock = (int)(intptr_t)sock;

    socklen_t slen = sizeof(server); //initialize sock address len
    char buf[12];

    printf("UDP: Sending initialization message to server...\n");

    if (sendto(udp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&server, slen) < 0)    //send initialization message to server
    { 
        printf("UDP: Error sending message to server");
        exit(1);
    }

    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);

    printf("UDP: Message succesfully sent to %s at %d port \n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    pthread_mutex_lock(&mutex1);
    printf("UDP: Mutex1 locked and unlocked\n");
    pthread_mutex_unlock(&mutex1);

    //set ack array depending on number of sections the file is broken up into

    // ack.nack = malloc((int)sections * sizeof(int));
    // memset(ack.sack, 0, sections * sizeof(ack.sack[0]));

    ack.sack = malloc((int)sections * sizeof(int));
    memset(ack.sack, 0, sections * sizeof(ack.sack[0]));

    int index = 0, attempts = 0;

    struct msg m_msg, prev_msg; //use prev_msg in case messages are sent before values are assigned
    prev_msg.chunkNum = -1;     //if the previous value discovered is sent, then just ignore

    int previously_received[sections];
    int received[sections];
    int received_array_index = 0;

    udp_ready = true;

    while (1)
    {
        printf("UDP: Begin receive loop\n");

        for (int i = 0; i < sections; i++)
        { //reset received array
            received[i] = -1;
        }

        while (1)
        {
            if (all_sent)
            {
                printf("UDP: End receive loop\n");

                // pthread_mutex_lock(&mutex2);
                // printf("UDP: Mutex2 locked and unlocked\n");
                // pthread_mutex_unlock(&mutex2);

                // printf("UDP: Nack array size: %d/%d\n", (ack.nack->top_index)+1, sections);

                printf("UDP: Received %d values\n", received_array_index);

                printf("UDP: Values received: ");
                for (int k = 0; k < received_array_index; k++)
                {
                    if (received[k] != -1)
                    {
                        printf("%d ", received[k]);
                    }
                }
                printf("\n");
                
                received_array_index = 0;

                all_sent = false;
                attempts += 1;
                break;
            }

            slen = sizeof(struct sockaddr_in);

            recvfrom(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *)&server, &slen);

            if (prev_msg.chunkNum == m_msg.chunkNum || ack.sack[m_msg.chunkNum])
            {                                                               //check if value is previous message received
                continue;                                                   //or already in the ack array, if so, continue
            }

            if(DEBUG) {
                printf("Received %d\n", m_msg.chunkNum);
            }

            ack.sack[m_msg.chunkNum] = 1;

            fseek(outfp, m_msg.chunkNum * MESSAGE_SIZE, SEEK_SET);
            fwrite(m_msg.val, sizeof(char), MESSAGE_SIZE, outfp);

            received[received_array_index] = m_msg.chunkNum;
            received_array_index += 1;

            prev_msg = m_msg;
        }

        bool all_received = check_sack();

        if (all_received)
        {
            struct timespec end;
            clock_gettime(CLOCK_REALTIME, &end);

            double execute_time = (end.tv_sec + end.tv_nsec / NANOSECONDS_PER_SECOND) - (start.tv_sec + start.tv_nsec / NANOSECONDS_PER_SECOND);

            if(TIME) {
                printf("NACK transferral took %f seconds on the server side\n", execute_time);
            }

            done = true;
            printf("Transferral: All done. Attempts made at sending: %d\n", attempts);
            fflush(stdout);

            fclose(outfp);

            pthread_mutex_destroy(&mutex1);
            pthread_mutex_destroy(&mutex2);

            exit(1);
        }
    }
}

void *tcp_thread_sack(void *sock)
{
    printf("TCP: Start thread\n");
    int tcp_sock = (int)(intptr_t)sock;

    pthread_mutex_lock(&mutex1);
    printf("TCP: Mutex1 locked\n");

    printf("TCP: Client connecting...\n");

    int x = connect(tcp_sock, (struct sockaddr *)&server, sizeof(server));

    printf("TCP: Successfully connected with %s at port %d\n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    int sett_send = send(tcp_sock, &_settings, sizeof(settings), 0);

    if (sett_send < 0)
    {
        perror("TCP: Settings send\n");
    }
    else
    {
        printf("TCP: Sent settings to server with size %d. Expected size: %ld\n", sett_send, sizeof(settings));
    }

    bool mirrored_settings;
    int ms_recv = recv(tcp_sock, &mirrored_settings, sizeof(bool), MSG_WAITALL);

    if (!mirrored_settings) {
        printf("Mismatched ACK types, exiting...\n");
        sleep(2);
        exit(0);
    }

    int fs_recv = recv(tcp_sock, &file_size, sizeof(int), MSG_WAITALL);

    if (fs_recv < 0)
    {
        perror("TCP: File size receive");
    }
    else
    {
        printf("TCP: Received file size from server with size %d. Expected size: %ld\n", fs_recv, sizeof(int));
    }

    if (DEBUG)
    {
        printf("TCP: File size from server: %d bytes", file_size);
    }

    //use file_size to create ack array and determine sectioning of file

    float _sections = file_size/(float)MESSAGE_SIZE;

    _sections = ceilf(_sections);

    sections = (int)_sections;
    ack.sack = malloc((int)sections * sizeof(int));

    printf("TCP: Mutex1 unlocked\n");
    pthread_mutex_unlock(&mutex1);

    while (!done)
    {
        printf("TCP: Waiting on all_sent message\n");
        recv(tcp_sock, &all_sent, sizeof(bool), MSG_WAITALL);

        printf("TCP: All_sent message received\n");

        printf("TCP: Sending back acknowledgement array\n");

        if ((send(tcp_sock, ack.sack, sections * sizeof(ack.sack[0]), 0)) < 0)
        {
            perror("Ack send");
        }

        printf("TCP: Ack array sent\n");

        bool all_received = check_sack();

        if (all_received)
        {
            done = true;
        }
    }
}

void *udp_thread_sack(void *sock)
{
    printf("UDP: Start thread\n");
    int udp_sock = (int)(intptr_t)sock;

    socklen_t slen = sizeof(server); //initialize sock address len
    char buf[12];

    printf("UDP: Sending initialization message to server...\n");

    FILE* out_fp = fopen("new.txt", "w");

    if (sendto(udp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&server, slen) < 0)
    { //send initialization message to server
        printf("UDP: Error sending message to server");
        exit(1);
    }
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);

    printf("UDP: Message succesfully sent to %s at %d port \n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    pthread_mutex_lock(&mutex1);
    printf("UDP: Mutex1 locked and unlocked\n");
    pthread_mutex_unlock(&mutex1);

    //set ack array depending on number of sections the file is broken up into

    memset(ack.sack, 0, sections * sizeof(ack.sack[0]));

    int index = 0, attempts = 0;

    struct msg m_msg, prev_msg; //use prev_msg in case messages are sent before values are assigned
    prev_msg.chunkNum = -1;     //if the previous value discovered is sent, then just ignore

    int received[sections];
    int received_array_index = 0;

    while (1)
    {
        printf("UDP: Begin receive loop\n");

        for (int i = 0; i < sections; i++)
        { //reset received array
            received[i] = -1;
        }
        
        while (1)
        {
            if (all_sent)
            {
                break;
            }

            slen = sizeof(struct sockaddr_in);

            // printf("%ld", sizeof(m_msg

            recvfrom(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *)&server, &slen);

            // printf("%d\n%s\n", m_msg.chunkNum, m_msg.val);

            if (prev_msg.chunkNum == m_msg.chunkNum || ack.sack[m_msg.chunkNum])
            {             //check if value is previous message received
                continue; //or already in the ack array, if so, continue
            }

            if(m_msg.chunkNum == -1) {
                continue;
            }

            //printf("Received %d\n", m_msg.chunkNum);

            ack.sack[m_msg.chunkNum] = 1;

            // printf("Writing %d\n", m_msg.chunkNum);

            fseek(outfp, m_msg.chunkNum * MESSAGE_SIZE, SEEK_SET);
            fwrite(m_msg.val, sizeof(char), MESSAGE_SIZE, outfp);

            received[received_array_index] = m_msg.chunkNum;
            received_array_index += 1;

            prev_msg = m_msg;
        }

        printf("UDP: End receive loop\n");

        printf("UDP: Received %d values\n", received_array_index);

        printf("UDP: Received: ");
        for (int k = 0; k < sizeof(received) / sizeof(int); k++)
        {
            if (received[k] != -1)
            {
                printf("%d ", received[k]);
            }
        }
        printf("\n");

        printf("UDP: New ack array size: %d/%d\n", count_sack(), sections);

        received_array_index = 0;

        all_sent = false;
        attempts += 1;

        bool all_received = check_sack();

        if (all_received)
        {
            struct timespec end;
            clock_gettime(CLOCK_REALTIME, &end);

            double execute_time = (end.tv_sec + end.tv_nsec / NANOSECONDS_PER_SECOND) - (start.tv_sec + start.tv_nsec / NANOSECONDS_PER_SECOND);

            if(TIME) {
                printf("SACK transferral took %f seconds on the server side\n", execute_time);
            }

            done = true;
            printf("Transferral: All done. Attempts made at sending: %d\n", attempts);
            fflush(stdout);

            printf("closing\n");
            fclose(outfp);

            pthread_mutex_destroy(&mutex1);
            pthread_mutex_destroy(&mutex2);

            exit(1);
        }
    }
}

bool check_sack()
{
    bool all_received = true;

    for (int i = 0; i < sections; i++)
    {
        if (!ack.sack[i])
        {
            all_received = false;
        }
    }

    return all_received;
}

int count_sack()
{
    int received = 0;

    for (int i = 0; i < sections; i++)
    {
        if (ack.sack[i])
        {
            received += 1;
        }
    }

    return received;
}

// bool check_nack()
// {
//     return is_empty(ack.nack);
// }

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
            change_size(stack, 40);
        } else return;
    }

    stack->arr[++stack->top_index] = val;
    if(DEBUG) {
        printf("%d pushed to stack\n", val);
    }
}

void change_size(stack* stack, int amt_inc) {
    printf("Change size: %d %d\n", stack->size, amt_inc);
    if(stack->size + amt_inc < 0) {
        perror("Cannot set size less than 0\n");
        return;
    }
    printf("1\n");
    unsigned int new_size = stack->size += amt_inc;
    printf("2\n");

    int* new_arr = (int*) malloc(new_size * sizeof(int));
    printf("3\n");

    for(int i = 0; i < new_size; i++) {
        new_arr[i] = -1;
    }
    printf("4\n");

    for(int i = 0; i < stack->size; i++) {
        new_arr[i] = stack->arr[i];
    }
    printf("5\n");

    free(stack->arr);
    printf("6\n");

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
        if(DEBUG) {
            printf("i: %d\n", i);
            printf("arr[i]: %d\n", stack->arr[i]);
        }
        if (val == stack->arr[i]) {
            return i;
        }
    }
    return -1;
}

bool contains(stack* stack, int val) {
    return (get_index(stack, val) >= 0);
}

void clear(stack* stack) {
    free(stack->arr);

    stack->arr = (int*) malloc(stack->size * sizeof(int));
    stack->top_index = -1;
}