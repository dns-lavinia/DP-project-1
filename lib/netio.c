#include "netio.h"

struct sockaddr_in create_sockaddr(uint32_t inaddr, short sin_port) {
    struct sockaddr_in addr;
    memset((void *)&addr, 0, sizeof(addr));

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

char* print_sockaddr(struct sockaddr_in addr) {
    char* str = malloc(sizeof(char) * 30);
    sprintf(str, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    return str;
}

int stream_read(int fd, void* buf, size_t len, off_t offset) {
    int bytes_read = 0;
    int bytes_left = len;

    while (bytes_left > 0) {
        if ((bytes_read = pread(fd, buf + bytes_read, bytes_left, offset)) < 0) {
            return -1;
        } 
        if (bytes_read == 0) {
            break;
        }
        
        bytes_left -= bytes_read;
        buf += bytes_read;
    }
    return len - bytes_left;
}

int stream_write(int fd, void* buf, size_t len) {
    int bytes_written = 0;
    int bytes_left = len;

    while (bytes_left > 0) {
        if ((bytes_written = write(fd, buf + bytes_written, bytes_left)) < 0) {
            return -1;
        }
        
        bytes_left -= bytes_written;
        buf += bytes_written;
    }
    return len - bytes_left;
}