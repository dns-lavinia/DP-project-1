#include <arpa/inet.h>  // for inet_addr
#include <dirent.h>     // for opendir, closedir
#include <errno.h>
#include <fcntl.h>       //for open file modes.
#include <netinet/in.h>  // for sockaddr_in struct
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // for memset
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  // for close
#include <sys/stat.h> // for fstat

#include "../lib/netio.h"
#include "../lib/protocol.h"

//==============================================================================
//================ Some general info that we can delete later ==================
//
// The peer acts both as a client and as a server
//
// Initially, a peer can be configured to have a list of known partners
//
// The user asks the (locally installed) peer to get a file -> after file is
// successfully transferred, it is stored in a directory chosen by the user
//
// The local peer contacts the other peers concurrently
//
// Protocol errors (file does not exist, wrong command) should be handled
// appropriately
//
// We should do health checks every x seconds
//
//==============================================================================

// Change this value later, maybe relate it somehow to the number of threads
// for a program
#define MAX_PEERS 10
#define SRV_PORT 5005
#define BUF_LEN 1024

// Global variable for peer-to-peer conection
int connection_vector[5];

typedef struct {
    int port;
} thread_data_t;

thread_data_t thread_data;

//----- act_as_seeder is used to facilitate other peers with a list of
//----- available files and send them to a peer as requested
void send_file(int leecher_fd, char *file_name, int total_seg, int current_seg) {
    char *file_path = malloc(strlen(file_name) + strlen("./res/") + 1);
    int fd;

    strcpy(file_path, "./res/");
    strcat(file_path, file_name);

    if ((fd = open(file_path, O_RDONLY)) < 0) {
        perror("OPEN ERROR");
        exit(1);
    }

    
    struct stat st;
    fstat(fd, &st);

    // total bytes to read
    int total_to_read = st.st_size;
    int seg_size = total_to_read / total_seg; // size of one segment to send
    int n_read = seg_size;
    int offset = current_seg * seg_size;

    // if the size of the file is smaller than BUF_LEN - 20, then change n_read
    if (n_read > (BUF_LEN - 20)) {
        n_read = BUF_LEN - 20;
    }

    char buffer[BUF_LEN - 20];
    int bytes_read;
    message_t msg;
    while ((bytes_read = stream_read(fd, buffer, n_read, offset)) > 0) {
        msg = create_message(P2P_FILE_DATA, buffer, bytes_read);
        write(leecher_fd, &msg, sizeof(msg));


        // keep track and update how much of the segment was read
        seg_size -= n_read;

        if(n_read > seg_size) {
            n_read = seg_size;
        }

        offset += bytes_read;

        if(offset == ((current_seg * seg_size) + 1)) {
            break;
        }
    }

    close(fd);
}

