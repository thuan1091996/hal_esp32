
/*------------------------------------------------------------------------------*/
/*							 Includes and dependencies						    */
/*------------------------------------------------------------------------------*/
#include "hal.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

/*------------------------------------------------------------------------------*/
/*					  	   Function prototypes Implement					    */
/*------------------------------------------------------------------------------*/
#define TAG "HAL"

/***************************** INIT_HELPER_FUNCTIONS ****************************/
int nvs_init()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);
    if (ESP_OK != ret)
    {
        ESP_LOGE("nvs", "Failed to initialize NVS Flash. Erasing and re-initializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        if (nvs_flash_init() != ESP_OK)
        {
            ESP_LOGE("nvs", "Failed to erase and re-initialize NVS Flash. Aborting...");
            return FAILURE;
        }
    }
    ESP_LOGI("nvs", "NVS Flash initialized \r\n");   
    return SUCCESS;
}

int hal__init()
{
    int ret = __InitUART();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "UART init failed");
    }
    ret = __InitI2C();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "I2C init failed");
    }
    
    ret = __InitADC();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "ADC init failed");
    }

    ret = nvs_init();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "NVS init failed");
    }

    ret = wifi_custom_init();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "WIFI init failed");
    }
    return ret;
}