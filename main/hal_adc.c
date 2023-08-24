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

    vTaskDelay(pdMS_TO_TICKS(100));
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


