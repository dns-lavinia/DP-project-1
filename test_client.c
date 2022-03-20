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

struct sockaddr_in set_socket_addr(uint32_t inaddr, short sin_port) {
    struct sockaddr_in addr;
    memset((void*) &addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(inaddr);
    addr.sin_port = htons(sin_port);

    return addr;
}

int main() {
    int sockfd, connfd;
    uint32_t peer_address = 2130706433;
    struct sockaddr_in client_addr = set_socket_addr(peer_address, PORT);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if ((connfd = connect(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr))) < 0) {
        perror("CONNECT ERROR");
        exit(1);
    }

    if (write(sockfd, "Hello", 5) < 0) {
        perror("WRITE ERROR");
        exit(1);
    }

    close(sockfd);
}