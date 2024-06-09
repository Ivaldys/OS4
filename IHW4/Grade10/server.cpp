#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define DATABASE_SIZE 40
#define MAX_MONITOR_CLIENTS 5
#define BROADCAST_PORT 9999

#define MAX_CLIENTS 100


struct sockaddr_in client_addresses[MAX_CLIENTS];
int num_clients = 0;


void add_client_address(struct sockaddr_in client_addr) {
    if (num_clients < MAX_CLIENTS) {
        client_addresses[num_clients++] = client_addr;
    }
}



int database[DATABASE_SIZE];
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_fd;

void send_broadcast_message(const char *message) {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Broadcast socket creation failed");
        return;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1) {
        perror("setsockopt (SO_BROADCAST) failed");
        close(sockfd);
        return;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // Broadcast address

    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) == -1) {
        perror("Broadcast send failed");
    }

    close(sockfd);
}

void handle_client(struct sockaddr_in *client_addr, socklen_t client_len, char *buffer) {
    if (strncmp(buffer, "READ", 4) == 0) {
        int index = atoi(buffer + 5);
        int value;

        pthread_mutex_lock(&db_mutex);
        value = database[index];
        pthread_mutex_unlock(&db_mutex);

        char response[1024];
        snprintf(response, sizeof(response), "VALUE %d", value);
        sendto(server_fd, response, strlen(response), 0, (struct sockaddr *)client_addr, client_len);

        snprintf(response, sizeof(response), "READ VALUE %d FROM CELL WITH INDEX %d", value, index);
        send_broadcast_message(response);
    } else if (strncmp(buffer, "WRITE", 5) == 0) {
        int index, new_value;
        sscanf(buffer + 6, "%d %d", &index, &new_value);

        pthread_mutex_lock(&writer_mutex);
        pthread_mutex_lock(&db_mutex);
        database[index] = new_value;
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_unlock(&writer_mutex);

        char response[1024];
        snprintf(response, sizeof(response), "UPDATED");
        sendto(server_fd, response, strlen(response), 0, (struct sockaddr *)client_addr, client_len);

        snprintf(response, sizeof(response), "WRITE VALUE %d TO INDEX %d", new_value, index);
        send_broadcast_message(response);
    }
}


void signal_handler(int signal) {
    printf("Caught signal %d, terminating server...\n", signal);
    send_broadcast_message("SERVER_SHUTDOWN");

    for (int i = 0; i < num_clients; ++i) {
        sendto(server_fd, "SERVER_SHUTDOWN", strlen("SERVER_SHUTDOWN"), 0,
            (struct sockaddr *)&client_addresses[i], sizeof(client_addresses[i]));
    }


    close(server_fd);
    exit(0);
}


int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return -1;
    }

    const char *SERVER_IP = argv[1];
    int PORT = atoi(argv[2]);

    for (int i = 0; i < DATABASE_SIZE; ++i) {
        database[i] = i + 1;
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024];

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);

    printf("UDP Server listening on %s:%d\n", SERVER_IP, PORT);

    while (1) {
        int len = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_addr, &client_len);
        if (len > 0) {
            buffer[len] = '\0';
            handle_client(&client_addr, client_len, buffer);
            add_client_address(client_addr);
        }
    }

    close(server_fd);
    return 0;
}
