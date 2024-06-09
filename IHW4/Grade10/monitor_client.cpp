#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 1024
#define BROADCAST_PORT 9999

int client_socket;

void signal_handler(int signal) {
    printf("Caught signal %d, terminating monitor client...\n", signal);
    close(client_socket);
    exit(0);
}

int main() {
    struct sockaddr_in server_addr;

    if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket creation failed");
        return -1;
    }

    int reuse_addr = 1;
    if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1) {
        perror("setsockopt (SO_REUSEADDR) failed");
        close(client_socket);
        return -1;
    }

    int broadcast_enable = 1;
    if (setsockopt(client_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1) {
        perror("setsockopt (SO_BROADCAST) failed");
        close(client_socket);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(BROADCAST_PORT);

    if (bind(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(client_socket);
        return -1;
    }

    signal(SIGINT, signal_handler);

    printf("Monitor client listening for broadcasts on port %d\n", BROADCAST_PORT);

    char buffer[MAX_BUFFER_SIZE];
    int bytes_received;

    while (1) {
        bytes_received = recvfrom(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0, NULL, NULL);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            if (strcmp(buffer, "SERVER_SHUTDOWN") == 0) {
                printf("Received server shutdown signal. Terminating...\n");
                exit(0);
            }


            printf("SERVER LOG: %s\n", buffer);
        } else if (bytes_received == -1) {
            perror("Receive failed");
            break;
        }
    }

    close(client_socket);
    printf("Connection closed.\n");

    return 0;
}
