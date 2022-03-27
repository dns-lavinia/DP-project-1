#include <arpa/inet.h>  // for inet_addr
#include <dirent.h>     // for opendir, closedir
#include <errno.h>
#include <netinet/in.h>  // for sockaddr_in struct
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // for memset
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  // for close

#include "../lib/netio.h"
#include "../lib/protocol.h"
#include "../lib/logger.h"

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define PORT 5006

typedef struct {
    int connected_clients;
    int connection_fds[MAX_CLIENTS];
    struct sockaddr_in connection_addrs[MAX_CLIENTS];
} thread_data_t;

thread_data_t thread_data;

typedef struct {
    int *ports;
    int num_ports;
} seeder_t;

seeder_t find_seeders_for_file(char *file_name, int client_fd) {
    int seeder_ports[MAX_CLIENTS];
    int num_ports = 0;

    logger(LOG_SERVER, "looking for seeders...");

    message_t req_msg = create_message(P2P_FILE_REQUEST, file_name, strlen(file_name) + 1);
    message_t res_msg;

    for (int i = 0; i < thread_data.connected_clients; i++) {
        if ( client_fd == thread_data.connection_fds[i] ) {
            continue;
        }

        logger(LOG_SERVER, "sending request to %d", thread_data.connection_fds[i]);

        // send file request to client
        if (write(thread_data.connection_fds[i], &req_msg, sizeof(message_t)) < 0) {
            perror("WRITE ERROR");
            exit(1);
        }
        // wait for response from client
        if (read(thread_data.connection_fds[i], &res_msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        logger(LOG_PROTOCOL, msg_to_string(res_msg));

        if (res_msg.code == P2P_FILE_FOUND) {
            logger(LOG_SERVER, "%d - found file - port: %d", thread_data.connection_fds[i], *(int *)res_msg.body);

            seeder_ports[num_ports] = *(int *)res_msg.body;
            num_ports++;
        } else if (res_msg.code == P2P_FILE_NOT_FOUND) {
            logger(LOG_SERVER, "%d - no file found", thread_data.connection_fds[i]);
        }
    }

    seeder_t seeder_list = {
        .ports = seeder_ports,
        .num_ports = num_ports,
    };

    return seeder_list;
}

void push_client(int fd, struct sockaddr_in client_sockaddr) {
    thread_data.connection_fds[thread_data.connected_clients] = fd;
    thread_data.connection_addrs[thread_data.connected_clients] = client_sockaddr;
    thread_data.connected_clients++;

    logger(LOG_SERVER, "client connected: %d (%d / %d)", fd, thread_data.connected_clients, MAX_CLIENTS);
}

void pop_client(int fd) {
    thread_data.connected_clients--;
    logger(LOG_SERVER, "client disconnected: %d (%d / %d)", fd, thread_data.connected_clients, MAX_CLIENTS);
}

typedef struct {
    int client_fd;
    struct sockaddr_in client_sockaddr;
} thread_proc_args_t;

//* function: communicate_with_client
void* communicate_with_client(void* arg) {
    thread_proc_args_t* args = (thread_proc_args_t*) arg;
    int fd = args->client_fd;
    struct sockaddr_in client_sockaddr = args->client_sockaddr;

    push_client(fd, client_sockaddr);

    message_t msg;
    int run = 1;
    do {
        // Read message from client
        if (read(fd, &msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        logger(LOG_PROTOCOL, msg_to_string(msg));

        // Process message
        switch (msg.code) {
            case P2P_FILE_REQUEST:
                logger(LOG_SERVER, "file request: %s", msg.body);
                seeder_t seeders = find_seeders_for_file(msg.body, fd);

                if (seeders.num_ports == 0) {
                    logger(LOG_SERVER, "no seeders found");
                    msg = create_message(P2P_ERR_NO_PEERS, NULL, 0);
                } else {
                    logger(LOG_SERVER, "found %d seeders", seeders.num_ports);
                    msg = create_message(P2P_PEER_LIST, seeders.ports, seeders.num_ports * sizeof(int));
                }

                if (write(fd, &msg, sizeof(msg)) < 0) {
                    perror("WRITE ERROR");
                    exit(1);
                }
                break;
            
            case P2P_BYE:
            default:
                run = 0;
                break;
        }
    } while (run);

    close(fd);

    pop_client(fd);

    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_sockaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    logger(LOG_SERVER, "starting server...");

    // Server socket setup
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SOCKET ERROR");
        exit(1);
    }

    if (bind(server_fd, (struct sockaddr *) &server_sockaddr, sizeof(server_sockaddr)) < 0) {
        perror("BIND ERROR");
        exit(1);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("LISTEN ERROR");
        exit(1);
    }

    logger(LOG_SERVER, "server started: %s", print_sockaddr(server_sockaddr));

    // Concurrent server connection handling
    pthread_t tid[MAX_CLIENTS];
    int i = 0;

    int client_fd;
    struct sockaddr_in client_sockaddr;
    socklen_t client_sockaddr_len = sizeof(client_sockaddr);

    for (;;) {
        // Accept a connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_sockaddr, &client_sockaddr_len)) < 0) {
            perror("ACCEPT ERROR");
            continue;
        }

        // Create a new thread to handle the client
        thread_proc_args_t args = {
            .client_fd = client_fd,
            .client_sockaddr = client_sockaddr
        };
        if (pthread_create(&tid[i], NULL, &communicate_with_client, (void *) &args) != 0) {
            perror("THREAD ERROR");
            exit(1);
        }
        i++;
    }

    close(server_fd);
}