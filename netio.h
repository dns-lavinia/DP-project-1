#ifndef __NETIO_H
#define __NETIO_H

struct sockaddr_in set_socket_addr(uint32_t inaddr, short sin_port);

//----- set_addr is used to initialize a sockaddr_in struct
//
//----- Parameters
//----- 	addr: sockaddr_in struct to be initialized
//-----		inaddr: IP address
void set_addr(struct sockaddr_in *addr, uint32_t inaddr, short sin_port);

#endif