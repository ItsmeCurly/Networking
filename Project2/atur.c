#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LOCAL_IP "130.111.46.105"
#define PORT 80

struct sockaddr_in sa;

int main()
{
    int err;
    char my_buf[16384];

    err = sprintf(my_buf, "%s", "GET /~phillip.dickens/pubs.html HTTP/1.1\r\nHost: www.aturing.umcs.maine.edu\r\n\r\n");
    int my_sock = socket(AF_INET, SOCK_STREAM, 0);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    
    inet_pton(AF_INET, LOCAL_IP, &(sa.sin_addr));
    
    int x = connect(my_sock, (struct sockaddr *)&sa, sizeof(sa));
    if (x != 0)
    {
        perror("Connect");
        exit(0);
    }

    err = send(my_sock, my_buf, 4096, 0);
    printf("Sent %d bytes to server\n", err);
    err = recv(my_sock, my_buf, 16384, MSG_WAITALL);
    printf("Received %d bytes from server!!!\n", err);

    printf("GOT THIS FROM Server: \n\n%s\n", my_buf);
}
