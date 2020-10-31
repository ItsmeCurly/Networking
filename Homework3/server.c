#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#define s_IP "0.0.0.0"
#define s_PORT 45022

int main(int argc, char *argv[]) {
    int m_sock, client_sock, slen;
    struct sockaddr_in sa, sa_client;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(2001);
    inet_pton(AF_INET, IP, &(sa.sin_addr));

    #define NUM_TRIES 5
    int i = 0;

    printf("Binding...")

    while(bind(m_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        if (i == 0) {
            perror("Error: Bind failed\n");
        }
        else {
            char* temp = "Attempt";
            char* m_b = malloc(strlen(temp) + 8);

            sprintf(m_b, "%s #%d\n", temp, i);
            
            perror(m_b);
        }
        sleep(2);   //attempt to bind sometimes fails, set it so that it waits 2 seconds after every failed bind, up to 5 attempts

        if (i >= NUM_TRIES) {
            exit(0);
        }
    }

    printf("Bind completed\n");

    printf("Waiting for response...\n");

    recv_len = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &sa_client, &slen); //receive initialization message from client

    //sendto(sock, buf, recv_len, 0 (struct sockaddr *)&sa_client, slen);

    #define ARR_SIZE 10000

    int arr[ARR_SIZE];

    for (int i = 0; i < ARR_SIZE; i++) {
        arr[i] = i;
    }

    char buf[2];

    for (int i = 0; i < ARR_SIZE; i++) {
        buf[0] = i, buf[1] = arr[i]; //set header to message #, content to the array value at nth position
        
        sendto(sock, buf, sizeof(buf), 0, (struct sockaddr *) &sa_client, slen); //following the specified sendto format
    }
}