#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define P2P_BYE 0
#define P2P_FILE_REQUEST 1
#define P2P_PEER_LIST 2
#define P2P_FILE_FOUND 3
#define P2P_FILE_NOT_FOUND 4
#define P2P_ERR_NO_PEERS 5
#define P2P_FILE_DATA 6

typedef struct {
    int code;
    int body_size;
    char body[1024];
} message_t;

message_t create_message(int code, void *body, int body_size);

void print_message(message_t msg);

#endif