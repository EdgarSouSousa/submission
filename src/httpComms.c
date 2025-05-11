#include "httpComms.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG_HTTP = "HTTP_COMMS";

//This is the helper function used by both post with ack and simple post
//A new HTTP client is created for every message, while not optimal for data transmission it avoids some memory leak issues 
static esp_err_t perform_http_post(const char *url, const char *payload, esp_http_client_handle_t *client) {
    //Boilerplate code for creating client
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    *client = esp_http_client_init(&config);
    if (*client == NULL) {
        ESP_LOGE(TAG_HTTP, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    //Sending the payload and reading ESP_OK
    esp_http_client_set_post_field(*client, payload, strlen(payload));
    esp_http_client_set_header(*client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_perform(*client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_HTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    return err;
}


//Simple post to HTTP used in sending data (we assume that data is not critical)
bool http_post(const char *endpoint, const char *payload) {
    char url_buffer[128];
    snprintf(url_buffer, sizeof(url_buffer), "%s%s", HTTP_BASE_URL, endpoint);
    esp_http_client_handle_t client = NULL;
    esp_err_t err = perform_http_post(url_buffer, payload, &client);
    if (client) {
        esp_http_client_cleanup(client);
    }
    return (err == ESP_OK);
}

//Heavier response for sending pings and error messages
bool http_post_check_response(const char *endpoint, const char *payload) {
    
    // Copy payloda to message
    char url_buffer[128];
    snprintf(url_buffer, sizeof(url_buffer), "%s%s", HTTP_BASE_URL, endpoint);
    esp_http_client_handle_t client = NULL;

    //call helper function
    esp_err_t err = perform_http_post(url_buffer, payload, &client);
    if (err != ESP_OK) {
        if (client) {
            esp_http_client_cleanup(client);
        }
        return false;
    }

    //check response if it is code 200
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGW(TAG_HTTP, "HTTP POST request to %s returned status code: %d", endpoint, status_code);
        esp_http_client_cleanup(client);
        return false;
    }


    //Heavier verification of the server response
    //Expects a JSON
    esp_err_t data_len = esp_http_client_get_content_length(client);
    if (data_len > 0) {
        char *buf = malloc(data_len + 1);
        if (buf != NULL) {
            esp_http_client_read_response(client, buf, data_len);
            buf[data_len] = 0;
            ESP_LOGI(TAG_HTTP, "HTTP Response from %s: %s", endpoint, buf);

            cJSON *json = cJSON_Parse(buf);
            if (json == NULL) {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL) {
                    ESP_LOGE(TAG_HTTP, "JSON parse error: %s", error_ptr);
                }
                free(buf);
                esp_http_client_cleanup(client);
                return false;
            }

            cJSON *response = cJSON_GetObjectItemCaseSensitive(json, "response");
            if (cJSON_IsString(response) && (response->valuestring != NULL)) {
                ESP_LOGI(TAG_HTTP, "Response message: %s", response->valuestring);
                cJSON_Delete(json);
                free(buf);
                esp_http_client_cleanup(client);
                return true;
            } else {
                ESP_LOGW(TAG_HTTP, "Could not parse 'response' field in JSON");
                cJSON_Delete(json);
                free(buf);
                esp_http_client_cleanup(client);
                return false;
            }
        } else {
            ESP_LOGE(TAG_HTTP, "Failed to allocate memory for HTTP response");
            esp_http_client_cleanup(client);
            return false;
        }
    } else {
        ESP_LOGI(TAG_HTTP, "HTTP POST to %s successful (no response body)", endpoint);
        esp_http_client_cleanup(client);
        return true;
    }
}

// Ping used in setup after Wi-Fi connect event
bool http_ping() {
    return http_post_check_response("/api/ping", "");
}

// POST error used by the CHECK_AND_REPORT macro
bool http_post_error(const char *errorMessage) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG_HTTP, "Failed to create JSON object for error message");
        return false;
    }
    if (!cJSON_AddStringToObject(json, "error", errorMessage)) {
        ESP_LOGE(TAG_HTTP, "Failed to add error message to JSON");
        cJSON_Delete(json);
        return false;
    }
    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str == NULL) {
        ESP_LOGE(TAG_HTTP, "Failed to serialize JSON error message");
        cJSON_Delete(json);
        return false;
    }
    bool result = http_post_check_response("/api/error", json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);
    return result;
}