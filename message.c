#include "message.h"
#include <string.h>
#include <stdlib.h>

message_t *create_message(int cmd, char *body) {
    message_t *msg = (message_t *) malloc(sizeof(message_t));
    msg->cmd = cmd;
    msg->body_size = strlen(body);
    strcpy(msg->body, body);
    return msg;
}