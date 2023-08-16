#include <stdio.h>

#include "hal.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "assert.h"

#include "hal.h"
#include "nrf_slte.h"

#define TEST_WIFI_API          (0)
#define TEST_GPIO_API          (0)
#define TEST_I2C_API           (0)
#define TEST_ADC_API           (0)

void wifi_custom__task(void *pvParameters);
void gpio_custom__task(void *pvParameters);
void i2c_custom_task(void *pvParameters);
void adc_custom_task(void *pvParameters);

void app_main(void)
{
    if(hal__init() != SUCCESS)
    {
        ESP_LOGE("main", "Failed to init HAL");
    }

#if (TEST_WIFI_API == 1)
    xTaskCreate(&wifi_custom__task, "wifi_custom__task", 4096, NULL, 5, NULL);
#endif /* End of (TEST_WIFI_API == 1) */

#if (TEST_GPIO_API == 1)
    xTaskCreate(&gpio_custom__task, "gpio_custom__task", 4096, NULL, 5, NULL);
#endif /* End of (TEST_GPIO_API == 1) */

#if (TEST_I2C_API == 1)
    xTaskCreate(&i2c_custom_task, "i2c_custom_task", 4096, NULL, 5, NULL);
#endif /* End of (TEST_I2C_API == 1) */

#if (TEST_ADC_API == 1)
    xTaskCreate(&adc_custom_task, "adc_custom_task", 4096, NULL, 5, NULL);
#endif /* End of (TEST_ADC_API == 1) */ 
uint8_t greeting_text[]= "Hello World!\n";
        if( hal__setDutyCycle(26, 1500) == FAILURE)
        {
            ESP_LOGE("main", "Failed to set duty cycle on pin 26");
        }
        if( hal__setDutyCycle(33, 0) == FAILURE)
        {
            ESP_LOGE("main", "Failed to set duty cycle on pin 33");
        }
        if( hal__setDutyCycle(34, 500) == FAILURE)
        {
            ESP_LOGE("main", "Failed to set duty cycle on pin 34");
        }
    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // Test PWM APIS with max duty = 1024. 
        // Duty cycle + 10% each second on pin 33
        for(uint16_t duty = 0; duty <= 1000; duty += 10)
        {
            if( hal__setDutyCycle(33, duty) == FAILURE)
            {
                ESP_LOGE("main", "Failed to set duty cycle on pin 33");
            }
            if(duty == 0)
                vTaskDelay(3000 / portTICK_PERIOD_MS);      
            if(duty == 1000)
                vTaskDelay(3000 / portTICK_PERIOD_MS);      
            else
                vTaskDelay(300 / portTICK_PERIOD_MS);

        }
    }
}

