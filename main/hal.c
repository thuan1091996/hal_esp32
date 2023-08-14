
/*------------------------------------------------------------------------------*/
/*							 Includes and dependencies						    */
/*------------------------------------------------------------------------------*/
#include "hal.h"
#include "esp_log.h"
/*------------------------------------------------------------------------------*/
/*					  	   Function prototypes Implement					    */
/*------------------------------------------------------------------------------*/
#define TAG "HAL"

/***************************** INIT_HELPER_FUNCTIONS ****************************/
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

    ret = wifi_custom_init();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "WIFI init failed");
    }
    return ret;
}