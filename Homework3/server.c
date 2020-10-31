#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#define s_IP "127.0.0.1"
#define s_PORT 45022

char* concat(const char*, const char*);

int main(int argc, char *argv[]) {
    int m_sock;
    socklen_t addrLen;
    struct sockaddr_in server, client;
    
    if((m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(s_PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    //inet_pton(AF_INET, s_IP, &(server.sin_addr));

    #define NUM_TRIES 5
    int i = 1;

    printf("Binding...\n");

    while (bind(m_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        if (i == 1) {
            perror("Error: Bind failed");
        }
        else {
            char* temp = "Attempt";
            char* m_b = malloc(strlen(temp) + 8);

            sprintf(m_b, "%s #%d", temp, i);
            
            perror(m_b);
        }
        sleep(2);   //attempt to bind sometimes fails, set it so that it waits 2 seconds after every failed bind, up to 5 attempts

        if (i >= NUM_TRIES) {
            exit(0);
        }
        i+=1;
    }

    printf("Bind completed\n");

    printf("Waiting for response...\n");

    char temp[12];

    addrLen = sizeof(client);

    if (recvfrom(m_sock, temp, sizeof(temp), 0, (struct sockaddr *) &client, &addrLen) < 0) { //receive initialization message from client
        printf("Error receiving message from client");
        exit(1);
    }

    free(temp);

    printf("Message received from %s at %d port \n", inet_ntoa(client.sin_addr), htons(client.sin_port));

    //create array of integers to send

    #define ARR_SIZE 10000

    int arr[ARR_SIZE];

    for (int i = 0; i < ARR_SIZE; i++) {
        arr[i] = i;
    }

    printf("Sending data to client...\n");
    char s1[8], s2[8], *buf;

    for (int i = 0; i < ARR_SIZE; i++) {
        sprintf(s1, "%d", i);
        sprintf(s2, "%d", arr[i]);

        buf = concat(s1, s2);
        
        if (sendto(m_sock, buf, sizeof(buf), 0, (struct sockaddr *) &client, addrLen) < 0) {
            perror("A message was not sent correctly");
        } //following the specified sendto format
    }

    printf("Data sent successfully\n");

    close(m_sock);
}

char* concat(const char *s1, const char *s2) {
    char *result = malloc(strlen(s1) + strlen(s2) + 1);

    strcpy(result, s1);
    strcat(result, s2);

    return result;
}