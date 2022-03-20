#ifndef __MESSAGE_H
#define __MESSAGE_H

#define P2P_BYE 0
#define P2P_FILE_REQUEST 1
#define P2P_PEER_LIST 2
#define P2P_FILE_FOUND 3
#define P2P_FILE_NOT_FOUND 4
#define P2P_ERR_NO_PEERS 10

typedef struct {
    int cmd;
    int body_size;
    char body[1024];
} message_t;

message_t *create_message(int cmd, char *body);

void print_message(int code);

#endif