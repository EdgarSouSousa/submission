#pragma once
#include <time.h>

typedef enum {
    HTTP_MSG_SEND_DATA,
    HTTP_MSG_SEND_PING,
    HTTP_MSG_SEND_ERROR
} HttpMessageType;

typedef struct {
    HttpMessageType type;
    union {
        struct {
            int value;
            time_t timestamp;
        } data;
        struct {
            char error_msg[128];
        } error;
    };
} HttpMessage;