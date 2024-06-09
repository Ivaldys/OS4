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

int database[DATABASE_SIZE];
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_fd;

void signal_handler(int signal) {
    printf("Caught signal %d, terminating server...\n", signal);
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

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);

    printf("Server listening on %s:%d\n", SERVER_IP, PORT);

    char buffer[1024];
    while (1) {
        int len_read = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len_read <= 0) {
            perror("recvfrom failed");
            continue;
        }

        buffer[len_read] = '\0';

        char *request = strdup(buffer);
        if (strncmp(request, "READ", 4) == 0) {
            printf("Received READ request from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            int index = atoi(request + 5);
            int value;

            pthread_mutex_lock(&db_mutex);
            value = database[index];
            pthread_mutex_unlock(&db_mutex);

            char response[1024];
            snprintf(response, sizeof(response), "VALUE %d", value);
            sendto(server_fd, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len);
        } else if (strncmp(request, "WRITE", 5) == 0) {
            printf("Received WRITE request from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            int index, new_value;
            sscanf(request + 6, "%d %d", &index, &new_value);

            pthread_mutex_lock(&writer_mutex);
            pthread_mutex_lock(&db_mutex);
            database[index] = new_value;
            pthread_mutex_unlock(&db_mutex);
            pthread_mutex_unlock(&writer_mutex);

            char *response = "UPDATED";
            sendto(server_fd, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len);
        }
        free(request);
    }

    close(server_fd);
    return 0;
}
