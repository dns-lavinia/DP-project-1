#include "netio.h"
#include <stdlib.h>
#include <netinet/in.h> // for sockaddr_in struct
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

struct sockaddr_in set_socket_addr(uint32_t inaddr, short sin_port) {
    struct sockaddr_in addr;
    memset((void*) &addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(inaddr);
    addr.sin_port = htons(sin_port);

    return addr;
}

void set_addr(struct sockaddr_in *addr, uint32_t inaddr, short sin_port) {
    memset((void *)addr, 0, sizeof(*addr));

    // Set the family to be the IPv4 internet protocols
    addr->sin_family = AF_INET;

    addr->sin_addr.s_addr = htonl(inaddr);
    addr->sin_port = htons(sin_port);
}