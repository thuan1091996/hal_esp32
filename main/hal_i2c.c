/*******************************************************************************
* Title                 :   I2C HAL layer for ESP-IDF
* Filename              :   hal_i2c.c
* Origin Date           :   2023/08/08
* Version               :   0.0.0
* Compiler              :   ESP-IDF V5.0.2
* Target                :   ESP32 
* Notes                 :   None
*******************************************************************************/


/** \file hal_i2c.c
 *  \brief This module contains the
 */
/******************************************************************************
* Includes
*******************************************************************************/
#include <stdint.h>
#include <string.h>
#include "hal.h"
#include "esp_log.h"
#include "driver/i2c.h"

/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define TAG                     "HAL_I2C"
#define I2C_MASTER_NUM          (0)
#define I2C_MASTER_FREQ_HZ      (400000)
#define I2C0_SDA_IO             (22) 
#define I2C0_SCL_IO             (23)
#define I2C1_SDA_IO             (21)
#define I2C1_SCL_IO             (25)
#define I2C_DEFAULT_TIMEOUT     (3000 / portTICK_PERIOD_MS)
#define I2C_NUM                 (2)
/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/
// Mutex for I2C0 and I2C1
SemaphoreHandle_t i2c_mutex[I2C_NUM] = {NULL, NULL};
/******************************************************************************
* Function Prototypes
*******************************************************************************/

/******************************************************************************
* Function Definitions
*******************************************************************************/
//Initialize I2C. Returns 0 on success, -1 on failure.
int __InitI2C0()
{
    esp_err_t status;
    // I2C on pins IO23/IO25
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C0_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C0_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0
    };
    status = i2c_param_config(I2C_NUM_0, &conf);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_param_config failed with status %d", status);
        return FAILURE;
    }
    status = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_driver_install failed with status %d", status);
        return FAILURE;
    }
    return SUCCESS;
}

int __InitI2C1()
{
    esp_err_t status;
    // I2C on pins IO21/IO22
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C1_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C1_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0
    };
    status = i2c_param_config(I2C_NUM_1, &conf);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_param_config failed with status %d", status);
        return FAILURE;
    }
    status = i2c_driver_install(I2C_NUM_1, conf.mode, 0, 0, 0);
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_driver_install failed with status %d", status);
        return FAILURE;
    }
    esp_log_level_set(TAG, ESP_LOG_ERROR);
    return SUCCESS;
}


// Initialize I2C0 and I2C1. Returns 0 on success, -1 on failure.
int __InitI2C()
{
    int status = FAILURE;
    status = __InitI2C0();
    if(status != SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to init I2C0");
        return FAILURE;
    }
    // Create mutex for I2C0
    i2c_mutex[0] = xSemaphoreCreateMutex();
    if(i2c_mutex[0] == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex for I2C0");
        return FAILURE;
    }

    /*
    status = __InitI2C1();
    if(status != SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to init I2C1");
        return FAILURE;
    }
    // Create mutex for I2C1
    i2c_mutex[1] = xSemaphoreCreateMutex();
    if(i2c_mutex[1] == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex for I2C1");
        return FAILURE;
    }
    */
    
    return SUCCESS;
}

bool hal__I2CEXISTS(uint8_t i2c_num, uint8_t ADDR)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Check if I2C device exists at ADDR. Returns true if it does, false if not.
    esp_err_t status;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Create an I2C command to check if the slave exists
    // by sending a start condition, followed by the slave address with a write bit, and then stop condition.
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    // Execute the I2C command and check for errors
    status = i2c_master_cmd_begin(i2c_num, cmd, I2C_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "I2C device exists at address 0x%02X", ADDR);
        status = true;
    } 
    else 
    {
        ESP_LOGE(TAG, "I2C device does not exist at address 0x%02X ", ADDR);
        status = false;
    }
    return status;
}


int hal__I2CREAD_uint8(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    // Acquire the I2C mutex
    xSemaphoreTake(i2c_mutex[i2c_num], portMAX_DELAY);
    
    // Read data from I2C device at ADDR, register REG. Returns 0 on success, -1 on failure.
    esp_err_t status = i2c_master_write_read_device(i2c_num, ADDR, &REG, 1, data, 1, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C read failed with status %d", status);
        // Release the I2C mutex
        xSemaphoreGive(i2c_mutex[i2c_num]);
        status = FAILURE;
    }
    else
    { 
        ESP_LOGI(TAG, "hal__I2CREAD_uint8 read at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, *data);
    }
    // Release the I2C mutex
    xSemaphoreGive(i2c_mutex[i2c_num]);
    return status;
}

int hal__I2CREAD(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data, uint16_t len)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    // Acquire the I2C mutex
    xSemaphoreTake(i2c_mutex[i2c_num], portMAX_DELAY);
    
    // Read data from I2C device at ADDR, register REG. Returns number of bytes read on success, -1 on failure.
    esp_err_t status = i2c_master_write_read_device(i2c_num, ADDR, &REG, 1, data, len, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C read failed with status %d", status);
        status = FAILURE;
    }
    else
    {
        ESP_LOGI(TAG, "hal__I2CREAD read at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, *data);
    }

    // Release the I2C mutex
    xSemaphoreGive(i2c_mutex[i2c_num]);
    return status;
}

int hal__I2CWRITE_uint8(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t data)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Write data to I2C device at ADDR, register REG. Returns 0 on success, -1 on failure.

    // Acquire the I2C mutex
    xSemaphoreTake(i2c_mutex[i2c_num], portMAX_DELAY);

    uint8_t buf[2] = {REG, data};

    esp_err_t status = i2c_master_write_to_device(i2c_num, ADDR, buf, 2, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C write failed with status %d", status);
        status = FAILURE;
    }
    else
    { 
        ESP_LOGI(TAG, "hal__I2CWRITE_uint8 write at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, data);
    }
    // Release the I2C mutex
    xSemaphoreGive(i2c_mutex[i2c_num]);
    return status;
}

int hal__I2CWRITE(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data, uint16_t len)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Write data to I2C device at ADDR, register REG. Returns number of bytes written on success, -1 on failure.

    // Acquire the I2C mutex
    xSemaphoreTake(i2c_mutex[i2c_num], portMAX_DELAY);

    // Append the register address to the data buffer
    uint8_t buf[len + 1];
    buf[0] = REG;
    memcpy(buf + 1, data, len);

    esp_err_t status = i2c_master_write_to_device(i2c_num, ADDR, buf, len + 1, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C write failed with status %d", status);
        status = FAILURE;
    }
    else
    {
        ESP_LOGI(TAG, "hal__I2CWRITE write at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, *data);
    }
    // Release the I2C mutex
    xSemaphoreGive(i2c_mutex[i2c_num]);
    return status;
}