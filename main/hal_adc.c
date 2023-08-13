/*******************************************************************************
* Title                 :   ADC HAL for ESP32 using ESP-IDF
* Filename              :   hal_adc.c
* Author                :   ItachiVN
* Origin Date           :   2023/08/09
* Version               :   0.0.0
* Compiler              :   ESP-IDF V5.0.2
* Target                :   ESP32 
* Notes                 :   None
*******************************************************************************/

/*************** MODULE REVISION LOG ******************************************
*
*    Date       Software Version	Initials	Description
*  2023/08/09       0.0.0	         ItachiVN      Module Created.
*
*******************************************************************************/

/** \file hal_adc.c
 *  \brief This module contains the hardware abstraction layer for ADC.
 */
/******************************************************************************
* Includes
*******************************************************************************/
#include "hal.h"
#include "sdkconfig.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#include "soc/adc_periph.h"
#include "soc/touch_sensor_periph.h"
/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define TAG             "HAL_ADC"
#define DEFAULT_VREF    1100

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/
static esp_adc_cal_characteristics_t adc1_chars, adc2_chars;
static bool cali_enable = FAILURE;



static uint8_t __analogAttenuation = 3;//11db
static uint8_t __analogWidth = ADC_WIDTH_MAX - 1; //3 for ESP32/ESP32C3; 4 for ESP32S2
static uint8_t __analogReturnedWidth = 12; 

static int __analogVRef = 0;
#if CONFIG_IDF_TARGET_ESP32
static uint8_t __analogVRefPin = 0;
#endif
/******************************************************************************
* Function Prototypes
*******************************************************************************/

/******************************************************************************
* Function Definitions
*******************************************************************************/
int8_t digitalPinToAnalogChannel(uint8_t pin) 
{
    uint8_t channel = 0;
    if (pin < SOC_GPIO_PIN_COUNT) {
        for (uint8_t i = 0; i < SOC_ADC_PERIPH_NUM; i++) {
            for (uint8_t j = 0; j < SOC_ADC_MAX_CHANNEL_NUM; j++) {
                if (adc_channel_io_map[i][j] == pin) {
                    return channel;
                }
                channel++;
            }
        }
    }
    return -1;
}

int8_t digitalPinToTouchChannel(uint8_t pin) 
{
    int8_t status = -1;
    if (pin < SOC_GPIO_PIN_COUNT) {
        for (uint8_t i = 0; i < SOC_TOUCH_SENSOR_NUM; i++) {
            if (touch_sensor_channel_io_map[i] == pin) {
                status = i;
                break;
            }
        }
    }
    return status;
}

static inline uint16_t mapResolution(uint16_t value)
{
    uint8_t from = __analogWidth + 9;
    if (from == __analogReturnedWidth) {
        return value;
    }
    if (from > __analogReturnedWidth) {
        return value >> (from  - __analogReturnedWidth);
    }
    return value << (__analogReturnedWidth - from);
}


//todo:
/*
- Add ADC
- Sorry about delay
- Send code with I2C integrate
- Test uart
*/
int __analogInit()
{
    static bool initialized = FAILURE;
    ESP_LOGI(TAG, "ADC init");
    if(initialized)
    {
        return SUCCESS;
    }

    esp_err_t status = ESP_OK;

    status = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF);
    if (status == ESP_ERR_NOT_SUPPORTED) 
    {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    } 
    else if (status == ESP_ERR_INVALID_VERSION) 
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } 
    else if (status == ESP_OK) 
    {
        cali_enable = true;
        ESP_LOGI(TAG, "Detect reference voltage stored in eFuse, enable software calibration");
    }
    else 
    {
        ESP_LOGE(TAG, "Invalid arg");
    }

    // Calibrate ADC1, ADC2
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc2_chars);


    //Configure all ADC1 pins to ADC_ATTEN_DB_11
    for(uint8_t i = ADC1_CHANNEL_0; i < ADC1_CHANNEL_MAX; i++)
    {
        if (ESP_OK != adc1_config_channel_atten(i, ADC_ATTEN_DB_11))
        {
            ESP_LOGE(TAG, "Failed to configure ADC1 channel %d", i);
            return FAILURE;
        }
    }

    //Configure all ADC2 pins to ADC_ATTEN_DB_11
    for(uint8_t i = ADC2_CHANNEL_0; i < ADC2_CHANNEL_MAX; i++)
    {
        if (ESP_OK != adc2_config_channel_atten(i, ADC_ATTEN_DB_11))
        {
            ESP_LOGE(TAG, "Failed to configure ADC2 channel %d", i);
            return FAILURE;
        }
    }

    status = adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    if(status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC1 width with status %d", status);
        return FAILURE;
    }

    initialized = true;
    // Change log level to error
    ESP_LOGI(TAG, "ADC init success");
    esp_log_level_set(TAG, ESP_LOG_ERROR);

    return SUCCESS;
}