#if (TEST_ADC_API != 0)
#define TEST_ADC_PORT1  0
#define TEST_ADC_PORT2  1
void adc_custom_task(void *pvParameters)
{
    // Read all ADC pins in ADC1 of ESP32 in format 
    // RAW ADC: [PIN36_VALUE PIN37_VALUE PIN38_VALUE PIN39_VALUE PIN32_VALUE PIN33_VALUE PIN34_VALUE PIN35_VALUE]   
    // VOLTAGE: [PIN36_VOLTAGE PIN37_VOLTAGE PIN38_VOLTAGE PIN39_VOLTAGE PIN32_VOLTAGE PIN33_VOLTAGE PIN34_VOLTAGE PIN35_VOLTAGE]
    while(1)
    {


        uint16_t adc_raw[8] = {0};
        uint32_t adc_voltage[8] = {0};

#if (TEST_ADC_PORT1 != 0)

        adc_raw[0] = hal__ADCRead(36);
        adc_raw[1] = hal__ADCRead(37);
        adc_raw[2] = hal__ADCRead(38);
        adc_raw[3] = hal__ADCRead(39);
        adc_raw[4] = hal__ADCRead(32);
        adc_raw[5] = hal__ADCRead(33);
        adc_raw[6] = hal__ADCRead(34);
        adc_raw[7] = hal__ADCRead(35);
        adc_voltage[0] = hal__ADCReadMV(36);
        adc_voltage[1] = hal__ADCReadMV(37);
        adc_voltage[2] = hal__ADCReadMV(38);
        adc_voltage[3] = hal__ADCReadMV(39);
        adc_voltage[4] = hal__ADCReadMV(32);
        adc_voltage[5] = hal__ADCReadMV(33);
        adc_voltage[6] = hal__ADCReadMV(34);
        adc_voltage[7] = hal__ADCReadMV(35);
        ESP_LOGI("[test adc]", "ADC1 mV:[%ld %ld %ld %ld %ld %ld %ld %ld]\n", adc_voltage[0], adc_voltage[1], adc_voltage[2], adc_voltage[3], adc_voltage[4], adc_voltage[5], adc_voltage[6], adc_voltage[7]);

#endif /* End of (TEST_ADC_PORT1 != 0) */

#if (TEST_ADC_PORT2 != 0)
        adc_voltage[0] = hal__ADCReadMV(4);
        adc_voltage[1] = hal__ADCReadMV(0);
        adc_voltage[2] = hal__ADCReadMV(2);
        adc_voltage[3] = hal__ADCReadMV(15);
        adc_voltage[4] = hal__ADCReadMV(13);
        adc_voltage[5] = hal__ADCReadMV(12);
        adc_voltage[6] = hal__ADCReadMV(14);
        adc_voltage[7] = hal__ADCReadMV(27);
        // ESP_LOGI("[test adc]", "RAW: [%d %d %d %d %d %d %d %d]\n", adc_raw[0], adc_raw[1], adc_raw[2], adc_raw[3], adc_raw[4], adc_raw[5], adc_raw[6], adc_raw[7]);
        ESP_LOGI("[test adc2]", "ADC2 mV:[%ld %ld %ld %ld %ld %ld %ld %ld]\n", adc_voltage[0], adc_voltage[1], adc_voltage[2], adc_voltage[3], adc_voltage[4], adc_voltage[5], adc_voltage[6], adc_voltage[7]);
        // adc_raw[4] = hal__ADCRead(39);
        // adc_voltage[4] = hal__ADCReadMV(39);
        // ESP_LOGI("[test adc]", "RAW: [%d] - mV:[%ld]\n", adc_raw[4], adc_voltage[4]);
#endif /* (TEST_ADC_PORT2 != 0) */

        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}
#endif /* End of (TEST_ADC_API == 1) */

#if (TEST_I2C_API != 0)
#define DS3231_I2C_ADDR 	0x68
#define DS3231_REG_CONTROL 	0x0e
#define DS3231_REG_SECOND 	0x00
#define DS3231_REG_MINUTE 	0x01
#define DS3231_REG_HOUR  	0x02
#define DS3231_REG_DOW    	0x03


uint8_t DS3231_DecodeBCD(uint8_t bin) {
	return (((bin & 0xf0) >> 4) * 10) + (bin & 0x0f);
}

/**
 * @brief Encodes a decimal number to binaty-coded decimal for storage in registers.
 * @param dec Decimal number to encode.
 * @return Encoded binary-coded decimal value.
 */
uint8_t DS3231_EncodeBCD(uint8_t dec) {
	return (dec % 10 + ((dec / 10) << 4));
}


int8_t DS3231_GetRegByte(uint8_t regAddr) {
	uint8_t val;
    if (0 != hal__I2CREAD_uint8(0, DS3231_I2C_ADDR, regAddr, &val))
    {
        ESP_LOGE("[test]","I2C Read address 0x%02x failed", regAddr);
        return -1;
    }
    else
    {
        ESP_LOGI("[test]","I2C Read address 0x%02x: %d", regAddr, val);
    }
	return val;
}

int8_t DS3231_SetRegByte(uint8_t regAddr, uint8_t val) {
    if (0 != hal__I2CWRITE_uint8(0, DS3231_I2C_ADDR, regAddr, val))
    {
        ESP_LOGE("[test]","I2C Write address 0x%02x failed", regAddr);
        return -1;
    }
    else
    {
        ESP_LOGI("[test]","I2C Write address 0x%02x: %d", regAddr, val);
    }
    return 0;
}

/**
 * @brief Gets the current hour in 24h format.
 * @return Hour in 24h format, 0 to 23.
 */
uint8_t DS3231_GetHour(void) {
	return DS3231_DecodeBCD(DS3231_GetRegByte(DS3231_REG_HOUR));
}

void DS3231_GetFullTime(uint8_t* hour, uint8_t* minute, uint8_t* second) {
    // Using hal__I2CREAD() to read 3 time bytes (SS:MM:HH)
    uint8_t data[3] = {0};
    if(hal__I2CREAD(0, DS3231_I2C_ADDR, DS3231_REG_SECOND, data, 3))
    // Decode the data
    *hour = DS3231_DecodeBCD(data[2]);
    *minute = DS3231_DecodeBCD(data[1]);
    *second = DS3231_DecodeBCD(data[0]);
    ESP_LOGI("[test]", "Current time: %02d:%02d:%02d", *hour, *minute, *second);
}

void DS3231_SetFullTime(uint8_t hour, uint8_t min, uint8_t sec)
{
    // Encode the data
    uint8_t data[3] = {0};
    data[0] = DS3231_EncodeBCD(sec);
    data[1] = DS3231_EncodeBCD(min);
    data[2] = DS3231_EncodeBCD(hour);
    // Using hal__I2CWRITE() to write 3 time bytes (SS:MM:HH)
    if(hal__I2CWRITE(0, DS3231_I2C_ADDR, DS3231_REG_SECOND, data, 3))
    {
        ESP_LOGE("[test]", "Failed to set time");
    }
    else
    {
        ESP_LOGI("[test]", "Set time: %02d:%02d:%02d", hour, min, sec);
    }
}

/**
 * @brief Set the current hour, in 24h format.
 * @param hour_24mode Hour in 24h format, 0 to 23.
 */
void DS3231_SetHour(uint8_t hour_24mode) {
	DS3231_SetRegByte(DS3231_REG_HOUR, DS3231_EncodeBCD(hour_24mode & 0x3f));
}

void i2c_custom_task(void *pvParameters)
{
    uint8_t data = 0;
    uint8_t i2c_num = 0;
    uint8_t addr = 0x68;
    uint8_t reg = 0x00;
    int test_hour = 1;

    while(1)
    {

        //Test I2C APIs with RTC RS3231
        if( 0 != hal__I2CEXISTS(0, 0x68))
        {
            ESP_LOGE("main", "I2C device does not exist");
        }
        else
        {
            ESP_LOGI("main", "I2C device exists");
        }
        vTaskDelay(1000/ portTICK_PERIOD_MS);

        // Get/Set/Get hour
        uint8_t cur_hour = DS3231_GetHour();
        ESP_LOGI("main", "Current hour: %d", cur_hour);
        vTaskDelay(1000/ portTICK_PERIOD_MS);

        DS3231_SetHour(test_hour++ + 1);
        vTaskDelay(1000/ portTICK_PERIOD_MS);

        cur_hour = DS3231_GetHour();
        ESP_LOGI("main", "Current hour: %d", cur_hour);
        vTaskDelay(1000/ portTICK_PERIOD_MS);

        // Set full time use 
        uint8_t hour = 10;
        uint8_t minute = 11;
        uint8_t second = 12;
        DS3231_SetFullTime(hour, minute, second);
        for(uint8_t count=0; count<20; count++)
        {
            DS3231_GetFullTime(&hour, &minute, &second);
            vTaskDelay(1000/ portTICK_PERIOD_MS);
        }
    }
}
#endif /* End of (TEST_I2C_API == 1) */

#if (TEST_WIFI_API == 1)
void wifi_custom__task(void *pvParameters)
{
    wifi_custom__printCA();
    while(1)
    {
        wifi_custom__power_on();
        wifi_custom__connected();
        wifi_custom__get_time();
        wifi_custom__get_rssi();
        vTaskDelay(30000 / portTICK_PERIOD_MS);

        wifi_custom__power_off();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        wifi_custom__connected();

    }
}
#endif /* End of (TEST_WIFI_API == 1) */

#if (TEST_GPIO_API == 1)
void gpio_custom__task(void *pvParameters)
{
    uint8_t pinNum = 0;
    uint8_t state = 0;
    // Test pin 16-23 as outputs
    for(pinNum = 16; pinNum < 24; pinNum++)
    {
        hal__setState(pinNum, 1);
    }
    // Turn on each pin for 1 second and then of 1 every 1 sec
    for(pinNum = 16; pinNum < 20; pinNum++)
    {
        hal__setHigh(pinNum);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    for(pinNum = 16; pinNum < 20; pinNum++)
    {
        hal__setLow(pinNum);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Test 26 as input pull-down pin
    hal__setState(26, 2);
    // Test 27 as input pull-up pin
    hal__setState(27, 3);
    // Test 32 as HighZ pin
    hal__setState(32, 0);

    while(1)
    {
        // Read pin 26, 27 and 32
        state = hal__read(26);
        printf("Pin 26 state: %d\n", state);
        state = hal__read(27);
        printf("Pin 27 state: %d\n", state);
        state = hal__read(32);
        printf("Pin 32 state: %d\n", state);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
#endif /* End of (TEST_GPIO_API == 1) */
