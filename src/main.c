#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi_types.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include <time.h>

#include "httpComms.h"
#include "BME280.h"

//Wi-Fi configuration
#define WIFI_SSID      "PontoQuente"
#define WIFI_PASS      "qualquercoisa"
#define WIFI_MAX_RETRY  5
#define MODE WIFI_AUTH_WPA2_PSK

//Defines tha maximum number of retries that a sensor reading can be sent to the server
//WARNING: Setting this value too high can cause the shared memory region to fill
#define HTTP_MAX_RETRY 5

//I2C bus configuration
#define DATA_LENGTH 100
i2c_master_bus_handle_t bus0;

//I2C device configuration
i2c_master_dev_handle_t adafruit;
#define I2C_MASTER_SCL_IO 18
#define I2C_MASTER_SDA_IO 19
#define I2C_PORT_NUM_0 0

// Tags for logging
static const char *TAG_WIFI = "WIFI";
static const char *SETUP = "APP_MAIN";
static const char *COMMS_TASK = "COMMS_TASK";
static const char *SENSOR_TASK = "SENSOR_TASK";
static const char *ERROR_HANDLER = "ERROR_HANDLER";

//Check and report macro serves to verify correct function behaviour
//And send errors to the server
#define CHECK_AND_REPORT(err, msg) do { \
    if ((err) != ESP_OK) { \
        ESP_LOGE(ERROR_HANDLER, "%s: %s", msg, esp_err_to_name(err)); \
        http_post_error(msg); \
    } \
} while (0)

// Event group to signal Wi-Fi connection
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// Queue for HTTP messages
QueueHandle_t http_queue;

// Event handler for Wi-Fi Connection Events taken from ESP-IDF examples
static void event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_WIFI, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Wifi initialization boilerplate function taken from ESP-IDF examples
void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = MODE,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}


// Add to queue function
void add_to_queue(QueueHandle_t queue, const char *endpoint, const char *payload) {
    HttpMessage message;
    message.endpoint = strdup(endpoint); 
    message.payload = strdup(payload);

    //First step is allocating memory for the messsage
    if (message.endpoint == NULL || message.payload == NULL) {
        ESP_LOGE(COMMS_TASK, "Failure to allocate memory for the message");
        if (message.endpoint) free(message.endpoint);
        if (message.payload) free(message.payload);
        return;
    }

    //Second step is the addition to the Queue
    if (xQueueSend(queue, &message, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(COMMS_TASK, "Failed to send message to the Queue");
        free(message.endpoint);
        free(message.payload); 
    }
}


void sensorTask(void *pvParameters) {
    int32_t temp_raw;
    uint8_t chip_id;
    esp_err_t err;

    // Initialize the BME280 sensor
    CHECK_AND_REPORT(bme280_init(adafruit), "Failed to initialize BME280");

    // Read the chip ID
    CHECK_AND_REPORT(bme280_rd_id(adafruit, &chip_id), "Failed to read chip ID");

    while (1) {
        err = bme280_rd_temp(adafruit, &temp_raw);
        CHECK_AND_REPORT(err, "Failed to read temperature");
        if (err != ESP_OK) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(SENSOR_TASK, "Raw temperature data: %ld", temp_raw);
        float temperature = compensate_temperature(temp_raw);
        time_t now;
        time(&now);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "temperature", temperature);
        cJSON_AddNumberToObject(root, "timestamp", (double)now);

        char *json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (json_str) {
            add_to_queue(http_queue, "/api/temperature", json_str);
            cJSON_free(json_str);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//Inside comms task I do not utilize the CHECK_AND_REPORT macro
//I assume that if an error ocurrs in the commsTask that means the error message will also not be sent
void commsTask(void *pvParameters) {
    HttpMessage message;
    while (1) {
        if (xQueueReceive(http_queue, &message, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(COMMS_TASK, "Sending HTTP POST to %s with payload: %s", message.endpoint, message.payload);
            
            //Retry HTTP send until MAX_TRIES
            int retry_count = 0;
            bool success = false;
            while (retry_count < HTTP_MAX_RETRY) {
                if (http_post(message.endpoint, message.payload)) {
                    ESP_LOGI(COMMS_TASK, "HTTP POST sent successfully");
                    success = true;
                    break;
                } else {
                    ESP_LOGE(COMMS_TASK, "Failed to send HTTP POST, retrying...");
                    retry_count++;
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            if (!success) {
                ESP_LOGE(COMMS_TASK, "Failed to send HTTP POST after %d retries", HTTP_MAX_RETRY);
            }

            // Free the memory after the message is processed
            free(message.endpoint); 
            free(message.payload);  
        }
    }
}

// The main fucntion acts mostly as setting up and initial configurations
void app_main() {

    //Create QUEUE
    http_queue = xQueueCreate(10, sizeof(HttpMessage));
    if (http_queue == NULL) {
        ESP_LOGE(SETUP, "Failed to create queue");
        return;
    }

    //Initiate NVS flash
    CHECK_AND_REPORT(nvs_flash_init(), "Failed to init NVS");
    wifi_init_sta();
    ESP_LOGI(SETUP, "Wi-Fi initialized");

    //Wait for wifi connection
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(SETUP, "Wi-Fi connected, sending initial ping");

    //Setup sntp
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;

    //try to get current time until max retires
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(SETUP, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    //Send ping to verify if server is ready.
    //Here we wait for the server response unlike 
    if (!http_ping()) {
        ESP_LOGW(SETUP, "Initial ping failed");
    } else {
        ESP_LOGI(SETUP, "Initial ping successful");
    }

    //I2C bus setup
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
    };


    //now that we know that the server is alive we can use the Check and Report macro
    CHECK_AND_REPORT(i2c_new_master_bus(&i2c_mst_config, &bus0), "Failed to initialize I2C bus");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x77,
        .scl_speed_hz = 100000,
    };

    CHECK_AND_REPORT(i2c_master_bus_add_device(bus0, &dev_cfg, &adafruit), "Failed to add I2C device");

    //Create tasks
    xTaskCreate(sensorTask, "sensorTask", 4096, NULL, 1, NULL);
    xTaskCreate(commsTask, "commsTask", 4069, NULL, 1, NULL);
}
