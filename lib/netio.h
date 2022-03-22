#ifndef __NETIO_H
#define __NETIO_H

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>  // for sockaddr_in struct
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct sockaddr_in create_sockaddr(uint32_t inaddr, short sin_port);

//----- set_addr is used to initialize a sockaddr_in struct
//
//----- Parameters
//----- 	addr: sockaddr_in struct to be initialized
//-----		inaddr: IP address
void set_addr(struct sockaddr_in *addr, uint32_t inaddr, short sin_port);

char* print_sockaddr(struct sockaddr_in addr);

int stream_read(int fd, void* buf, size_t count);

int stream_write(int fd, void* buf, size_t count);

#endif