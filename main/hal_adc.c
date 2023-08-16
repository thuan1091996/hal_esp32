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
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define TAG                         "HAL_ADC"
#define DEFAULT_ADC_ATTEN           ADC_ATTEN_DB_11

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/
static adc_oneshot_unit_handle_t adc_handle[2]={0};
static adc_cali_handle_t adc_calib_handle[2]={0};
static adc_oneshot_chan_cfg_t adc_chann_config_default = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_11,
};
static bool initialized = false;

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static bool __InitADCCalib(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);

/******************************************************************************
* Function Definitions
*******************************************************************************/
#if (ESP_IDF_VERSION_MAJOR >= 5)
int __InitADC()
{
    ESP_LOGI(TAG, "ADC initializing...");
    if(initialized)
    {
        return SUCCESS;
    }

    // Default configs
    adc_oneshot_unit_init_cfg_t init_adc_conf_default = {0};

    // ADC1 init
    init_adc_conf_default.unit_id = ADC_UNIT_1;
    if (ESP_OK != adc_oneshot_new_unit(&init_adc_conf_default, &adc_handle[0]))
    {
        ESP_LOGE(TAG, "ADC1 init failed");
        return FAILURE;
    }
    ESP_LOGI(TAG, "ADC1 Handle %p", adc_handle[0]);

    // ADC2 init
    init_adc_conf_default.unit_id = ADC_UNIT_2;
    if (ESP_OK != adc_oneshot_new_unit(&init_adc_conf_default, &adc_handle[1]))
    {
        ESP_LOGE(TAG, "ADC2 init failed");
        return FAILURE;
    }
    ESP_LOGI(TAG, "ADC2 Handle %p", adc_handle[1]);

    // ADC calibration
    if ( true != __InitADCCalib(ADC_UNIT_1, DEFAULT_ADC_ATTEN, &adc_calib_handle[0]))
    {
        ESP_LOGE(TAG, "ADC1 calibration failed");
        return FAILURE;
    }
    if (true != __InitADCCalib(ADC_UNIT_2, DEFAULT_ADC_ATTEN, &adc_calib_handle[1]))
    {
        ESP_LOGE(TAG, "ADC2 calibration failed");
        return FAILURE;
    }

    initialized = true;
    // Change log level to error
    ESP_LOGI(TAG, "ADC init success");
    esp_log_level_set(TAG, ESP_LOG_ERROR);

    return SUCCESS;
}

int __ADCGetPinInfo(uint8_t pin, adc_unit_t *unit_id, adc_channel_t *channel)
{
    // Check pin input
    if (adc_oneshot_io_to_channel(pin, unit_id, channel) != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC pin invalid");
        return FAILURE;
    }
    if(*unit_id != ADC_UNIT_1 && *unit_id != ADC_UNIT_2)
    {
        ESP_LOGE(TAG, "ADC unit invalid");
        return FAILURE;
    }
    if(*channel >= SOC_ADC_CHANNEL_NUM(*unit_id))
    {
        ESP_LOGE(TAG, "ADC channel invalid");
        return FAILURE;
    }
    ESP_LOGD(TAG, "ADC pin %d, unit %d, channel %d", pin, *unit_id, *channel);
    return SUCCESS;
}

int hal__ADCRead(uint8_t pin)
{
    adc_unit_t adc_port;
    adc_channel_t adc_channel;
    if( __ADCGetPinInfo(pin, &adc_port, &adc_channel) != SUCCESS)
    {
        return FAILURE;
    }

    //Config channel
    if(ESP_OK != adc_oneshot_config_channel(adc_handle[adc_port], adc_channel, &adc_chann_config_default))
    {
        ESP_LOGE(TAG, "ADC1 config channel failed");
        return FAILURE;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    int adc_raw;
    //Read ADC value
    if(ESP_OK != adc_oneshot_read(adc_handle[adc_port], adc_channel, &adc_raw))
    {
        ESP_LOGE(TAG, "ADC1 read failed");
        return FAILURE;
    }
    ESP_LOGI(TAG, "ADC at pin %d: %d", pin, adc_raw);
    return adc_raw;
}

int hal__ADCReadMV(uint8_t pin)
{
    adc_unit_t adc_port;
    adc_channel_t adc_channel;
    if( __ADCGetPinInfo(pin, &adc_port, &adc_channel) != SUCCESS)
    {
        return FAILURE;
    }

    int raw_adc = hal__ADCRead(pin);
    int adc_mv = -1;
    if(raw_adc == FAILURE)
    {
        return FAILURE;
    }

    //Convert raw to mV
    if(adc_cali_raw_to_voltage(adc_calib_handle[adc_port], raw_adc, &adc_mv) != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC1 convert raw to mV failed");
        return FAILURE;
    }
    ESP_LOGI(TAG, "ADC at pin %d: %d mV", pin, adc_mv);
    return adc_mv;
}

static bool __InitADCCalib(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "ADC %d calibration Success", unit+1);
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

#else  /* VERSION 4 */
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

int __InitADC()
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


int hal__ADCRead(uint8_t pin)
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

uint32_t hal__ADCReadMV(uint8_t pin){
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


#endif /* End of ESP_IDF_VERSION_MAJOR >= 5 */

