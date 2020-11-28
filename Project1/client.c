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

#define client_IP "10.0.2.15"
#define client_PORT 45023

#define server_IP "10.0.2.15"
#define server_PORT 45022

#define NUM_BIND_TRIES 5
#define MESSAGE_SIZE 4096

void *tcp_thread(void *ptr);
void *udp_thread(void *ptr);
bool check_ack();
int check_ack2();

//stack functions
stackptr create_stack(unsigned int size);
int is_full(stackptr stack);
int is_empty(stackptr stack);
void push(stackptr stack, int val);
int pop(stackptr stack);
int peek(stackptr stack);

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
    char *val;
};

struct stack {
    int top;
    unsigned int size;
    int* arr;
};

typedef struct stack* stackptr;

struct sockaddr_in server;

settings _settings;

pthread_mutex_t mutex1;
pthread_mutex_t mutex2; //figure out naming

bool all_sent = false, done = false;

int file_size, sections;

union _ack {
    int* sack;
    struct linked_list nack;
} ack;

int main(int argc, char *argv[])
{

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
        else if (strcmp(argv[i], "-debug"))
        {
            DEBUG = true;
        }
        else if (strcmp(argv[i], "-time"))
        {
            TIME = true;
        }
    }

    _settings = (settings){ack_type, DEBUG, TIME};

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
        rc = pthread_create(&udp, NULL, tcp_thread_sack, (void *)(intptr_t)udp_sock);
    }
    else
    {
        rc = pthread_create(&udp, NULL, tcp_thread_nack, (void *)(intptr_t)udp_sock);
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
        perror("TCP: Settings send\n");
    }
    else
    {
        printf("TCP: Sent settings to server with size %d. Expected size: %ld\n", sett_send, sizeof(settings));
    }

    int fs_recv = recv(tcp_sock, &file_size, sizeof(int), MSG_WAITALL);

    if (fs_recv < 0)
    {
        perror("TCP: Settings receive\n");
    }
    else
    {
        printf("TCP: Received file size from client with size %d. Expected size: %ld\n", fs_recv, sizeof(settings));
    }

    if (DEBUG)
    {
        printf("TCP: File size from server: %d bytes", file_size);
    }

    //use file_size to create ack array and determine sectioning of file

    float _sections = file_size / MESSAGE_SIZE;

    if (!roundf(_sections) == _sections)
    { //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;

    struct stack nack;
    // nack.head = NULL;
    nack.size = 0;

    ack.nack = nack;    //initialize nack linked list

    printf("TCP: Mutex1 unlocked\n");
    pthread_mutex_unlock(&mutex1);

    while (!done)
    {
        printf("TCP: Waiting on all_sent message\n");
        recv(tcp_sock, &all_sent, sizeof(bool), MSG_WAITALL);

        printf("TCP: All_sent message received\n");

        printf("TCP: Sending back acknowledgement array\n");

        if ((send(tcp_sock, ack, sections * sizeof(ack[0]), 0)) < 0) //TODO: Change to NACK
        {
            perror("Ack send");
        }

        printf("TCP: Ack array sent\n");

        bool all_received = check_ack(); //TODO: Change to NACK

        if (all_received)
        {
            done = true;
        }
    }
}

