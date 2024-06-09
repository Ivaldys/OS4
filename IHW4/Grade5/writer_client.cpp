#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define DATABASE_SIZE 40

typedef struct {
    int id;
    const char* server_ip;
    int port;
} WriterArgs;

pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;

void signal_handler(int signal) {
    printf("Caught signal %d, terminating writer clients...\n", signal);
    exit(0);
}

void* writer_task(void* arg) {
    WriterArgs* args = (WriterArgs*)arg;
    int id = args->id;
    const char* SERVER_IP = args->server_ip;
    int PORT = args->port;

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address / Address not supported\n");
        close(sock);
        return NULL;
    }

    while (1) {
        usleep((1000 + rand() % 5000) * 1000); // Sleep for random milliseconds
        
        pthread_mutex_lock(&rand_mutex);
        int index = rand() % DATABASE_SIZE;
        int new_value = rand() % 40;
        pthread_mutex_unlock(&rand_mutex);

        char request[1024];
        snprintf(request, sizeof(request), "WRITE %d %d", index, new_value);

        // Send the length of the request first
        int request_len = strlen(request);
        if (sendto(sock, &request_len, sizeof(request_len), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != sizeof(request_len)) {
            fprintf(stderr, "Error sending request length\n");
            break;
        }
        if (sendto(sock, request, request_len, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != request_len) {
            fprintf(stderr, "Error sending request\n");
            break;
        }

        // Since UDP is connectionless, there's no need to wait for a response
        printf("Writer %d: Index %d, New Value %d\n", id, index, new_value);
    }

    close(sock);
    printf("Writer %d finished.\n", id);
    return NULL;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <SERVER_IP> <PORT> <NUM_WRITERS>\n", argv[0]);
        return -1;
    }

    const char* SERVER_IP = argv[1];
    int PORT = atoi(argv[2]);
    int NUM_WRITERS = atoi(argv[3]);

    srand(time(NULL));

    signal(SIGINT, signal_handler);

    pthread_t writers[NUM_WRITERS];
    WriterArgs writer_args[NUM_WRITERS];
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writer_args[i].id = i + 1;
        writer_args[i].server_ip = SERVER_IP;
        writer_args[i].port = PORT;
        if (pthread_create(&writers[i], NULL, writer_task, &writer_args[i]) != 0) {
            fprintf(stderr, "Error creating writer thread\n");
            return -1;
        }
    }

    for (int i = 0; i < NUM_WRITERS; ++i) {
        pthread_join(writers[i], NULL);
    }

    return 0;
}
