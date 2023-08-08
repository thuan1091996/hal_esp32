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

void app_main(void)
{
    if(hal__init() != SUCCESS)
    {
        ESP_LOGE("main", "Failed to init HAL");
    }
    
    while(1)
    {
        wifi_custom__power_on();
        wifi_custom__connected();
        wifi_custom__get_time();
        wifi_custom__get_rssi();
        vTaskDelay(30000 / portTICK_PERIOD_MS);

        wifi_custom__power_off();
        wifi_custom__connected();
    }
}