void *udp_thread_nack(void *sock)
{
    printf("UDP: Start thread\n");
    int udp_sock = (int)(intptr_t)sock;

    socklen_t slen = sizeof(server); //initialize sock address len
    char buf[12];

    printf("UDP: Sending initialization message to server...\n");

    if (sendto(udp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&server, slen) < 0)
    { //send initialization message to server
        printf("UDP: Error sending message to server");
        exit(1);
    }

    printf("UDP: Message succesfully sent to %s at %d port \n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    pthread_mutex_lock(&mutex1);
    printf("UDP: Mutex1 locked and unlocked\n");
    pthread_mutex_unlock(&mutex1);

    //set ack array depending on number of sections the file is broken up into

    memset(ack, 0, sections * sizeof(ack[0])); //TODO: Change to NACK

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

            recvfrom(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *)&server, &slen);

            if (prev_msg.chunkNum == m_msg.chunkNum || ack[m_msg.chunkNum]) //TODO: Change to NACK
            {                                                               //check if value is previous message received
                continue;                                                   //or already in the ack array, if so, continue
            }

            //printf("Received %d\n", m_msg.chunkNum);

            ack[m_msg.chunkNum] = 1; //TODO: Change to NACK

            received[received_array_index] = m_msg.chunkNum;
            received_array_index += 1;

            prev_msg = m_msg;
        }

        printf("UDP: End receive loop\n");

        printf("UDP: Ack array size: %d/%d\n", check_ack2(), sections); //TODO: Change to NACK

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

        received_array_index = 0;

        all_sent = false;
        attempts += 1;

        bool all_received = check_ack(); //TODO: Change to NACK

        if (all_received)
        {
            done = true;
            printf("Transferral: All done. Attempts made at sending: %d\n", attempts);
            fflush(stdout);

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

    int fs_recv = recv(tcp_sock, &file_size, sizeof(int), MSG_WAITALL);

    if (fs_recv < 0)
    {
        perror("TCP: Settings receive\n");
    }
    else
    {
        printf("TCP: Received file size from client with size %d. Expected size: %ld\n", fs_recv, sizeof(settings));
    }

    if (DEBUG)
    {
        printf("TCP: File size from server: %d bytes", file_size);
    }

    //use file_size to create ack array and determine sectioning of file

    float _sections = file_size / MESSAGE_SIZE;

    if (!roundf(_sections) == _sections)
    { //possibly check for tolerance value here
        _sections = ceilf(_sections);
    }

    sections = (int)_sections;

    ack = malloc((int)sections * sizeof(int));

    printf("TCP: Mutex1 unlocked\n");
    pthread_mutex_unlock(&mutex1);

    while (!done)
    {
        printf("TCP: Waiting on all_sent message\n");
        recv(tcp_sock, &all_sent, sizeof(bool), MSG_WAITALL);

        printf("TCP: All_sent message received\n");

        printf("TCP: Sending back acknowledgement array\n");

        if ((send(tcp_sock, ack, sections * sizeof(ack[0]), 0)) < 0)
        {
            perror("Ack send");
        }

        printf("TCP: Ack array sent\n");

        bool all_received = check_ack();

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

    if (sendto(udp_sock, buf, sizeof(buf), 0, (struct sockaddr *)&server, slen) < 0)
    { //send initialization message to server
        printf("UDP: Error sending message to server");
        exit(1);
    }

    printf("UDP: Message succesfully sent to %s at %d port \n", inet_ntoa(server.sin_addr), htons(server.sin_port));

    pthread_mutex_lock(&mutex1);
    printf("UDP: Mutex1 locked and unlocked\n");
    pthread_mutex_unlock(&mutex1);

    //set ack array depending on number of sections the file is broken up into

    memset(ack, 0, sections * sizeof(ack[0]));

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

            recvfrom(udp_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *)&server, &slen);

            if (prev_msg.chunkNum == m_msg.chunkNum || ack[m_msg.chunkNum])
            {             //check if value is previous message received
                continue; //or already in the ack array, if so, continue
            }

            //printf("Received %d\n", m_msg.chunkNum);

            ack[m_msg.chunkNum] = 1;

            received[received_array_index] = m_msg.chunkNum;
            received_array_index += 1;

            prev_msg = m_msg;
        }

        printf("UDP: End receive loop\n");

        printf("UDP: Ack array size: %d/%d\n", check_ack2(), sections);

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

        received_array_index = 0;

        all_sent = false;
        attempts += 1;

        bool all_received = check_ack();

        if (all_received)
        {
            done = true;
            printf("Transferral: All done. Attempts made at sending: %d\n", attempts);
            fflush(stdout);

            pthread_mutex_destroy(&mutex1);
            pthread_mutex_destroy(&mutex2);

            exit(1);
        }
    }
}

bool check_ack()
{
    bool all_received = true;

    for (int i = 0; i < sections; i++)
    {
        if (!ack[i])
        {
            all_received = false;
        }
    }

    return all_received;
}

int check_ack2()
{
    int received = 0;

    for (int i = 0; i < sections; i++)
    {
        if (ack[i])
        {
            received += 1;
        }
    }

    return received;
}

stackptr create_stack(unsigned int size){
    stackptr stack = (stackptr) malloc(sizeof(stackptr));

    stack->size = size;
    stack->top = -1;
    stack->arr = (int*) malloc(stack->size * sizeof(int));

    return stack;
}

int is_full(stackptr stack) {
    return stack->top == stack->size - 1;
}

int is_empty(stackptr stack) {
    return stack->top == -1;
}

void push(stackptr stack, int val){
    if(is_full(stack)) return;

    stack->arr[++stack->top] = val;
    if(DEBUG) {
        printf("%d pushed to stack\n", val);
    }
}

int pop(stackptr stack) {

}

int peek(stackptr stack) {

}