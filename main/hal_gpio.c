/*******************************************************************************
* Title                 :   HAL GPIO
* Filename              :   hal_gpio.c
* Author                :   ItachiVN
* Origin Date           :   2023/08/08
* Version               :   0.0.0
* Compiler              :   ESP-IDF V5.0.2
* Target                :   ESP32 
* Notes                 :   None
*******************************************************************************/

/*************** MODULE REVISION LOG ******************************************
*
*    Date       Software Version	Initials	Description
*  2023/08/08       0.0.0	         ItachiVN      Module Created.
*
*******************************************************************************/

/******************************************************************************
* Includes
*******************************************************************************/
#include "hal.h"
#include "driver/gpio.h"
/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/

/******************************************************************************
* Function Prototypes
*******************************************************************************/

/******************************************************************************
* Function Definitions
*******************************************************************************/
// More on: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

// state = 0 -> High impedance, 1 -> Output, 2 -> Input pull-down, 3 -> Input pull-up
int hal__setState(uint8_t pinNum, uint8_t state)
{
    param_check(GPIO_IS_VALID_GPIO(pinNum));
    param_check( (0 <= state) && (state <= 3) );

    gpio_num_t gpio_num = pinNum;
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << gpio_num),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    if(state == 0)
    {
        io_config.mode = GPIO_MODE_INPUT;
    }
    else if(state == 1)
    {
        io_config.mode = GPIO_MODE_OUTPUT;
    }
    else if(state == 2)
    {
        // High impedance -> input with pull-down
        io_config.mode = GPIO_MODE_INPUT;
        io_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    }
    else if (state == 3)
    {
        io_config.mode = GPIO_MODE_INPUT;
        io_config.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    else
    {
        return FAILURE;
    }

    if (gpio_config(&io_config) != ESP_OK)
    {
        return FAILURE;
    }
    return SUCCESS;
}

int hal__setHigh(uint8_t pinNum)
{
    param_check(GPIO_IS_VALID_OUTPUT_GPIO(pinNum));
    gpio_num_t gpio_num = pinNum;
    if (gpio_set_level(gpio_num, 1) != ESP_OK)
    {
        return FAILURE;
    }
    return SUCCESS;
}

int hal__setLow(uint8_t pinNum)
{
    param_check(GPIO_IS_VALID_OUTPUT_GPIO(pinNum));
    gpio_num_t gpio_num = pinNum;
    if (gpio_set_level(gpio_num, 0) != ESP_OK)
    {
        return FAILURE;
    }
    return SUCCESS;
}

int hal__read(uint8_t pinNum)
{
    param_check(GPIO_IS_VALID_GPIO(pinNum));
    gpio_num_t gpio_num = pinNum;
    return gpio_get_level(gpio_num);
}