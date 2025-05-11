#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"                      
#include "driver/i2c_master.h"           
#include "esp_log.h"
#include "BME280.h"

//Functions were heavily inspired by the existing manufacturers library
//Adapted to remove usage of Arduino Wire.h library


//Read single register
esp_err_t bme280_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

//read to single register
esp_err_t bme280_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = { reg, data };
    return i2c_master_transmit(dev, buf, 2, -1);
}


//Initiate communication
esp_err_t bme280_init(i2c_master_dev_handle_t dev) {
    esp_err_t err;

    // Soft reset
    err = bme280_write_reg(dev, BME280_REG_RESET, 0xB6);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(100));

    // Set normal mode, temp and pressure oversampling x1
    err = bme280_write_reg(dev, BME280_REG_CTRL_MEAS, 0x27);
    if (err != ESP_OK) return err;

    // Set config: standby time 1000ms, filter off
    err = bme280_write_reg(dev, BME280_REG_CONFIG, 0xA0);
    return err;
}

esp_err_t bme280_rd_id(i2c_master_dev_handle_t dev, uint8_t *chip_id) {
    return bme280_read_reg(dev, BME280_REG_ID, chip_id, 1);
}

//Simple read temp function
esp_err_t bme280_rd_temp(i2c_master_dev_handle_t dev, int32_t *temp_raw) {
    uint8_t data[3];
    esp_err_t err = bme280_read_reg(dev, 0xFA, data, 3);
    if (err != ESP_OK) return err;

    *temp_raw = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    return ESP_OK;
}

//Calibration value was directly measured and calibrated by me
float compensate_temperature(int32_t raw_temp) {
    return ((float)raw_temp / 26653);
}
