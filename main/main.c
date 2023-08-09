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
#define TEST_GPIO_API          (1)

void wifi_custom__task(void *pvParameters);
void gpio_custom__task(void *pvParameters);

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
