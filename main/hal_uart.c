/*******************************************************************************
* Title                 :    
* Filename              :   hal_uart.c
* Origin Date           :   2023/07/18
* Version               :   0.0.0
* Compiler              :   ESP-IDF v5.x
* Target                :   ESP32
* Notes                 :   None
*******************************************************************************/

/******************************************************************************
* Includes
*******************************************************************************/
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hal.h"
/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define MODULE_NAME                         "HAL_UART"
#define UART_BUFFER_SIZE					(512U)

#define UART_TX1_PIN						(GPIO_NUM_1)
#define UART_RX1_PIN						(GPIO_NUM_3)
#define UART_TX2_PIN						(GPIO_NUM_17)
#define UART_RX2_PIN						(GPIO_NUM_16)

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
/*int __initUART1()
{
    int ret = SUCCESS;

    Configure parameters of an UART driver 
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    Set the UART parameters 
    esp_err_t err = uart_param_config(UART_NUM_1, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(MODULE_NAME, "Failed to initialize UART1 parameters");
        return FAILURE;
    }

    Setup UART IO 
    err = uart_set_pin(UART_NUM_1, UART_TX1_PIN, UART_RX1_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(MODULE_NAME, "Failed to set pins for UART1");
        return FAILURE;
    }

	Setup UART buffered 
    err = uart_driver_install(UART_NUM_1, UART_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(MODULE_NAME, "Failed to install UART1 driver");
        return FAILURE;
    }

    return ret;
}*/

int __initUART2()
{
    int ret = SUCCESS;

    /* Configure parameters of an UART driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,

    };

    /* Set the UART parameters */
    esp_err_t err = uart_param_config(UART_NUM_2, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(MODULE_NAME, "Failed to initialize UART2 parameters");
        return FAILURE;
    }

    /* Setup UART IO */
    err = uart_set_pin(UART_NUM_2, UART_TX2_PIN, UART_RX2_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(MODULE_NAME, "Failed to set pins for UART2");
        return FAILURE;
    }

    /* Setup UART buffered */
    err = uart_driver_install(UART_NUM_2, UART_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(MODULE_NAME, "Failed to install UART2 driver");
        return FAILURE;
    }

    return ret;
}

int __InitUART()
{
    int ret = SUCCESS;

	/*if(SUCCESS != __initUART1())
	{
		ESP_LOGE(MODULE_NAME, "Failed to init UART1");
		return FAILURE;
	}*/

	if(SUCCESS != __initUART2())
	{
		printf("Failed to init UART2\n\r");
		return FAILURE;
	}
    return ret;
}

int hal__UARTAvailable(uint8_t uartNum)
{
	param_check( (1 <= uartNum) && (uartNum <= 2) );
	int buffered_size = 0;
	uart_get_buffered_data_len(uartNum, (size_t *)&buffered_size);
	return buffered_size;
}

//Write data to UART. Returns 0 on success, -1 on failure.
int hal__UARTWrite_uint8(uint8_t uartNum, uint8_t data)
{
	param_check( (1 <= uartNum) && (uartNum <= 2) );
	if (uart_write_bytes(uartNum, (const void *)&data, 1) < 0)
		return FAILURE;
	else
		return SUCCESS;
}

//Write data to UART. Returns number of bytes written on success, -1 on failure.
int hal__UARTWrite(uint8_t uartNum, uint8_t *data, uint16_t len)
{
	param_check( (1 <= uartNum) && (uartNum <= 2) );
	param_check( NULL != data );
	param_check( 0 < len );
	if (uart_write_bytes(uartNum, (const void *)data, len) < 0)
		return FAILURE;
	else
		return SUCCESS;
}

//Read data from UART. Returns 0 on success, -1 on failure.
int hal__UARTRead_uint8(uint8_t uartNum, uint8_t *data)
{
	param_check( (1 <= uartNum) && (uartNum <= 2) );
	param_check( NULL != data );
	if (uart_read_bytes(uartNum, data, 1, 0) < 0)
		return FAILURE;
	else
		return SUCCESS;
}

//Read data from UART. Returns number of bytes read on success, -1 on failure.
int hal__UARTRead(uint8_t uartNum, uint8_t *data, uint16_t len)
{
	param_check( (1 <= uartNum) && (uartNum <= 2) );
	param_check( NULL != data );
	param_check( 0 < len );
	int read_len = 0;
	read_len = uart_read_bytes(uartNum, data, len, 0);
	if (read_len < 0)
		return FAILURE;
	else
		return read_len;
}