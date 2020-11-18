#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#define s_IP "130.111.46.105"
#define s_PORT 45022

char* concat(const char*, const char*);
void *tcp_thread(void *ptr);
void *udp_thread(void *ptr);

struct msg {
    int chunkNum;
    int val;
};

int main(int argc, char *argv[]) {
    int tcp_sock, udp_sock;
    socklen_t addrLen;
    struct sockaddr_in udp_server, tcp_server, udp_client, tcp_client;
    
    if((udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    if((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(s_PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    //inet_pton(AF_INET, s_IP, &(server.sin_addr));

    #define NUM_TRIES 5
    int j = 1;

    printf("Binding...\n");

    while (bind(m_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
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

        if (j >= NUM_TRIES) {
            exit(0);
        }
        j+=1;
    }

    printf("Bind completed\n");

    printf("Waiting for response...\n");

    char temp[12];

    addrLen = sizeof(client);

    if (recvfrom(m_sock, temp, sizeof(temp), 0, (struct sockaddr *) &client, &addrLen) < 0) { //receive initialization message from client
        printf("Error receiving message from client");
        exit(1);
    }

    printf("Initialization message received from %s at %d port \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    //create array of integers to send

    #define ARR_SIZE 10000

    int arr[ARR_SIZE];

    for (int i = 0; i < ARR_SIZE; i++) {
        arr[i] = i;
    }

    printf("Sending data to client...\n");

    for (int i = 0; i < ARR_SIZE; i++) {
        struct msg m_msg;
        
        m_msg.chunkNum = i;
        m_msg.val = arr[i];
        
        if (sendto(m_sock, &m_msg, sizeof(m_msg), 0, (struct sockaddr *) &client, addrLen) < 0) {
            perror("A message was not sent correctly");\
        }
    }

    printf("Data sent successfully\n");

    close(m_sock);
}



void *tcp_thread(void* ptr) {
    
}

void *udp_thread(void* ptr) {
    
}