#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h> // for sockaddr_in struct
#include <sys/socket.h>
#include <unistd.h> // for close
#include <dirent.h> // for opendir, closedir
#include <errno.h>
#include <string.h> // for memset
#include <arpa/inet.h> // for inet_addr
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_CLIENTS 10
#define PORT 5005

int connected_clients = 0;

struct sockaddr_in set_socket_addr(uint32_t inaddr, short sin_port) {
    struct sockaddr_in addr;
    memset((void*) &addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(inaddr);
    addr.sin_port = htons(sin_port);

    return addr;
}

typedef struct {
    int sockfd;
    struct sockaddr_in client_addr;
} thread_proc_args_t;

typedef struct {
    int connected_clients;
} thread_data_t;

thread_data_t thread_data;

void* server_process(void* arg) {
    thread_proc_args_t* args = (thread_proc_args_t*) arg;
    int fd = args->sockfd;

    thread_data.connected_clients++;
    printf("[SERVER] client connected: %d (%d / %d)\n", fd, thread_data.connected_clients, MAX_CLIENTS);

    do {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        int bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read == 0) {
            break;
        }

        printf("[SERVER] received: %d\n", bytes_read);
        break;
    } while (1);

    close(fd);

    thread_data.connected_clients--;
    printf("[SERVER] client disconnected (%d / %d)\n", thread_data.connected_clients, MAX_CLIENTS);

    return NULL;
}

int main() {
    int sockfd, connfd;
    struct sockaddr_in server_addr = set_socket_addr(INADDR_ANY, PORT);
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    printf("[SERVER] starting server...\n");

    // Create a socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SOCKET ERROR");
        exit(1);
    }

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("BIND ERROR");
        exit(1);
    }

    if (listen(sockfd, MAX_CLIENTS) < 0) {
        perror("LISTEN ERROR");
        exit(1);
    }

    printf("[SERVER] server started\n");

    pthread_t tid[MAX_CLIENTS];
    int i = 0;

    for (;;) {
        // Accept a connection
        if ((connfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
            perror("ACCEPT ERROR");
            exit(1);
        }

        printf("[SERVER] client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Read from the socket
        thread_proc_args_t args;
        args.sockfd = connfd;
        args.client_addr = client_addr;
        if (pthread_create(&tid[i], NULL, &server_process, (void *) &args) != 0) {
            perror("THREAD ERROR");
            exit(1);
        }
        i++;

        close(connfd);
    }
}