/*******************************************************************************
* Title                 :   I2C HAL layer for ESP-IDF
* Filename              :   hal_i2c.c
* Author                :   ItachiVN
* Origin Date           :   2023/08/08
* Version               :   0.0.0
* Compiler              :   ESP-IDF V5.0.2
* Target                :   ESP32 
* Notes                 :   None
*******************************************************************************/

/*************** MODULE REVISION LOG ******************************************
*
*    Date       Software Version	Initials	Description
*  2023/08/08       0.0.0	         ItachiVN      Module Created.
*
*******************************************************************************/

/** \file hal_i2c.c
 *  \brief This module contains the
 */
/******************************************************************************
* Includes
*******************************************************************************/
#include "hal.h"
#include "esp_log.h"
#include "driver/i2c.h"

/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define TAG                     "HAL_I2C"
#define I2C_MASTER_NUM          (0)
#define I2C_MASTER_FREQ_HZ      (100000)
#define I2C_SDA_IO              (23)
#define I2C_SCL_IO              (25)
#define I2C_DEFAULT_TIMEOUT     (3000 / portTICK_PERIOD_MS)
/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/

/******************************************************************************
* Function Prototypes
*******************************************************************************/

/******************************************************************************
* Function Definitions
*******************************************************************************/
//Initialize I2C. Returns 0 on success, -1 on failure.
int __initI2C0()
{
    esp_err_t status;
    // I2C on pins IO23/IO25
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
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
    } 
    else 
    {
        ESP_LOGE(TAG, "I2C device does not exist at address 0x%02X ", ADDR);
    }
    return status;
}


int hal__I2CREAD_uint8(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Read data from I2C device at ADDR, register REG. Returns 0 on success, -1 on failure.
    esp_err_t status = i2c_master_write_read_device(i2c_num, ADDR, &REG, 1, data, 1, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C read failed with status %d", status);
        return FAILURE;
    }
    ESP_LOGI(TAG, "hal__I2CREAD_uint8 read at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, *data);
    return SUCCESS;
}

int hal__I2CREAD(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data, uint16_t len)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Read data from I2C device at ADDR, register REG. Returns number of bytes read on success, -1 on failure.
    esp_err_t status = i2c_master_write_read_device(i2c_num, ADDR, &REG, 1, data, len, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C read failed with status %d", status);
        return FAILURE;
    }
    ESP_LOGI(TAG, "hal__I2CREAD read at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, *data);
    return SUCCESS;
}

int hal__I2CWRITE_uint8(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t data)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Write data to I2C device at ADDR, register REG. Returns 0 on success, -1 on failure.
    esp_err_t status = i2c_master_write_to_device(i2c_num, ADDR, &data, 1, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C write failed with status %d", status);
        return FAILURE;
    }
    ESP_LOGI(TAG, "hal__I2CWRITE_uint8 write at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, data);
    return SUCCESS;
}

int hal__I2CWRITE(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data, uint16_t len)
{
    param_check(i2c_num == 0 || i2c_num == 1);
    //Write data to I2C device at ADDR, register REG. Returns number of bytes written on success, -1 on failure.
    esp_err_t status = i2c_master_write_to_device(i2c_num, ADDR, data, len, I2C_DEFAULT_TIMEOUT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C write failed with status %d", status);
        return FAILURE;
    }
    ESP_LOGI(TAG, "hal__I2CWRITE write at address 0x%02X, register 0x%02X, data 0x%02X", ADDR, REG, *data);
    return SUCCESS;
}