bool __adcAttachPin(uint8_t pin){
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0){
        ESP_LOGE(TAG,"Pin %u is not ADC pin!", pin);
        return FAILURE;
    }
    __analogInit();
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad >= 0)
    {
        uint32_t touch = READ_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG);
        if(touch & (1 << pad)){
            touch &= ~((1 << (pad + SENS_TOUCH_PAD_OUTEN2_S))
                    | (1 << (pad + SENS_TOUCH_PAD_OUTEN1_S))
                    | (1 << (pad + SENS_TOUCH_PAD_WORKEN_S)));
            WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG, touch);
        }
    }
    return true;
}


int __analogRead(uint8_t pin)
{
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0){
        ESP_LOGE(TAG, "Pin %u is not ADC pin!", pin);
        return FAILURE;
    }
    esp_err_t r = ESP_OK;

    int value = 0;
    __adcAttachPin(pin);
    if(channel > (SOC_ADC_MAX_CHANNEL_NUM - 1)){
        channel -= SOC_ADC_MAX_CHANNEL_NUM;
        r = adc2_get_raw( channel, __analogWidth, &value);
        if ( r == ESP_OK ) {
            return mapResolution(value);
        } else if ( r == ESP_ERR_INVALID_STATE ) {
            ESP_LOGE(TAG,"GPIO%u: %s: ADC2 not initialized yet.", pin, esp_err_to_name(r));
        } else if ( r == ESP_ERR_TIMEOUT ) {
            ESP_LOGE(TAG,"GPIO%u: %s: ADC2 is in use by Wi-Fi. Please see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-limitations for more info", pin, esp_err_to_name(r));
        } else {
            ESP_LOGE(TAG,"GPIO%u: %s", pin, esp_err_to_name(r));
        }
    } else {
        value = adc1_get_raw(channel);
        return mapResolution(value);
    }
    return mapResolution(value);
}

uint32_t __analogReadMilliVolts(uint8_t pin){
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0){
        ESP_LOGE(TAG,"Pin %u is not ADC pin!", pin);
        return 0;
    }

    if(!__analogVRef){
        if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
            ESP_LOGD(TAG, "eFuse Two Point: Supported");
            __analogVRef = DEFAULT_VREF;
        }
        if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
            ESP_LOGD(TAG, "eFuse Vref: Supported");
            __analogVRef = DEFAULT_VREF;
        }
        if(!__analogVRef){
            __analogVRef = DEFAULT_VREF;

            #if CONFIG_IDF_TARGET_ESP32
            if(__analogVRefPin){
                esp_adc_cal_characteristics_t chars;
                if(adc_vref_to_gpio(ADC_UNIT_2, __analogVRefPin) == ESP_OK){
                    __analogVRef = __analogRead(__analogVRefPin);
                    esp_adc_cal_characterize(1, __analogAttenuation, __analogWidth, DEFAULT_VREF, &chars);
                    __analogVRef = esp_adc_cal_raw_to_voltage(__analogVRef, &chars);
                    ESP_LOGD(TAG, "Vref to GPIO%u: %u", __analogVRefPin, __analogVRef);
                }
            }
            #endif
        }
    }
    uint8_t unit = 1;
    if(channel > (SOC_ADC_MAX_CHANNEL_NUM - 1)){
        unit = 2;
    }

    int adc_reading = __analogRead(pin);

    esp_adc_cal_characteristics_t chars = {};
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, ADC_ATTEN_DB_11, __analogWidth, __analogVRef, &chars);

    static bool print_chars_info = true;
    if(print_chars_info)
    {
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            ESP_LOGI(TAG, "ADC%u: Characterized using Two Point Value: %u\n", (unsigned int)unit, (unsigned int) chars.vref);
        } 
        else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            ESP_LOGI(TAG, "ADC%u: Characterized using eFuse Vref: %u\n", (unsigned int)unit, (unsigned int) chars.vref);
        } 
        #if CONFIG_IDF_TARGET_ESP32
        else if(__analogVRef != DEFAULT_VREF){
            ESP_LOGI(TAG, "ADC%u: Characterized using Vref to GPIO%u: %u\n", unit, __analogVRefPin, (unsigned int) chars.vref);
        }
        #endif
        else {
            ESP_LOGI(TAG, "ADC%u: Characterized using Default Vref: %u\n", (unsigned int)unit, (unsigned int) chars.vref);
        }
        print_chars_info = FAILURE;
    }
    return esp_adc_cal_raw_to_voltage((uint32_t)adc_reading, &chars);
}

void __analogSetVRefPin(uint8_t pin){
    if(pin <25 || pin > 27){
        pin = 0;
    }
    __analogVRefPin = pin;
}
