#include <arpa/inet.h>  // for inet_addr
#include <dirent.h>     // for opendir, closedir
#include <errno.h>
#include <netinet/in.h>  // for sockaddr_in struct
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // for memset
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  // for close

#include "../lib/netio.h"
#include "../lib/protocol.h"

#define PORT 5005

int main() {
    int client_fd;
    struct sockaddr_in client_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(2130706433),
        .sin_port = htons(PORT)
    };

    // Client socket setup
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SOCKET ERROR");
        exit(1);
    }

    if (connect(client_fd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
        perror("CONNECT ERROR");
        exit(1);
    }

    message_t *msg = create_message(P2P_FILE_REQUEST, "test.txt", strlen("test.txt"));
    
    if (write(client_fd, (void *) msg, sizeof(msg)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    // waiting for request for file 'test'
    if (read(client_fd, (void *) msg, sizeof(msg)) < 0) {
        perror("READ ERROR");
        exit(1);
    }

    printf("%d\n", msg->cmd);
    printf("%s\n", msg->body);

    // response to request for file 'test'
    msg->cmd = P2P_FILE_FOUND;
    if (write(client_fd, (void *) msg, sizeof(msg)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    close(client_fd);
}