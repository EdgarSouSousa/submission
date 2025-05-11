#ifndef HTTP_COMMS_H_
#define HTTP_COMMS_H_

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_http_client.h"

// Define the base URL of your HTTP server
#define HTTP_BASE_URL "http://192.168.217.186:8000"

// Structure to hold HTTP message data
typedef struct HttpMessage {
    char *endpoint;
    char *payload;
} HttpMessage;

// Function to send an HTTP POST request
bool http_post(const char *endpoint, const char *payload);

// Function to send an HTTP POST request and check the response
bool http_post_check_response(const char *endpoint, const char *payload);

// Function to send a ping request
bool http_ping();

// Function to send an error message
bool http_post_error(const char *errorMessage);

#endif /* HTTP_COMMS_H_ */