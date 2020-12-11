#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define LOCAL_IP "192.168.144.136"
#define PORT 10059
#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 1048576

void *handle_comm(void *);
char *trim(char *str);
void log_info(char* msg);

pthread_t tid[15];

FILE *f;

bool DEBUG = 1;

struct thread_args
{
    int client_sock;
    int pthread_id;
};

int main(int argc, char *argv[])
{

    int pthread_id = 0;
    int proxy_fd, client_fd;
    struct sockaddr_in server, client;

    proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
    client_fd = socket(AF_INET, SOCK_STREAM, 0);

    printf("Sockets created\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, LOCAL_IP, &(server.sin_addr));

    if (bind(proxy_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Bind failed. Error");
        return 1;
    }
    printf("Bind completed \n");

    f = fopen("proxy.log", "a+");

    if (f == NULL) {
        printf("Could not open the log file\n");
        perror("Log file open\n");

        return 1;
    }

    int err = fprintf(f, "Begin logging session\n\n");

    if (err < 0) {
        printf("Error writing to file\n");
    }

    fflush(f);

    int c = sizeof(struct sockaddr_in);

    listen(proxy_fd, 15);

    while (1)
    {
        printf("Waiting for incoming connections...\n\n");
        fflush(stdout);

        client_fd = accept(proxy_fd, (struct sockaddr *)&client, (socklen_t *)&c);
        if (client_fd < 0)
        {
            perror("accept failed");
            return 1;
        }

        printf("Connection accepted... Creating thread to handle communication\n\n");
        fflush(stdout);

        struct thread_args *ta = malloc(sizeof(struct thread_args));

        ta->pthread_id = pthread_id;
        ta->client_sock = client_fd;

        pthread_create(&(tid[pthread_id]), NULL, handle_comm, ta);

        pthread_id++;
        if (pthread_id == 15)
            pthread_id = 0;
    }
}

void *handle_comm(void *thread_args)
{
#define ta ((struct thread_args *)thread_args)

    char client_message[MAX_REQUEST_SIZE];
    int recv_size;

    // printf("%d\n", client_sock);

    recv_size = recv(ta->client_sock, client_message, MAX_REQUEST_SIZE, 0);

    // printf("Received %d bytes. Msg is:\n\n%s \n",
    //        recv_size, client_message);

    fflush(stdout);

    char *temp = calloc(strlen(client_message) + 1, sizeof(char));
    char *temp2 = calloc(strlen(client_message) + 1, sizeof(char));

    strcpy(temp, client_message);
    strcpy(temp2, client_message);

    char *request_line = strtok(temp2, "\n");

    log_info(request_line);

    char *request = strtok(temp, " ");

    if (strcmp(request, "GET") != 0)
    {
        printf("Got a %s request, exiting thread %d\n", request, ta->pthread_id);
        pthread_exit(NULL);
    }
    else
    {
        printf("Got a GET request\n");
    }

    //--------------------------
    // parse host name
    //--------------------------

    char *temp_host, *host_name;

    strtok(NULL, "\n"); //end of header

    while (1)
    {
        temp_host = strtok(NULL, " ");  //Host:
        host_name = strtok(NULL, "\n"); //host name
        if (strcmp(temp_host, "Host:") == 0)
        {
            break;
        }
    }

    printf("Thread %d: %s\n", ta->pthread_id, host_name);

    //--------------------------
    // end parse host name
    //--------------------------

    // free(temp);
    // free(temp_host);

    struct addrinfo *servinfo, *rp;
    struct addrinfo hints;

    char *port = "80";

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // printf("before trim string is %s\n", host_name);
    // printf("length before: %ld\n", strlen(host_name)); //should be 10

    host_name = trim(host_name);

    // if (DEBUG)
    // {
    //     printf("after trim string is %s\n", host_name);
    //     printf("length after: %ld\n", strlen(host_name)); //should be 8
    // }

    int status = getaddrinfo(host_name, port, &hints, &servinfo);

    if (DEBUG)
    {
        printf("Address Info Status: %d\n", status);
    }

    printf("IP addresses for %s:\n\n", host_name);

    //code got from https://beej.us/guide/bgnet/examples/showip.c

    char ipstr[INET6_ADDRSTRLEN];

    for(rp = servinfo; rp != NULL; rp = rp->ai_next) {
		void *addr;
		char *ipver;

		// get the pointer to the address itself,
		// different fields in IPv4 and IPv6:
		if (rp->ai_family == AF_INET) { // IPv4
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
			addr = &(ipv4->sin_addr);
			ipver = "IPv4";
		} else { // IPv6
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
			addr = &(ipv6->sin6_addr);
			ipver = "IPv6";
		}

		// convert the IP to a string and print it:
		inet_ntop(rp->ai_family, addr, ipstr, sizeof ipstr);
		printf("%s: %s\n", ipver, ipstr);
	}

    freeaddrinfo(servinfo); // free the linked list

    struct sockaddr_in server_sa;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    server_sa.sin_family = AF_INET;
    server_sa.sin_port = htons(80);
    inet_pton(AF_INET, ipstr, &(server_sa.sin_addr));

    int err_connect = connect(server_fd, (struct sockaddr *)&server_sa, sizeof(server_sa));
    if (err_connect != 0)
    {
        perror("Connect");
        exit(0);
    }

    printf("Thread %d: Connected successfully to %s at port %d \n", ta->pthread_id, inet_ntoa(server_sa.sin_addr), htons(server_sa.sin_port));

    int send_server = send(server_fd, &client_message, sizeof(client_message), 0);
    printf("Thread %d: Sent %d bytes to server\n", ta->pthread_id, send_server);

    char buf[MAX_RESPONSE_SIZE];

    int recv_server = recv(server_fd, buf, sizeof(buf), MSG_WAITALL);
    printf("Thread %d: Received %d bytes from server!\n", ta->pthread_id, recv_server);

    send(ta->client_sock, buf, sizeof(buf), 0);

    printf("Thread %d: Sent data back to client\n", ta->pthread_id);

    // printf("Thread Exiting!\n");
    fflush(stdout);

    pthread_exit(NULL);
}

char *trim(char *str)
{
    char *end;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        end--;
    }
    *(++end) = '\0';
    return str;
}

void log_info(char* message) {
    struct tm *tm;
    time_t t;
    char str_time[100];
    char str_date[100];

    t = time(NULL);
    tm = localtime(&t);

    strftime(str_time, sizeof(str_time), "%H:%M:%S", tm);
    strftime(str_date, sizeof(str_date), "%Y/%m/%d", tm);
    fprintf(f, "%s - %s: %s\n", str_date, str_time, message);

    fflush(f);
}