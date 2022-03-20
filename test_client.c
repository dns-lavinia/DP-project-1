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

#define PORT 5005

#define P2P_BYE 0
#define P2P_FILE_REQUEST 1
#define P2P_PEER_LIST 2
#define P2P_FILE_FOUND 3
#define P2P_FILE_NOT_FOUND 4
#define P2P_ERR_NO_PEERS 10

struct sockaddr_in set_socket_addr(uint32_t inaddr, short sin_port) {
    struct sockaddr_in addr;
    memset((void*) &addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(inaddr);
    addr.sin_port = htons(sin_port);

    return addr;
}

typedef struct {
    int cmd;
    int body_size;
    char body[100];
} message_t;

int main() {
    int sockfd, connfd;
    uint32_t peer_address = 2130706433;
    struct sockaddr_in client_addr = set_socket_addr(peer_address, PORT);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if ((connfd = connect(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr))) < 0) {
        perror("CONNECT ERROR");
        exit(1);
    }

    message_t msg;
    msg.cmd = P2P_FILE_REQUEST;
    msg.body_size = 4;
    strcpy(msg.body, "test");
    if (write(sockfd, (void *) &msg, sizeof(msg)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    // waiting for request for file 'test'
    if (read(sockfd, (void *) &msg, sizeof(msg)) < 0) {
        perror("READ ERROR");
        exit(1);
    }

    printf("%d\n", msg.cmd);
    printf("%s\n", msg.body);

    // response to request for file 'test'
    msg.cmd = P2P_FILE_FOUND;
    if (write(sockfd, (void *) &msg, sizeof(msg)) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    while (1) {}

    close(sockfd);
}