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

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define PORT 5005

#define P2P_BYE 0
#define P2P_FILE_REQUEST 1
#define P2P_PEER_LIST 2
#define P2P_FILE_FOUND 3
#define P2P_FILE_NOT_FOUND 4
#define P2P_ERR_NO_PEERS 10

typedef struct {
    int cmd;
    int body_size;
    char body[1024];
} message_t;

message_t *create_message(int cmd, char *body) {
    message_t *msg = (message_t *) malloc(sizeof(message_t));
    msg->cmd = cmd;
    msg->body_size = strlen(body);
    strcpy(msg->body, body);
    return msg;
}

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
    int connection_fds[MAX_CLIENTS];
    struct sockaddr_in connection_addrs[MAX_CLIENTS];
} thread_data_t;

thread_data_t thread_data;

int *find_seeders_for_file(char *file_name) {
    int *seeders = malloc(sizeof(int) * MAX_CLIENTS);
    int no_seeders = 0;

    printf("[SERVER] looking for seeders...\n");

    message_t *req_msg = create_message(P2P_FILE_REQUEST, file_name);
    message_t *res_msg = (message_t *) malloc(sizeof(message_t));
    for (int i = 0; i < thread_data.connected_clients; i++) {
        printf("[SERVER] sending request to %d\n", thread_data.connection_fds[i]);

        // send file request to client
        if (write(thread_data.connection_fds[i], (void *)req_msg, sizeof(message_t)) < 0) {
            perror("WRITE ERROR");
            exit(1);
        }
        // wait for response from client
        if (read(thread_data.connection_fds[i], (void *)res_msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        if (res_msg->cmd == P2P_FILE_FOUND) {
            printf("[SERVER] %d - found file\n", thread_data.connection_fds[i]);
            seeders[no_seeders] = thread_data.connection_fds[i];
            no_seeders++;
        } else if (res_msg->cmd == P2P_FILE_NOT_FOUND) {
            printf("[SERVER] %d - no file found\n", thread_data.connection_fds[i]);
        }
    }

    return seeders;
}

void push_client(int fd, struct sockaddr_in client_addr) {
    thread_data.connection_fds[thread_data.connected_clients] = fd;
    thread_data.connection_addrs[thread_data.connected_clients] = client_addr;
    thread_data.connected_clients++;

    printf("[SERVER] client connected: %d (%d / %d)\n", fd, thread_data.connected_clients, MAX_CLIENTS);
}

void pop_client(int fd) {
    thread_data.connected_clients--;
    printf("[SERVER] client disconnected: %d (%d / %d)\n", fd, thread_data.connected_clients, MAX_CLIENTS);
}

void* server_process(void* arg) {
    thread_proc_args_t* args = (thread_proc_args_t*) arg;
    int fd = args->sockfd;
    
    push_client(fd, args->client_addr);

    int run = 1;
    do {
        char *buffer = (char *)malloc(BUFFER_SIZE);
        int bytes_read;

        memset(buffer, 0, BUFFER_SIZE);

        if ((bytes_read = read(fd, buffer, BUFFER_SIZE)) < 0) {
            perror("READ ERROR");
            exit(1);
        }
        message_t msg = *(message_t *)buffer;

        switch (msg.cmd) {
            case P2P_FILE_REQUEST:
                printf("[SERVER] file request: %s\n", msg.body);
                int *seeders = find_seeders_for_file(msg.body);

                if (seeders == NULL) {
                    msg.cmd = P2P_ERR_NO_PEERS;
                    msg.body_size = 0;
                } else {
                    msg.cmd = P2P_PEER_LIST;
                    msg.body_size = sizeof(int) * MAX_CLIENTS;
                    memcpy(msg.body, seeders, msg.body_size);
                }

                if (write(fd, (void *) &msg, sizeof(msg)) < 0) {
                    perror("WRITE ERROR");
                    exit(1);
                }
                break;
            
            case P2P_BYE:
                printf("[SERVER] client disconnected: %d\n", fd);
                run = 0;
                break;

        }
    } while (run);

    close(fd);

    pop_client(fd);

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

        // Create a new thread to handle the client
        thread_proc_args_t args;
        args.sockfd = connfd;
        args.client_addr = client_addr;
        if (pthread_create(&tid[i], NULL, &server_process, (void *) &args) != 0) {
            perror("THREAD ERROR");
            exit(1);
        }
        i++;
    }

    close(sockfd);
}