void receive_file(int leecher_fd, char *file_name, int n_seeders, int current_seg) {
    char *file_path = malloc(strlen("./files/") + strlen("temp") + 3);
    char str_seg[2];
    int fd;

    snprintf(str_seg, 2, "%d", current_seg);

    strcpy(file_path, "./files/");
    strcat(file_path, "temp");
    strcat(file_path, str_seg);

    if ((fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("OPEN ERROR");
        exit(1);
    }

    message_t msg;

    while (read(leecher_fd, &msg, sizeof(msg)) > 0) {
        print_message(msg);

        if (msg.code == P2P_FILE_DATA) {
            stream_write(fd, msg.body, msg.body_size);
        } else {
            break;
        }
        if (msg.body_size < BUF_LEN - 20) {
            break;
        }
    }

    close(fd);
}

void *transfer_file(void *arg) {
    int fd = *(int *)arg;
    char *file_name = NULL;

    message_t msg;
    int run = 1;
    do {
        // Read message from leecher
        if (read(fd, &msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        print_message(msg);

        // Process message
        switch (msg.code) {
            case P2P_FILE_REQUEST:
                printf("[SEEDER] sending file: %s\n", msg.body);

                file_name = (char*)malloc(msg.body_size);
                memcpy(file_name, msg.body, msg.body_size);
                break;

            case P2P_SEGMENT_REQUEST:
                if(NULL == file_name) {
                    fprintf(stderr, "[SEEDER] File name not specified for segment transfer.\n");
                    exit(1);
                }

                printf("[SEEDER] sending requested segment: %d\n", *((int *)msg.body + 1));

                int total_seg = *((int *)msg.body);
                int current_seg = *((int *)msg.body + 1);

                send_file(fd, file_name, total_seg, current_seg);
                break;
            case P2P_BYE:
                run = 0;
                break;

            default:
                fprintf(stderr, "Unknown command\n");
                exit(1);
        }
    } while (run);

    free(file_name);
    close(fd);

    printf("[SEEDER] connection closed\n");

    return NULL;
}

void *act_as_seeder(void *args) {
    int seeder_fd;
    struct sockaddr_in seeder_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(0),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    socklen_t seeder_addr_len = sizeof(struct sockaddr_in);

    printf("[SEEDER] starting seeder server...\n");

    // Seeder socket setup
    if ((seeder_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SOCKET ERROR");
        exit(1);
    }

    if (bind(seeder_fd, (struct sockaddr *)&seeder_addr, seeder_addr_len) < 0) {
        perror("BIND ERROR");
        exit(1);
    }

    if (getsockname(seeder_fd, (struct sockaddr *)&seeder_addr, &seeder_addr_len) < 0) {
        perror("GETSOCKNAME ERROR");
        exit(1);
    }

    thread_data.port = ntohs(seeder_addr.sin_port);

    if (listen(seeder_fd, MAX_PEERS) < 0) {
        perror("LISTEN ERROR");
        exit(1);
    }

    printf("[SEEDER] seeder started: %s\n", print_sockaddr(seeder_addr));

    // Accept connection
    pthread_t tid[MAX_PEERS];
    int i = 0;

    int leecher_fd;
    struct sockaddr_in client_sockaddr;
    socklen_t leecher_sockaddr_len = sizeof(client_sockaddr);

    for (;;) {
        // Accept a connection
        if ((leecher_fd = accept(seeder_fd, (struct sockaddr *)&client_sockaddr, &leecher_sockaddr_len)) < 0) {
            perror("ACCEPT ERROR");
            continue;
        }

        printf("[SEEDER] leecher connected: %s - %d\n", print_sockaddr(client_sockaddr), leecher_fd);

        // Create a new thread to handle the leecher
        if (pthread_create(&tid[i], NULL, &transfer_file, (void *)&leecher_fd) != 0) {
            perror("THREAD ERROR");
            exit(1);
        }
        i++;
    }

    close(seeder_fd);

    return NULL;
}

typedef struct {
    int port;
    char *file_name;
    int total_seeders;
    int current_seg;
} thread_seeder_args;

void* connect_to_seeder(void *arg) {
    thread_seeder_args* args = (thread_seeder_args*) arg;
    int leecher_fd;

    struct sockaddr_in leecher_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(args->port),
        .sin_addr.s_addr = inet_addr("127.0.0.1")};

    if ((leecher_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SOCKET ERROR");
        exit(1);
    }

    if (connect(leecher_fd, (struct sockaddr *)&leecher_addr, sizeof(leecher_addr)) < 0) {
        perror("CONNECT ERROR");
        exit(1);
    }

    printf("[LEECHER] connected to seeder: %s\n", print_sockaddr(leecher_addr));

    // send the name of the file you request as leecher
    message_t req_msg = create_message(P2P_FILE_REQUEST, args->file_name, strlen(args->file_name) + 1);
    if (write(leecher_fd, &req_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    // send the total number of segments and segment
    int seg_req[] = {args->total_seeders, args->current_seg};

    // tell the seeder what segment you want as a leecher
    message_t req_seg_msg = create_message(P2P_SEGMENT_REQUEST, seg_req, 8);
    if (write(leecher_fd, &req_seg_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }
    

    printf("[LEECHER] downloading file: %s\n", args->file_name);
    receive_file(leecher_fd, args->file_name, args->total_seeders, args->current_seg);
    printf("[LEECHER] file download complete\n");

    message_t bye_msg = create_message(P2P_BYE, NULL, 0);
    if (write(leecher_fd, &bye_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    close(leecher_fd);

    return NULL;
}

//----- act_as_leecher is used for the functionality of the local peer
//----- to transfer requested files from other peers to the user's computer
void act_as_leecher(char *file_name, int *seeder_ports, int total_seeders) {
    pthread_t thread_id[10];
    thread_seeder_args args[total_seeders];
    char *file_path = malloc(strlen(file_name) + strlen("./files/") + 1);
    char str_seg[2], buf[BUF_LEN];
    size_t n_bytes = 0;
    int fd, fd_temp; 

    for(int i = 0; i < total_seeders; i++) {
        // int port, char *file_name, int total_seeders, int current_seg
        args[i] = (thread_seeder_args) {
            .port = seeder_ports[i],
            .file_name = file_name,
            .total_seeders = total_seeders,
            .current_seg = i
        };

        // create a new thread for every peer that has the file
        if (pthread_create(&thread_id[i], NULL, connect_to_seeder, (void*) &args[i]) != 0) {
            perror("THREAD CREATION ERROR");
            exit(1);
        }
    }

    for(int i = 0; i < total_seeders; i++) {
        // create a new thread for every peer that has the file
        if (pthread_join(thread_id[i], NULL) != 0) {
            perror("THREAD JOIN ERROR");
            exit(1);
        }
    }

    
    // create the file path on leecher's machine
    strcpy(file_path, "./files/");
    strcat(file_path, file_name);

    // create and open the final file
    if ((fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("OPEN ERROR");
        exit(1);
    }

    // recompose the file
    for(int i = 0; i < total_seeders; i++) {
        // read temp_i file
        snprintf(str_seg, 2, "%d", i);
        char *temp_path = malloc(strlen("./files/") + strlen("temp") + 3); 

        strcat(temp_path, "./files/temp");
        strcat(temp_path, str_seg);

        if ((fd_temp = open(temp_path, O_RDONLY)) < 0) {
            perror("OPEN ERROR");
            exit(1);
        }

        // read from temp file and write to final file
        while((n_bytes = read(fd_temp, buf, BUF_LEN)) > 0) {
            // write to final file
            if(write(fd, buf, n_bytes) < 0) {
                fprintf(stderr, "Could not write to file.\n");
                exit(1);
            }
        }

        if(n_bytes < 0) {
            perror("Something went wrong when reading\n");
            exit(1);
        }


        if(close(fd_temp) < 0) {
            fprintf(stderr, "Something went wrong when closing the file.\n");
            exit(1);
        }
    }

    if(close(fd) < 0) {

        fprintf(stderr, "Something went wrong when closing the file.\n");
        exit(1);
    }
}

//----- is_valid_path checks if a given path exists in the system
//
//----- Returns 1 if the path exists and 0 otherwise
int is_valid_path(char *path) {
    DIR *dir;

    if (NULL == (dir = opendir(path))) {
        return 0;
    } else {
        // close the opened dir
        if (-1 == closedir(dir)) {
            fprintf(stderr, "Could not close the directory %s\n", path);
        }

        return 1;
    }
}

int has_file(char *file) {
    char *file_path = malloc(strlen("./res/") + strlen(file) + 1);
    strcpy(file_path, "./res/");
    strcat(file_path, file);

    int fd = open(file_path, O_RDONLY);

    if (fd < 0) {
        return 0;
    }

    if(close(fd) < 0) {
        fprintf(stderr, "Could not close %s correctly\n", file_path);
        exit(1);
    }

    return 1;
}

typedef struct {
    char *file_name;
    int fd;
} thread_args;

void *act_as_client(void *arg) {
    thread_args args = *(thread_args *)arg;
    char *file_name = args.file_name;
    int fd = args.fd;

    message_t res_msg;
    message_t req_msg;

    while (1) {
        if (read(fd, &res_msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        print_message(res_msg);

        switch (res_msg.code) {
            case P2P_FILE_REQUEST:
                printf("[CLIENT] file requested: %s\n", res_msg.body);

                if (has_file(res_msg.body)) {
                    printf("[CLIENT] file found\n");

                    req_msg = create_message(P2P_FILE_FOUND, &thread_data.port, 4);
                    if (write(fd, &req_msg, sizeof(message_t)) < 0) {
                        perror("WRITE ERROR");
                        exit(1);
                    }
                } else {
                    printf("[CLIENT] file not found\n");

                    req_msg = create_message(P2P_FILE_NOT_FOUND, NULL, 0);
                    if (write(fd, &req_msg, sizeof(message_t)) < 0) {
                        perror("WRITE ERROR");
                        exit(1);
                    }
                }
                break;

            case P2P_ERR_NO_PEERS:
                printf("No peers available at the moment! Try again later!\n");
                break;

            case P2P_PEER_LIST:
                int seeder_number = res_msg.body_size / sizeof(int);
                printf("[CLIENT] Peer list received: \n");

                // print the ports of the peers that have the requested file
                for(int i = 0; i < seeder_number; i++) {
                    printf("\t[%d] - %d\n", i, *((int*)res_msg.body + i));
                }


                int* seeder_ports = (int*)res_msg.body;
                act_as_leecher(file_name, seeder_ports, seeder_number);

                break;

            default:
                fprintf(stderr, "Unknown command\n");
                exit(1);
        }
    }

    return NULL;
}

int connect_to_server(int peer_address) {
    int leecher_fd;
    struct sockaddr_in client_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SRV_PORT),
        .sin_addr.s_addr = htonl(peer_address),
    };

    // Connect to server
    if ((leecher_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SOCKET ERROR");
        return -1;
    }

    if (connect(leecher_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("CONNECT ERROR");
        return -1;
    }

    return leecher_fd;
}

void find_file_for_client(char *file_name, int fd) {
    message_t req_msg = create_message(P2P_FILE_REQUEST, file_name, strlen(file_name) + 1);

    // Send the file name to the server
    if (write(fd, &req_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }
}

int main(int argc, char **argv) {
    int leecher_fd;
    uint32_t peer_address = 2130706433;

    if ((leecher_fd = connect_to_server(peer_address)) < 0) {
        perror("CONNECT TO SERVER ERROR");
        exit(1);
    }

    // Create process to communicate with the server & peers
    pthread_t thread_client, thread_seeder;

    // Process for seeder to peers
    if (pthread_create(&thread_seeder, NULL, act_as_seeder, NULL) != 0) {
        perror("THREAD CREATION ERROR");
        exit(1);
    }

    // Process for client to server
    thread_args args = {
        .file_name = argv[1],
        .fd = leecher_fd};

    if (pthread_create(&thread_client, NULL, act_as_client, (void *)&args) != 0) {
        perror("THREAD CREATION ERROR");
        exit(1);
    }

    // Input the file name
    if (argc > 1) {
        find_file_for_client(argv[1], leecher_fd);
    }

    while (1) {
    }

    close(leecher_fd);

    return 0;
}