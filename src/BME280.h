#include "driver/i2c_types.h"
#include "esp_err.h"
#include <stdint.h>

// BME280 Register Addresses
#define BME280_REG_ID        0xD0
#define BME280_REG_RESET     0xE0
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_STATUS    0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_TEMP_MSB  0xFA


// Function prototypes
esp_err_t bme280_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len);
esp_err_t bme280_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t data);
esp_err_t bme280_init(i2c_master_dev_handle_t dev);
esp_err_t bme280_rd_id(i2c_master_dev_handle_t dev, uint8_t *chip_id);
esp_err_t bme280_rd_temp(i2c_master_dev_handle_t dev, int32_t *temp_raw);
float compensate_temperature(int32_t raw_temp);

