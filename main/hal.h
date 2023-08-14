#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------------*/
/*							 Includes and dependencies						    */
/*------------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "wifi_custom.h"
/*------------------------------------------------------------------------------*/
/*					  		   Preprocessor Constants						    */
/*------------------------------------------------------------------------------*/
#define SUCCESS                 0
#define FAILURE                 -1

#define param_check(param)	    if ( !(param) ) return FAILURE
#define error_check(con, error) if ( con ) return error


/*-----------------------------------------------------------------------------*/
/*							    Function prototypes					     	   */
/*-----------------------------------------------------------------------------*/






/* HAL Framework for */


int hal__init(); //Initialize HAL. Returns 0 on success, -1 on failure.
int __InitUART();
int __InitI2C();
int __InitADC();

/* TIMER_HELPER_FUNCTIONS */
int hal__setDutyCycle(uint8_t channelNum, uint16_t dutyCycle_tenth); //Set Duty Cycle, in tenths of percent. For Example, Passing (1, 50) will set Timer 1 Channel 1 to 5.0%. Returns 0 on success, -1 on failure.
//Note: channelNum should be 0-indexed. (0,1,2,3,4,5,6,7,8) - not related to the real physical pins or timer peripherals.
//Note: Setting this value should not directly replace the capture/compare value - it should save this value and replace the capture/compare value on the next timer overflow.

/* GPIO_HELPER_FUNCTIONS */
int hal__setHigh(uint8_t pinNum);
int hal__setLow(uint8_t pinNum);
int hal__read(uint8_t pinNum);

int hal__setState(uint8_t pinNum, uint8_t state); //(0,0) sets pin 0 as input, (0,1) sets pin 0 as output, (0,2) sets pin as high impedance. Returns 0 on success, -1 on failure.
//Note: link pinNum to the datasheet labels in a logical way PA[0:15], PB[0:15], PC[0:15], etc.

/* UART_HELPER_FUNCTIONS */
int hal__UARTAvailable(uint8_t uartNum); //Returns number of bytes available to read from UART. Returns 0 on success, -1 on failure.  //NOTE: if API limitations only allow for knowing IF data is available, return 1 if data is available.

int hal__UARTWrite_uint8(uint8_t uartNum, uint8_t data); //Write data to UART. Returns 0 on success, -1 on failure.
int hal__UARTWrite(uint8_t uartNum, uint8_t *data, uint16_t len); //Write data to UART. Returns number of bytes written on success, -1 on failure.

int hal__UARTRead_uint8(uint8_t uartNum, uint8_t *data); //Read data from UART. Returns 0 on success, -1 on failure.
int hal__UARTRead(uint8_t uartNum, uint8_t *data, uint16_t len); //Read data from UART. Returns number of bytes read on success, -1 on failure.

/* I2C_HELPER_FUNCTIONS */
bool hal__I2CEXISTS(uint8_t i2c_num, uint8_t ADDR); //Returns true if I2C device exists at ADDR, false if not.

int hal__I2CREAD_uint8(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data); //Read data from I2C device at ADDR, register REG. Returns 0 on success, -1 on failure.
int hal__I2CREAD(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data, uint16_t len); //Read data from I2C device at ADDR, register REG. Returns number of bytes read on success, -1 on failure.

int hal__I2CWRITE_uint8(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t data); //Write data to I2C device at ADDR, register REG. Returns 0 on success, -1 on failure.
int hal__I2CWRITE(uint8_t i2c_num, uint8_t ADDR, uint8_t REG, uint8_t *data, uint16_t len); //Write data to I2C device at ADDR, register REG. Returns number of bytes written on success, -1 on failure.

/* ADC_HELPER_FUNCTIONS */
int hal__ADCRead(uint8_t pin);
int hal__ADCReadMV(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */
