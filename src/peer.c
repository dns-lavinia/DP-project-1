#define _XOPEN_SOURCE 500
#include <arpa/inet.h>  // for inet_addr
#include <dirent.h>     // for opendir, closedir
#include <errno.h>
#include <fcntl.h>       //for open file modes.
#include <ftw.h>         // for nftw
#include <netinet/in.h>  // for sockaddr_in struct
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // for memset
#include <sys/socket.h>
#include <sys/stat.h>  // for fstat
#include <sys/types.h>
#include <unistd.h>  // for close

#include "../lib/netio.h"
#include "../lib/protocol.h"
#include "../lib/logger.h"

//==============================================================================
//================ Some general info that we can delete later ==================
//
// the peer acts both as a client and as a server
//
// initially, a peer can be configured to have a list of known partners
//
// the user asks the (locally installed) peer to get a file -> after file is
// successfully transferred, it is stored in a directory chosen by the user
//
// the local peer contacts the other peers concurrently
//
// protocol errors (file does not exist, wrong command) should be handled
// appropriately
//
// we should do health checks every x seconds
//
//==============================================================================

#define MAX_PEERS 10
#define SRV_PORT 5006
#define BUF_LEN 1024

// thread shared data
typedef struct {
    int port;
} thread_data_t;

thread_data_t thread_data;

/* SEEDER CODE */

void upload_file_segment(int leecher_fd, char *file_name, int total_seg, int current_seg) {
    int fd;
    char *file_path = malloc(strlen("./res/") + strlen(file_name) + 1);
    sprintf(file_path, "./res/%s", file_name);

    if ((fd = open(file_path, O_RDONLY)) < 0) {
        perror("OPEN ERROR");
        exit(1);
    }

    struct stat st;
    fstat(fd, &st);

    // total bytes to read
    int total_to_read = st.st_size;
    int seg_size = total_to_read / total_seg;  // size of one segment to send
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
        if (write(leecher_fd, &msg, sizeof(msg)) < 0) {
            perror("WRITE ERROR");
            exit(1);
        }

        // keep track and update how much of the segment was read
        seg_size -= n_read;

        if (n_read > seg_size) {
            n_read = seg_size;
        }

        offset += bytes_read;

        if (offset == ((current_seg * seg_size) + 1)) {
            break;
        }
    }

    close(fd);

    // end file upload
    msg = create_message(P2P_BYE, NULL, 0);
    if (write(leecher_fd, &msg, sizeof(msg)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }
}

void *handle_leecher_communication(void *arg) {
    int fd = *(int *)arg;
    char *file_name = NULL;

    message_t msg;
    int run = 1;
    do {
        // read message from leecher
        if (read(fd, &msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        logger(LOG_PROTOCOL, msg_to_string(msg));

        // process message
        switch (msg.code) {
            case P2P_FILE_REQUEST:
                logger(LOG_SEEDER, "sending file: %s", msg.body);

                file_name = (char *)malloc(msg.body_size);
                memcpy(file_name, msg.body, msg.body_size);
                break;

            case P2P_SEGMENT_REQUEST:
                if (NULL == file_name) {
                    fprintf(stderr, "[SEEDER] File name not specified for segment transfer.");
                    exit(1);
                }

                logger(LOG_SEEDER, "sending requested segment: %d", *((int *)msg.body + 1));

                int total_seg = *((int *)msg.body);
                int current_seg = *((int *)msg.body + 1);

                upload_file_segment(fd, file_name, total_seg, current_seg);
                break;
            case P2P_BYE:
                run = 0;
                break;

            default:
                fprintf(stderr, "Unknown command");
                exit(1);
        }
    } while (run);

    free(file_name);
    close(fd);

    logger(LOG_SEEDER, "connection closed");

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

    logger(LOG_SEEDER, "starting seeder server...");

    // seeder socket setup
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

    logger(LOG_SEEDER, "seeder started: %s", print_sockaddr(seeder_addr));

    // accept connection
    pthread_t tid[MAX_PEERS];
    int i = 0;

    int leecher_fd;
    struct sockaddr_in client_sockaddr;
    socklen_t leecher_sockaddr_len = sizeof(client_sockaddr);

    for (;;) {
        // accept a connection
        if ((leecher_fd = accept(seeder_fd, (struct sockaddr *)&client_sockaddr, &leecher_sockaddr_len)) < 0) {
            perror("ACCEPT ERROR");
            continue;
        }

        logger(LOG_SEEDER, "leecher connected: %s - %d", print_sockaddr(client_sockaddr), leecher_fd);

        // create a new thread to handle the leecher
        if (pthread_create(&tid[i], NULL, &handle_leecher_communication, (void *)&leecher_fd) != 0) {
            perror("THREAD ERROR");
            exit(1);
        }
        i++;
    }

    close(seeder_fd);

    return NULL;
}

/* LEECHER CODE */

void download_file_segment(int leecher_fd, char *file_name, int current_seg) {
    char *file_path = malloc(strlen("./files/temp/") + strlen("temp") + 3);
    sprintf(file_path, "./files/temp/temp%d", current_seg);

    // create temp file
    int fd;
    if ((fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("OPEN ERROR");
        exit(1);
    }

    message_t msg;

    while (read(leecher_fd, &msg, sizeof(msg)) > 0) {
        logger(LOG_PROTOCOL, msg_to_string(msg));

        switch (msg.code) {
            case P2P_FILE_DATA:
                if (write(fd, msg.body, msg.body_size) < 0) {
                    perror("WRITE ERROR");
                    exit(1);
                }
                break;

            case P2P_BYE:
                close(fd);
                return;

            default:
                fprintf(stderr, "Unknown command");
                exit(1);
        }
    }
}

typedef struct {
    int port;
    char *file_name;
    int total_seeders;
    int current_seg;
} handle_seeder_communication_args;

void *handle_seeder_communication(void *arg) {
    handle_seeder_communication_args *args = (handle_seeder_communication_args *)arg;

    // connect to seeder
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

    logger(LOG_LEECHER, "connected to seeder: %s", print_sockaddr(leecher_addr));

    // send the name of the file you request as leecher
    message_t req_msg = create_message(P2P_FILE_REQUEST, args->file_name, strlen(args->file_name) + 1);

    if (write(leecher_fd, &req_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    // send the total number of segments and segment
    int seg_req[] = {args->total_seeders, args->current_seg};

    // tell the seeder what segment you want as a leecher
    message_t req_seg_msg = create_message(P2P_SEGMENT_REQUEST, seg_req, 2 * sizeof(int));

    if (write(leecher_fd, &req_seg_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    // download the segment
    logger(LOG_LEECHER, "downloading file segment: %d / %d (%s)", args->current_seg + 1, args->total_seeders, args->file_name);
    download_file_segment(leecher_fd, args->file_name, args->current_seg);
    logger(LOG_LEECHER, "file segment download complete: %d / %d (%s)", args->current_seg + 1, args->total_seeders, args->file_name);

    // send bye message
    message_t bye_msg = create_message(P2P_BYE, NULL, 0);
    if (write(leecher_fd, &bye_msg, sizeof(message_t)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    close(leecher_fd);

    return NULL;
}

void recreate_file(char *file_name, int total_segments) {
    int fd, fd_temp;

    // create the file path on leecher's machine
    char *file_path = malloc(strlen("./files/") + strlen(file_name) + 1);
    sprintf(file_path, "./files/%s", file_name);

    // create and open the final file
    if ((fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("OPEN ERROR");
        exit(1);
    }

    // recreate the file
    char buffer[BUF_LEN];
    int bytes_read;

    for (int i = 0; i < total_segments; i++) {
        // read temp_i file
        char *temp_path = malloc(strlen("./files/temp/") + strlen("temp") + 3);
        sprintf(temp_path, "./files/temp/temp%d", i);

        if ((fd_temp = open(temp_path, O_RDONLY)) < 0) {
            perror("OPEN ERROR");
            exit(1);
        }

        // read from temp file and write to final file
        while ((bytes_read = read(fd_temp, buffer, BUF_LEN)) > 0) {
            // write to final file
            if (write(fd, buffer, bytes_read) < 0) {
                perror("WRITE ERROR");
                exit(1);
            }
        }

        if (bytes_read < 0) {
            perror("READ ERROR");
            exit(1);
        }

        if (close(fd_temp) < 0) {
            perror("CLOSE ERROR");
            exit(1);
        }
    }

    if (close(fd) < 0) {
        perror("CLOSE ERROR");
        exit(1);
    }
}

int remove_temp_files(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int remove_temp(char *path) {
    return nftw(path, remove_temp_files, 64, FTW_DEPTH | FTW_PHYS);
}

void act_as_leecher(char *file_name, int *seeder_ports, int total_seeders) {
    // create temp directory
    mkdir("./files/temp", 0755);

    // create a thread for each peer that has the file
    pthread_t thread_id[total_seeders];
    handle_seeder_communication_args act_as_client_args[total_seeders];
    for (int i = 0; i < total_seeders; i++) {
        act_as_client_args[i] = (handle_seeder_communication_args){
            .port = seeder_ports[i],
            .file_name = file_name,
            .total_seeders = total_seeders,
            .current_seg = i};

        if (pthread_create(&thread_id[i], NULL, handle_seeder_communication, &act_as_client_args[i]) != 0) {
            perror("THREAD CREATION ERROR");
            exit(1);
        }
    }

    // join all threads
    for (int i = 0; i < total_seeders; i++) {
        if (pthread_join(thread_id[i], NULL) != 0) {
            perror("THREAD JOIN ERROR");
            exit(1);
        }
    }

    recreate_file(file_name, total_seeders);

    // remove temp directory
    if (remove_temp("./files/temp") < 0) {
        perror("REMOVE TEMP ERROR");
        exit(1);
    }
}

/* CLIENT CODE */

int has_file(char *file) {
    char *file_path = malloc(strlen("./res/") + strlen(file) + 1);
    sprintf(file_path, "./res/%s", file);

    int fd = open(file_path, O_RDONLY);

    if (fd < 0) {
        return 0;
    }

    if (close(fd) < 0) {
        perror("CLOSE ERROR");
        exit(1);
    }

    return 1;
}

typedef struct {
    char *file_name;
    int fd;
} act_as_client_args;

void *act_as_client(void *arg) {
    act_as_client_args args = *(act_as_client_args *)arg;
    char *file_name = args.file_name;
    int fd = args.fd;

    message_t res_msg;
    message_t req_msg;

    while (1) {
        if (read(fd, &res_msg, sizeof(message_t)) < 0) {
            perror("READ ERROR");
            exit(1);
        }

        logger(LOG_PROTOCOL, msg_to_string(res_msg));

        switch (res_msg.code) {
            case P2P_FILE_REQUEST:
                logger(LOG_CLIENT, "file requested: %s", res_msg.body);

                if (has_file(res_msg.body)) {
                    logger(LOG_CLIENT, "file found");

                    req_msg = create_message(P2P_FILE_FOUND, &thread_data.port, 4);
                    if (write(fd, &req_msg, sizeof(message_t)) < 0) {
                        perror("WRITE ERROR");
                        exit(1);
                    }
                } else {
                    logger(LOG_CLIENT, "file not found");

                    req_msg = create_message(P2P_FILE_NOT_FOUND, NULL, 0);
                    if (write(fd, &req_msg, sizeof(message_t)) < 0) {
                        perror("WRITE ERROR");
                        exit(1);
                    }
                }
                break;

            case P2P_ERR_NO_PEERS:
                logger(LOG_CLIENT, "No peers available at the moment! Try again later!");
                break;

            case P2P_PEER_LIST:
                logger(LOG_CLIENT, "Peer list received: ");
                int seeder_number = res_msg.body_size / sizeof(int);

                // print the ports of the peers that have the requested file
                for (int i = 0; i < seeder_number; i++) {
                    printf("\t[%d] - %d\n", i, *((int *)res_msg.body + i));
                }

                int *seeder_ports = (int *)res_msg.body;
                act_as_leecher(file_name, seeder_ports, seeder_number);

                break;

            default:
                fprintf(stderr, "Unknown command");
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

    // connect to server
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

    // send the file name to the server
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

    // create process to communicate with the server & peers
    pthread_t thread_client, thread_seeder;

    // process for seeder to peers
    if (pthread_create(&thread_seeder, NULL, act_as_seeder, NULL) != 0) {
        perror("THREAD CREATION ERROR");
        exit(1);
    }

    // process for client to server
    act_as_client_args args = {
        .file_name = argv[1],
        .fd = leecher_fd};

    if (pthread_create(&thread_client, NULL, act_as_client, (void *)&args) != 0) {
        perror("THREAD CREATION ERROR");
        exit(1);
    }

    // input the file name
    if (argc > 1) {
        find_file_for_client(argv[1], leecher_fd);
    }

    while (1) {
    }

    close(leecher_fd);

    return 0;
}