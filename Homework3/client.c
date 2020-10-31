#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#define cl_IP "0.0.0.0"
#define cl_PORT 45022

#define s_IP "0.0.0.0"
#define s_PORT 45022

int main(int argc, char *argv[]) {
    int m_sock;
    char buf[12];
    struct sockaddr_in m_addr, server_addr;
    int num_bytes;

    int slen = sizeof(struct sockaddr_in);

    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error: socket creation failed\n");
        exit(0);
    }
    

    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(cl_PORT);

    inet_pton(AF_INET, cl_IP, &(m_addr.sin_addr)); //bind client ip

    printf("Binding...\n");
    
    #define NUM_TRIES 5
    int i = 0;

    while(bind(m_sock, (struct sockaddr *)&m_addr, sizeof(m_addr)) < 0) {
        if (i == 0) {
            perror("Error: Bind failed\n");
        }
        else {
            char* temp = "Attempt";
            char* m_b = malloc(strlen(temp) + 8);

            sprintf(m_b, "%s #%d\n", temp, i);
            
            perror(m_b);
        }
        sleep(2);

        if (i >= NUM_TRIES) {
            exit(0);
        }
    }

    printf("Bind completed\n");
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(s_PORT);

    inet_pton(AF_INET, s_IP, &(server_addr.sin_addr)); //server ip

    sendto(m_sock, buf, recv_len, 0, (struct sockaddr*) &server_addr, slen); //send initialization message to server

    #define ARR_SIZE 10000

    int index = 0;

    int arr[ARR_SIZE];
    memset(arr, -1, ARR_SIZE * sizeof(arr[0]));

    int ack[ARR_SIZE];
    memset(ack, 0, ARR_SIZE * sizeof(ack[0]));

    int received = 0;

    while(1) {
        recv_size = recvfrom(m_sock, buf, sizeof(buf), 0, (struct sockaddr *) &server_addr, &slen);

        if (recv_size < 2) {
            perror("Receive size less than expected\n");
        }

        //not too sure if I should check if the data is fragmented or not, that is, check for garbage before accessing it

        index = atoi(buf[0]);
        
        arr[index] = atoi(buf[1]);
        ack[index] = 1;

        received += 1;

        if (received >= 3000) {
            printf("Received 3000 messages, breaking");
            break;
        }
    }
}