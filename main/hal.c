
/*------------------------------------------------------------------------------*/
/*							 Includes and dependencies						    */
/*------------------------------------------------------------------------------*/
#include "hal.h"

/*------------------------------------------------------------------------------*/
/*					  	   Function prototypes Implement					    */
/*------------------------------------------------------------------------------*/

/***************************** INIT_HELPER_FUNCTIONS ****************************/
int hal__init()
{
    int ret = __InitUART();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "UART init failed");
    }
    ret = __initI2C();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "I2C init failed");
    }
    ret = wifi_custom_init();
    if(ret != SUCCESS)
    {
        ESP_LOGE(TAG, "WIFI init failed");
    }
    return ret;
}