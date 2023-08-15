/*******************************************************************************
* Title                 :   HAL for PWM
* Filename              :   hal_pwm.c
* Author                :   ItachiVN
* Origin Date           :   2023/08/15
* Version               :   0.0.0
* Compiler              :   ESP-IDF V5.0.2
* Target                :   ESP32 
* Notes                 :   None
*******************************************************************************/

/*************** MODULE REVISION LOG ******************************************
*
*    Date       Software Version	Initials	Description
*  2023/08/15       0.0.0	         ItachiVN      Module Created.
*
*******************************************************************************/

/** \file hal_pwm.c
 *  \brief This module contains the
 */
/******************************************************************************
* Includes
*******************************************************************************/
#include "esp_log.h"
#include "driver/ledc.h"

#include "hal.h"
/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define TAG                     "HAL_PWM"

#define LEDC_DEFAULT_FREQ_HZ    (10000)
#define LEDC_DEFAULT_DUTY_MAX   (1023)
#define LEDC_DEFAULT_BIT_WIDTH  (LEDC_TIMER_10_BIT)
#define LEDC_CHANNELS           (SOC_LEDC_CHANNEL_NUM)
#define LEDC_DEFAULT_CLK        (LEDC_AUTO_CLK)
#define LEDC_MAX_BIT_WIDTH      (SOC_LEDC_TIMER_BIT_WIDE_NUM)

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/

/*
 * LEDC Chan to Group/Channel/Timer Mapping
** ledc: 0  => Group: 0, Channel: 0, Timer: 0
** ledc: 1  => Group: 0, Channel: 1, Timer: 0
** ledc: 2  => Group: 0, Channel: 2, Timer: 1
** ledc: 3  => Group: 0, Channel: 3, Timer: 1
** ledc: 4  => Group: 0, Channel: 4, Timer: 2
** ledc: 5  => Group: 0, Channel: 5, Timer: 2
** ledc: 6  => Group: 0, Channel: 6, Timer: 3
** ledc: 7  => Group: 0, Channel: 7, Timer: 3
** ledc: 8  => Group: 1, Channel: 0, Timer: 0
** ledc: 9  => Group: 1, Channel: 1, Timer: 0
** ledc: 10 => Group: 1, Channel: 2, Timer: 1
** ledc: 11 => Group: 1, Channel: 3, Timer: 1
** ledc: 12 => Group: 1, Channel: 4, Timer: 2
** ledc: 13 => Group: 1, Channel: 5, Timer: 2
** ledc: 14 => Group: 1, Channel: 6, Timer: 3
** ledc: 15 => Group: 1, Channel: 7, Timer: 3
*/
uint8_t channels_resolution[LEDC_CHANNELS] = {0};
static int8_t pin_to_channel[SOC_GPIO_PIN_COUNT] = { 0 };
static int cnt_channel = LEDC_CHANNELS;
/******************************************************************************
* Function Prototypes
*******************************************************************************/

/**
 * @brief Setup a PWM channel
 * @param chan Channel to setup
 * @param freq Frequency in Hz
 * @param bit_num Resolution in bits
 * @return Frequency in Hz or 0 if error
 */
static uint32_t ledcSetup(uint8_t chan, uint32_t freq, uint8_t bit_num)
{
    if(chan >= LEDC_CHANNELS || bit_num > LEDC_MAX_BIT_WIDTH){
        ESP_LOGE(TAG, "No more LEDC channels available! (maximum %u) or bit width too big (maximum %u)", LEDC_CHANNELS, LEDC_MAX_BIT_WIDTH);
        return 0;
    }

    uint8_t group=(chan/8), timer=((chan/2)%4);

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = group,
        .timer_num        = timer,
        .duty_resolution  = bit_num,
        .freq_hz          = freq,
        .clk_cfg          = LEDC_DEFAULT_CLK
    };
    if(ledc_timer_config(&ledc_timer) != ESP_OK)
    {
        ESP_LOGE(TAG, "ledc setup failed!");
        return 0;
    }
    channels_resolution[chan] = bit_num;
    return ledc_get_freq(group,timer);
}

/**
 * @brief Attach a pin to a PWM channel
 * @param pin Pin to attach
 * @param chan Channel to attach
 * @return None
 */
static void ledcAttachPin(uint8_t pin, uint8_t chan)
{
    if(chan >= LEDC_CHANNELS){
        return;
    }
    uint8_t group=(chan/8), channel=(chan%8), timer=((chan/2)%4);
    uint32_t duty = ledc_get_duty(group,channel);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = group,
        .channel        = channel,
        .timer_sel      = timer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = pin,
        .duty           = duty,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

/**
 * @brief Write a PWM channel
 * @param chan Channel to write
 * @param duty Duty cycle
 * @return None
 */
static void ledcWrite(uint8_t chan, uint32_t duty)
{
    if(chan >= LEDC_CHANNELS){
        return;
    }
    uint8_t group=(chan/8), channel=(chan%8);

    //Fixing if all bits in resolution is set = LEDC FULL ON
    uint32_t max_duty = (1 << channels_resolution[chan]) - 1;

    if((duty == max_duty) && (max_duty != 1)){
        duty = max_duty + 1;
    }

    ledc_set_duty(group, channel, duty);
    ledc_update_duty(group, channel);
}

static int8_t analogGetChannel(uint8_t pin) {
    return pin_to_channel[pin];
}

void analogWrite(uint8_t pin, int value) {
    // Use ledc hardware for internal pins
    if (pin < SOC_GPIO_PIN_COUNT) {
        int8_t channel = -1;
        if (pin_to_channel[pin] == 0) {
            if (!cnt_channel) {
                ESP_LOGE(TAG, "No more analogWrite channels available! You can have maximum %d", LEDC_CHANNELS);
                return;
            }
            cnt_channel--;
            channel = cnt_channel;
        } else {
            channel = analogGetChannel(pin);
        }
        ESP_LOGI(TAG, "GPIO %d - Using Channel %d, Value = %d", pin, channel, value);
        if(ledcSetup(channel, LEDC_DEFAULT_FREQ_HZ, LEDC_DEFAULT_BIT_WIDTH) == 0){
            ESP_LOGE(TAG, "analogWrite setup failed (freq = %u, resolution = %u). Try setting different resolution or frequency", LEDC_DEFAULT_FREQ_HZ, LEDC_DEFAULT_BIT_WIDTH);
            return;
        }
        ledcAttachPin(pin, channel);
        pin_to_channel[pin] = channel;
        ledcWrite(channel, value);
    }
}

/******************************************************************************
* Function Definitions
*******************************************************************************/