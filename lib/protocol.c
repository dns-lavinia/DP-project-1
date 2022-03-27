#include "protocol.h"

#define PRINT_FLAG 1

message_t create_message(int code, void *body, int body_size) {
    message_t msg;
    msg.code = code;
    msg.body_size = body_size;
    memcpy(msg.body, body, body_size);

    return msg;
}

char *message_code[] = {
    "P2P_BYE",
    "P2P_FILE_REQUEST",
    "P2P_PEER_LIST",
    "P2P_FILE_FOUND",
    "P2P_FILE_NOT_FOUND",
    "P2P_ERR_NO_PEERS",
    "P2P_FILE_DATA",
    "P2P_SEGMENT_REQUEST"
};

void print_message(message_t msg) {
    if (!PRINT_FLAG) return;

    char* body = (char*) malloc(msg.body_size);
    memcpy(body, msg.body, msg.body_size);

    if (msg.body_size < 20) {
        printf("[PROTOCOL]\ncode: %s\nbody_size: %d\nbody: %.*s\n", message_code[msg.code], msg.body_size, msg.body_size, body);
    } else {
        printf("[PROTOCOL]\ncode: %s\nbody_size: %d\nbody: %.*s...\n", message_code[msg.code], msg.body_size, 20, body);
    }
}

char *msg_to_string(message_t msg) {
    char* str = malloc(30);
    sprintf(str, "%s - size: %d", message_code[msg.code], msg.body_size);

    return str;
}