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

void wifi_custom__task(void *pvParameters);

void app_main(void)
{
    if(hal__init() != SUCCESS)
    {
        ESP_LOGE("main", "Failed to init HAL");
    }

#if (TEST_WIFI_API == 1)
    xTaskCreate(&wifi_custom__task, "wifi_custom__task", 4096, NULL, 5, NULL);
#endif /* End of (TEST_WIFI_API == 1) */

    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }
}

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

