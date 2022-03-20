#include "message.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

message_t *create_message(int cmd, char *body) {
    message_t *msg = (message_t *) malloc(sizeof(message_t));
    msg->cmd = cmd;
    msg->body_size = strlen(body);
    strcpy(msg->body, body);
    return msg;
}

void print_message(int code) {
    switch (code) {
        case P2P_BYE:
            printf("P2P_BYE\n");
            break;
        case P2P_FILE_REQUEST:
            printf("P2P_FILE_REQUEST\n");
            break;
        case P2P_PEER_LIST:
            printf("P2P_PEER_LIST\n");
            break;
        case P2P_FILE_FOUND:
            printf("P2P_FILE_FOUND\n");
            break;
        case P2P_FILE_NOT_FOUND:
            printf("P2P_FILE_NOT_FOUND\n");
            break;
        case P2P_ERR_NO_PEERS:
            printf("P2P_ERR_NO_PEERS\n");
            break;
        default:
            printf("UNKNOWN\n");
            break;
    }
}