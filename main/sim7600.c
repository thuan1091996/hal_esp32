/******************************************************************************
* Includes
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal.h"

/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define AT_DEFAULT_UART_PORT            (2)
#define AT_DEFAULT_TIMEOUT_MS           (10000UL)
#define AT_BUFFER_SIZE                  (1024UL)
#define AT_FLUSH_RX_BEFORE_WRITE        (1) /* If set to 1, clear data in RX buffer before send AT cmd*/
/******************************************************************************
* Module configurations
*******************************************************************************/
#define TEST_USED_SAMPLE_HTTP_RESP      (0) /* Set to 1 overwrite response data with HTTP_RESP_EXAMPLE[] */
#define TEST_AT_DEBUG_PRINTF            (1) /* Set to 1 to print log msg using printf()*/     
#define TEST_DUMP_DATA_RECV             (1) /* Set to 1 to print data received in mailbox */


#define PORT_DELAY_MS(MS)               (vTaskDelay(MS / portTICK_PERIOD_MS))
#define PORT_GET_SYSTIME_MS()           (xTaskGetTickCount() * portTICK_PERIOD_MS)

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/
#define MIN(a,b) ((a) < (b) ? (a) : (b))


#if (TEST_AT_DEBUG_PRINTF == 1)
#define SIM7600_PRINTF(args...)                 printf(args)
#else  /* !(TEST_AT_DEBUG_PRINTF == 1) */
#define SIM7600_PRINTF(args...)                 (void)
#endif /* End of (TEST_AT_DEBUG_PRINTF == 1) */

// Specifically for ESP_IDF
#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp_log.h"

#define TAG                             "LTE_DRIVER"
#define SIM7600_INFO_PRINTF(args...)    ESP_LOGI(TAG, args)
#define SIM7600_INFO_PRINT_HEX(data, len) ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO)
#else /* !(CONFIG_IDF_TARGET_ESP32) */

#define SIM7600_INFO_PRINTF(args...)    (void)
#define SIM7600_INFO_PRINT_HEX(data, len) (void)
#endif /* End of (CONFIG_IDF_TARGET_ESP32) */


#if (TEST_USED_SAMPLE_HTTP_RESP == 1)
const char HTTP_RESP_EXAMPLE[] =  
									"#XHTTPCRSP:341,0\r\n"
									// header
									"HTTP/1.1 200 OK\r\n"
									"Date: Thu, 11 Mar 2021 04:36:19 GMT\r\n"
									"Content-Type: application/json; charset=utf-8\r\n"
									"Content-Length: 244\r\n"
									"Connection: keep-alive\r\n"
									"ETag: W/\"f4-ZKlqfH53aEj3f4zb0kDtYvHD+XU\"\r\n"
									"Vary: Accept-Encoding\r\n"
									"set-cookie: sails.sid=s%3AHGcBwpqlDDUZhU16VzuQkfTMhWhA4W1T.%2Bgm1%2BezKGo2JnWxaB5yYDo%2FNh0NbnJzJjEnkMcrfdEI; Path=/; HttpOnly\r\n"
									// body
									"#XHTTPCRSP:243,0\r\n"
									"{\"args\":{\"foo1\":\"bar1\",\"foo2\":\"bar2\"},\"headers\":{\"x-forwarded-proto\":\"http\",\"x-forwarded-port\":\"80\",\"host\":\"postman-echo.com\",\"x-amzn-trace-id\":\"Root=1-60499e43-67a96f1e18fec45b1db78c25\"},\"url\":\"http://postman-echo.com/get?foo1=bar1&foo2=bar2\"}\r\n"
									"#XHTTPCRSP:1,0\r\n"
									"}\r\n"
									"#XHTTPCRSP:0,1\r\n";
#endif /* End of (TEST_USED_SAMPLE_HTTP_RESP == 1) */


/******************************************************************************
* Module Typedefs
*******************************************************************************/
typedef struct 
{
    char rx_data[AT_BUFFER_SIZE];
    uint16_t rx_len;
} at_resp_data_mailbox_t;

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/
at_resp_data_mailbox_t at_rx_data = {0};

/******************************************************************************
* Mailbox functions for AT response data
*******************************************************************************/
int mailbox__get_len(at_resp_data_mailbox_t* mailbox)
{
    param_check(mailbox != NULL);
    return mailbox->rx_len;
}

int mailbox__put_data(at_resp_data_mailbox_t* mailbox, char* data, uint16_t len)
{
    uint16_t actual_len;
    param_check(mailbox != NULL);
    param_check(data != NULL);
    if(mailbox->rx_len + len > sizeof(mailbox->rx_data))
    {
        // Re-calulate the length to put into mailbox, discard the new data
        actual_len = sizeof(mailbox->rx_data) - mailbox->rx_len;
        SIM7600_PRINTF("mailbox__put_data(), Mailbox overflow, discarded %dB\n", len - actual_len);
        // Print the discarded data
        SIM7600_PRINTF("Discarded data:");
        for(int i = actual_len; i < len; i++)
        {
            SIM7600_PRINTF("%c", data[i]);
        }
        SIM7600_PRINTF("\r\n");
    }
    else
    {
        actual_len = len;
    }
    memcpy(&mailbox->rx_data[mailbox->rx_len], data, actual_len);
    mailbox->rx_len += len;
    return SUCCESS;
}

int mailbox__get_data(at_resp_data_mailbox_t* mailbox, char* data, uint16_t len)
{
    param_check(mailbox != NULL);
    param_check(data != NULL);
    if(mailbox->rx_len < len)
        return FAILURE;
    memcpy(data, mailbox->rx_data, len);
    mailbox->rx_len -= len;
    memmove(mailbox->rx_data, &mailbox->rx_data[len], mailbox->rx_len);
    return mailbox->rx_len;
}

int mailbox__flush(at_resp_data_mailbox_t* mailbox)
{
    param_check(mailbox != NULL);
    mailbox->rx_len = 0;
    memset(mailbox->rx_data, 0, sizeof(mailbox->rx_data));
    return SUCCESS;
}

int mailbox_logdata(at_resp_data_mailbox_t* mailbox)
{
    param_check(mailbox != NULL);
    SIM7600_INFO_PRINTF("%s", mailbox->rx_data);
    return SUCCESS;
}

//Returns the stack size of the current task. Returns current stack size on success, -1 on failure.
int hal__getStackSize(void)
{
    UBaseType_t remaining_stack =  uxTaskGetStackHighWaterMark(NULL);
    SIM7600_PRINTF("Remaining stack: %dB", remaining_stack);
    return remaining_stack;
}
/******************************************************************************
* Internal Function Prototypes
*******************************************************************************/
int __sim7600__send_command(char* command);
int __sim7600__wait_4response(char *expected_resp, uint16_t timeout_ms);
int __sim7600__get_resp(char* resp, uint16_t maxlength);
int __sim7600__clearCert();

int __sim7600__cal_rssi_from_cesq(char* cesq_response);
int __sim7600__get_http_content_len(const char* resp);

int __sim7600__httpsGET_send_req(char* url);
int __sim7600__httpsPOST_send_req(char* url, char* JSONdata, char* agent);
/******************************************************************************
* Internal Function Definitions
*******************************************************************************/
/**
 * @brief Get the rssi from CESQ response string
 * @param cesq_response 
 * @return RSSI in dBm
 * @note: More on: https://devzone.nordicsemi.com/f/nordic-q-a/71958/how-know-rssi
 */
int __sim7600__cal_rssi_from_cesq(char* cesq_response)
{
    param_check(cesq_response != NULL);
    int rsrq, rsrp, rssi;
    int status = sscanf(cesq_response, "+CESQ: %*d,%*d,%*d,%*d,%d,%d", &rsrq, &rsrp);
    if(status != 2)
        return FAILURE;
    if( (rsrp == 255) && (rsrq == 255) )
        return FAILURE; // Invalid response
    rssi = (double)(10 * log10(6) + (rsrp - 140) - (rsrq - 19.5));
    return rssi;
}

/**
 * @brief Get the unix timestamp from datetime object   
 * 
 * @param datetime 
 * @return FAILURE if error, else unix timestamp in seconds
 */
long __sim7600__get_unix_timestamp(const char* datetime)
{
    int year, month, day, hour, minute, second;
    param_check(datetime != NULL);
    if(sscanf(datetime, "UTC_TIME: %d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) != 6)
    {
        SIM7600_PRINTF("Invalid datetime format\n");
        return FAILURE;
    }

    struct tm current_time = {0};
    current_time.tm_year = year - 1900; // Year since 1900
    current_time.tm_mon = month - 1; // Months since January [0-11]
    current_time.tm_mday = day; 
    current_time.tm_hour = hour;
    current_time.tm_min = minute;
    current_time.tm_sec = second;

    time_t t = mktime(&current_time);

    // Check if time conversion is successful
    if(t == -1){
        SIM7600_PRINTF("Error: unable to make time using mktime\n");
        return FAILURE;
    }

    return (long)t;
}

/**
 * @brief Parse the URL to get the host and path
 * 
 * @param url 
 * @param host 
 * @param path 
 */
int __sim7600__parse_url(const char* url, char* host, char* path)
{
    param_check(url != NULL);
    param_check(host != NULL);
    param_check(path != NULL);

    /* Extract hostname and path from URL */
    const char* slash = strchr(url, '/');
    if (slash) 
    { 
        // '/' found, Get the length of the host by subtracting pointers
        uint8_t host_len = slash - url;
        memcpy(host, url, host_len);
        host[host_len] = '\0';
        strcpy(path, slash);
    }
    else 
    {
        // No '/' found, assume that the entire URL is the host
        strcpy(host, url);
        path[0] = '/';
        path[1] = '\0';
    }
    return SUCCESS;
}

/**
 * @brief: Get the content length from the response header lines
 * 
 * @param resp: The response header from the server 
 * @return int: The content length of the response body
 */
int __sim7600__get_http_content_len(const char* resp)
{
    param_check(resp != NULL);
    int content_length;
    char* content_length_str = strstr(resp, "Content-Length: ");
    if(content_length_str == NULL)
        return FAILURE; // Invalid response
    if (sscanf(content_length_str, "Content-Length: %d", &content_length) != 1)
    {
        SIM7600_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }
    return content_length;
}


int __sim7600__send_command(char* command)
{
    param_check(command != NULL);
#if (AT_FLUSH_RX_BEFORE_WRITE != 0)  //Clear AT RX buffer before sending command
    hal__UARTFlushRX(AT_DEFAULT_UART_PORT);
    mailbox__flush(&at_rx_data);        
#endif /* End of (AT_FLUSH_RX_BEFORE_WRITE != 0) */
    return hal__UARTWrite(AT_DEFAULT_UART_PORT, (uint8_t*) command, strnlen(command, AT_BUFFER_SIZE));
}

/**
 * @brief Waits for a response from the LTE modem and checks if it matches the expected response.
 * 
 * @param expected_resp The expected response from the LTE module.
 * @param timeout_ms The maximum time to wait for the response in milliseconds.
 * @return int Returns SUCCESS if the expected response is received within the timeout period, otherwise returns FAILURE.
 */

int __sim7600__wait_4response(char* expected_resp, uint16_t timeout_ms)
{
    uint8_t rcv_buf[AT_BUFFER_SIZE] = {0};  // Temp buffer to receive data from UART
    uint16_t recv_idx = 0;
    uint64_t max_recv_timeout = PORT_GET_SYSTIME_MS() + timeout_ms;
    // Wait until maximum AT_DEFAULT_TIMEOUT_MS to receive expected_resp
#if (TEST_DUMP_DATA_RECV == 1)
SIM7600_INFO_PRINTF(" ======================================================================== ");
#endif /* End of (TEST_DUMP_DATA_RECV == 1) */
    while (PORT_GET_SYSTIME_MS() < max_recv_timeout)
    {
        uint16_t resp_len = hal__UARTAvailable(AT_DEFAULT_UART_PORT);
        if(recv_idx + resp_len > sizeof(rcv_buf) )
        {
            SIM7600_PRINTF("__sim7600__wait_4response(), Buffer overflow, discarded %dB\n", recv_idx + resp_len - sizeof(rcv_buf));
            // Read until full and discard newest data
            resp_len = sizeof(rcv_buf) - recv_idx;
        }
        if(resp_len > 0)
        {
            hal__UARTRead(AT_DEFAULT_UART_PORT, &rcv_buf[recv_idx], resp_len);
            recv_idx += resp_len;
#if (TEST_DUMP_DATA_RECV == 1)
            SIM7600_INFO_PRINT_HEX(&rcv_buf[recv_idx - resp_len], resp_len);
SIM7600_INFO_PRINTF(" ======================================================================== ");
#endif /* End of (TEST_DUMP_DATA_RECV == 1) */
            if(strstr((char*)rcv_buf, "ERR") != NULL)
                return FAILURE; // Error response received
            else if(strstr((char*)rcv_buf, expected_resp) != NULL)
            {
                mailbox__put_data(&at_rx_data, (char*)rcv_buf, recv_idx);
                return SUCCESS;
            }
            // If buffer full without receiving expected_resp, log and return FAILURE
            if(recv_idx == sizeof(rcv_buf))
            {
                SIM7600_PRINTF("__sim7600__wait_4response(), Buffer full without receiving \"%s\"\n",expected_resp);
                SIM7600_PRINTF("Try to increase buffer, current buffer payload: ");
                for(uint8_t idx=0; idx<recv_idx; idx++)
                {
                    SIM7600_PRINTF("%c",rcv_buf[idx]);
                }
                SIM7600_PRINTF("\r\n");
                return FAILURE;
            }
        }
        PORT_DELAY_MS(100); // Wait for UART buffer to fill up
    }
    return FAILURE;
}


int __sim7600__get_resp(char* resp, uint16_t maxlength)
{
    param_check(resp != NULL);
    uint16_t cur_len = mailbox__get_len(&at_rx_data);
    if(cur_len > maxlength)
    {
        SIM7600_PRINTF("__sim7600__get_resp(), Buffer overflow, current buffer length: %dB, maxlength: %dB, discarded %dB\n",
                        cur_len, maxlength, cur_len - maxlength);
        cur_len = maxlength;
    }
    if( cur_len != 0)
    {
        cur_len = mailbox__get_data(&at_rx_data, resp, cur_len);
        mailbox__flush(&at_rx_data);
    }
    return cur_len;
}

int __sim7600__clearCert()
{
    if (__sim7600__send_command("AT+CFUN=4\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    // Check (list) to see if cert exists
    if (__sim7600__send_command("AT%CMNG=1,12354,0\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __sim7600__get_resp(resp, sizeof(resp));
    char* cert_str = strstr(resp, "%CMNG: 12354, 0");
    if(cert_str == NULL)
        return SUCCESS; // Cert does not exist, return success
    else
    {
        if (__sim7600__send_command("AT%CMNG=3,12354,0\r\n") != SUCCESS)
            return FAILURE; // Failed to send command

        if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
            return FAILURE; // Failed to receive resp (no "OK" received within timeout")
    }
    return SUCCESS;
}

/******************************************************************************
* Function Definitions
*******************************************************************************/
/*
 *  Implements [AT+CFUN=1] (Enables LTE modem.)
 *  Waits for "OK" response. Returns SUCCESS if ok. Returns FAILURE if error.
 */
int sim7600__power_on(void)
{
    if (__sim7600__send_command("AT+CFUN=1\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    return SUCCESS;
} 

/*
 * Implements [AT+CFUN=0] (Disables LTE modem.)
 * Waits for "OK" response. Returns SUCCESS if ok. Returns FAILURE if error.
 */
int sim7600__power_off(void)
{
    // CFUN=0 causes writing to NVM. When using CFUN=0, take NVM wear into account
    //  => Ready curent mode first to avoid unnecessary NVM writes  
    // Check the current functional mode, if already in power off mode, return SUCCESS. Otherwise force to power off
    do
    {
        if (__sim7600__send_command("AT+CFUN?\r\n") != SUCCESS)
            break;
        if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
            break;

        char resp[AT_BUFFER_SIZE] = {0};
        int resp_len = __sim7600__get_resp(resp, sizeof(resp));

        int cur_func_mode;
        char* cur_func_mode_str = strstr(resp, "+CFUN: ");
        if(cur_func_mode_str == NULL)
            break;

        //Get first token to extract current functional mode
        cur_func_mode_str = strtok(cur_func_mode_str, "\n");
        if(cur_func_mode_str == NULL)
            break; // Invalid response 
        if(sscanf(cur_func_mode_str, "+CFUN: %d ", &cur_func_mode) != 1)
            break; // Invalid response
        
        if(cur_func_mode == 0)
            return SUCCESS; // Already in power off mode

    }while(0);

    if (__sim7600__send_command("AT+CFUN=0\r\n") != SUCCESS)
        return FAILURE; // Failed to send command
    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")
        
    return SUCCESS;
}

/*
 * Implements [AT+CESQ] (Signal Quality Report), Waits for Response, Returns RSSI value in dBm.
 * https://infocenter.nordicsemi.com/topic/ref_at_commands/REF/at_commands/mob_termination_ctrl_status/cesq_set.html 
 * Example:
 * "AT+CESQ"
 * "+CESQ: 99,99,255,255,31,62 <CR><LF> OK"
 */
int sim7600__get_rssi(void)
{
    if (__sim7600__send_command("AT+CESQ\r\n") != SUCCESS)
        return FAILURE; // Failed to send command
    
    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __sim7600__get_resp(resp, sizeof(resp));

    char* cesq_response_str = strstr(resp, "+CESQ: ");
    if(cesq_response_str == NULL)
        return FAILURE; // Invalid response

    //Get first token to extract RSSI
    cesq_response_str = strtok(cesq_response_str, "\n");
    if(cesq_response_str == NULL)
        return FAILURE; // Invalid response
    
    int rssi = __sim7600__cal_rssi_from_cesq(cesq_response_str);

    return rssi;
}

/*
 * Implements [AT+COPS?] 
 * Expected Response: +COPS: (<status>,"long","short","numeric",<AcT>) // AcT - Access Technology
 * returns 1 if connected, 0 if not connected, -1 if error 
 * //https://infocenter.nordicsemi.com/topic/ref_at_commands/REF/at_commands/nw_service/cops_read.html
 */
int sim7600__connected(void)
{
    if (__sim7600__send_command("AT+COPS?\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __sim7600__get_resp(resp, sizeof(resp));

    char* cops_response_str = strstr(resp, "ERROR");
    if(cops_response_str != NULL)
        return FAILURE; // Invalid response

    cops_response_str = strstr(resp, "+COPS: ");
    if(cops_response_str == NULL)
        return FAILURE; // Invalid response

    int network_status;
    if (sscanf(cops_response_str, "+COPS: %d", &network_status) != 1)
        return FAILURE;

    if(network_status == 2)
        return 1; // Connected
    else
        return 0; // Not connected
    return FAILURE;
}

/*
 * Send +CPIN=? 
 * returns 1 if SIM is present (Resp == "READY"), 0 if not present (???), -1 if error
 */
int sim7600__get_SimPresent(void)
{
    if (__sim7600__send_command("AT+CPIN?\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("READY", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "READY" received within timeout")

    return 1;
}

/*
 * Implements [AT#CARRIER="time","read"] to get time returns seconds since UTC time 0. 
 * https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/CARRIER_AT_commands.html#lwm2m-carrier-library-xcarrier
 */
long sim7600__get_time(void)
{
    if (__sim7600__send_command("AT#XCARRIER=\"time\"\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if(__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __sim7600__get_resp(resp, sizeof(resp));

    // Parse response given example: #XCARRIER: UTC_TIME: 2022-12-30T14:56:46Z, UTC_OFFSET: 60, TIMEZONE: Europe/Paris
    char* utc_date_time_str = strstr(resp, "UTC_TIME: ");
    if(utc_date_time_str == NULL)
        return FAILURE; // Invalid response

    long unix_timestamp = __sim7600__get_unix_timestamp(utc_date_time_str);
    if(unix_timestamp == FAILURE)
        return FAILURE; // Invalid response

    return unix_timestamp;
}

/*
 * calls "__sim7600__clearCert()" to clear the certificate, if it exists.
 * Then, Implements [AT%CMNG=0,12354,0,\"<ca>\"] to set CA certificate,
 *  where <ca> represents the contents of ca, with the null terminator removed.
 *  Then, enables the modem using "sim7600__power_on()" Returns 0 if ok. Returns -1 if error.
 * https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/Generic_AT_commands.html#native-tls-cmng-xcmng
 */
int sim7600__setCA(char* ca)
{
    if (__sim7600__clearCert() != SUCCESS)
        return FAILURE; // Failed to clear certificate 

    char at_tx_buffer[AT_BUFFER_SIZE] = {0};
    snprintf(at_tx_buffer, sizeof(at_tx_buffer), "AT%%CMNG=0,12354,0,%s\r\n", ca); //FAULT
    if (__sim7600__send_command(at_tx_buffer) != SUCCESS)
        return FAILURE; // Failed to send command
        
    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")
    if ( sim7600__power_on() != SUCCESS)
        return FAILURE; // Failed to power on
    return SUCCESS;
}

/**
 * @brief Parse the response from the server and extract content
 *
 * @param resp_input: The response from the modem
 * @param resp_output: The HTTP response output
 * @param body_max_size: The maximum size of the body
 */
int __sim7600__https_parse_response(char* resp_input, char* resp_output, int resp_max_len)
{
    char* p_process = NULL;
    int recv_content_len = 0;
    int recv_state = 0;
    int recv_cur_len = 0;
    // Split the response by newline characters
    char *line = strtok((char *)resp_input, "\n");

    while (line != NULL)
    {
        p_process = strstr(line, "#XHTTPCRSP:");
        if(p_process != NULL)
        {
            // Get received length and status
            if (sscanf(p_process, "#XHTTPCRSP:%d,%d", &recv_content_len, &recv_state) != 2)
            {
                SIM7600_PRINTF("Invalid response, current processing %s\n", line);
                continue;
            }
            if(recv_state == 0)
            {
//            	SIM7600_PRINTF("Receiving %dB \n", recv_content_len);
            }
            else if(recv_state == 1)
                return SUCCESS;
        }
        else //Processing HTTP response
        {
            // Check total length of response to prevent buffer overflow
            if(recv_cur_len + strlen(line) > resp_max_len)
            {
                SIM7600_PRINTF("__sim7600__https_parse_response(), Buffer overflow by %d \n", recv_cur_len + strlen(line) - resp_max_len);
                // Fill until resp_output[resp_max_len] is full and discard the rest
                if(recv_cur_len < resp_max_len)
                    strncat(resp_output, line, resp_max_len - recv_cur_len);    //Fill until resp_output[resp_max_len] is full
                recv_cur_len += strlen(line); // Make sure that in the end, we know how many bytes are lost due to buffer overflow
            }
            else
            {
                // Append response to response buffer
                strcat(resp_output, line);
                recv_cur_len += strlen(line);
                // Append "\n" which removed by strtok()
                if(recv_cur_len + 1 < resp_max_len)
                {
                    strcat(resp_output, "\n");
                    recv_cur_len += 1;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    return FAILURE;
}

/**
 * @brief Connect and Send HTTPS GET request to the server
 * 
 * @param url: The URL to send the GET request to
 * @return int SUCCESS if ok. Returns FAILURE if error.
 */
int __sim7600__httpsGET_send_req(char* url)
{
    int status;
    char resp[AT_BUFFER_SIZE] = {0};
    char at_send_buffer[255] = {0};
    char host[100] = {0};
    char path[100] = {0};
    char* response_data;

    /* Extract hostname and path from URL */
    __sim7600__parse_url(url, host, path);
    SIM7600_PRINTF("Host: %s\n", host);
    SIM7600_PRINTF("Path: %s\n", path);

    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCCON=1,\"%s\",443,12354\r\n", host);
    // Connect to HTTPS server using IPv4
    if (__sim7600__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command
	
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    int resp_len = __sim7600__get_resp(resp, sizeof(resp));

    response_data = strstr(resp, "#XHTTPCCON");
    if(response_data == NULL)
        return FAILURE; // Invalid response

    if (sscanf(response_data, "#XHTTPCCON: %d", &status) != 1)
    {
        SIM7600_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }
    if(status != 1)
        return FAILURE; // Failed to connect to server

    // Connected to server, send GET request
    memset(at_send_buffer, 0, strlen(at_send_buffer));
    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCREQ=\"GET\",\"%s\"\r\n", path);
    if (__sim7600__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__sim7600__wait_4response("#XHTTPCREQ", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    resp_len = __sim7600__get_resp(resp, sizeof(resp));

    response_data = strstr(resp, "XHTTPCREQ");
    if(response_data == NULL)
    	return FAILURE;
    if (sscanf(response_data, "XHTTPCREQ: %d", &status) != 1)
    {
        SIM7600_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }

    if(status < 0)
        return FAILURE; // Failed to send request
    
    return SUCCESS;
}

/* 
 * then implements [AT#XHTTPCREQ=\"POST\",\"/myurl\",\"User-Agent: <agent>\r\n\",\"application/json\","<JSONdata>\"]  where
 * <agent>:     is the contents of agent, with the null terminator removed
 * <JSONdata>:  is the contents of JSONdata, with the null terminator removed. 
 * Returns      0 if ok. Returns -1 if error. Returns response in response char array. 
 * https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/HTTPC_AT_commands.html
*/
int __sim7600__httpsPOST_send_req(char* url, char* JSONdata, char* agent)
{
    int status;
    char resp[AT_BUFFER_SIZE] = {0};
    char at_send_buffer[255] = {0};
    char host[100] = {0};
    char path[100] = {0};
    char* response_data;

    /* Extract hostname and path from URL */
    __sim7600__parse_url(url, host, path);
    SIM7600_PRINTF("Host: %s\n", host);
    SIM7600_PRINTF("Path: %s\n", path);

    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCCON=1,\"%s\",443,12354\r\n", host);
    // Connect to HTTPS server using IPv4
    if (__sim7600__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__sim7600__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    int resp_len = __sim7600__get_resp(resp, sizeof(resp));

    response_data = strstr(resp, "#XHTTPCCON");
    if(response_data == NULL)
        return FAILURE; // Invalid response

    if (sscanf(response_data, "#XHTTPCCON: %d", &status) != 1)
    {
        SIM7600_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }

    if(status != 1)
        return FAILURE; // Failed to connect to server

    // Connected to server, send POST request
    memset(at_send_buffer, 0, strlen(at_send_buffer));
    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCREQ=\"POST\",\"%s\",\"User-Agent: %s\r\n\Content-Type: application/json\r\n\"", path, agent);
    if (__sim7600__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("#XHTTPCREQ", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    resp_len = __sim7600__get_resp(resp, sizeof(resp));
    response_data = strstr(resp, "XHTTPCREQ");
    if(response_data == NULL)
        return FAILURE;
    if(sscanf(response_data, "XHTTPCREQ: %d", &status) != 1)
    {
        SIM7600_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }
    if(status != 1) // Send data payload
        return FAILURE;

    memset(at_send_buffer, 0, strlen(at_send_buffer));
    snprintf(at_send_buffer, sizeof(at_send_buffer), "%s\r\n", JSONdata);
    if (__sim7600__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    if (__sim7600__wait_4response("#XHTTPCREQ", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    resp_len = __sim7600__get_resp(resp, sizeof(resp));
    response_data = strstr(resp, "XHTTPCREQ");
    if(response_data == NULL)
        return FAILURE;
    if(sscanf(response_data, "XHTTPCREQ: %d", &status) != 1)
    {
        SIM7600_PRINTF("Invalid response format\n");
        return FAILURE; // Invalid response
    }
        
    if(status < 0)
        return FAILURE; // Failed to send request
        
    return SUCCESS;
}

/* 
 * If url = "google.com/myurl" Implements [AT#XHTTPCCON=1,\"google.com\",443,12354], waits for a valid reply
 * then implements [AT#XHTTPCREQ=\"POST\",\"/myurl\",\"User-Agent: <agent>\r\n\",\"application/json\","<JSONdata>\"]  where
 * <agent>:     is the contents of agent, with the null terminator removed
 * <JSONdata>:  is the contents of JSONdata, with the null terminator removed. 
 * Returns      0 if ok. Returns -1 if error. Returns response in response char array. 
 * https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/HTTPC_AT_commands.html
*/
int sim7600__httpsPOST(char* url, char* JSONdata, char* agent, char* http_response, uint16_t maxlength)
{
    if(__sim7600__httpsPOST_send_req(url, JSONdata, agent) != SUCCESS)
    {
        SIM7600_PRINTF("Failed to send HTTP POST request \n");
        return FAILURE; // Failed to send request
    }
    char resp[AT_BUFFER_SIZE] = {0};
    
    mailbox__flush(&at_rx_data);
	
	vTaskDelay(3000 / portTICK_PERIOD_MS);

    if (__sim7600__wait_4response("#XHTTPCRSP:0,1", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
    {
        SIM7600_PRINTF("Failed to receive full HTTP response within timeout, trying to parse whatever receive... \n");
    }

    int resp_len = __sim7600__get_resp(resp, sizeof(resp));
        
    #if (TEST_USED_SAMPLE_HTTP_RESP == 1)   // Overwrite resp with sample HTTP response
    memset(resp, 0, AT_BUFFER_SIZE);
    strncpy(resp, HTTP_RESP_EXAMPLE, AT_BUFFER_SIZE);
    #endif /* End of (TEST_USED_SAMPLE_HTTP_RESP == 1) */


    int ret_val = __sim7600__https_parse_response(resp, http_response, maxlength - 1);
    SIM7600_PRINTF("HTTP Response : \n %s\n", http_response);
    return ret_val;

}

int sim7600__httpsGET(char* url, char* http_response, uint16_t maxlength)
{
    
     if(__sim7600__httpsGET_send_req(url) != SUCCESS)
     {
         SIM7600_PRINTF("Failed to send HTTP GET request \n");
         return FAILURE; // Failed to send request
     }
    char resp[AT_BUFFER_SIZE] = {0};

    SIM7600_PRINTF("Waiting for HTTP response... \n");
    mailbox__flush(&at_rx_data);

	vTaskDelay(3000 / portTICK_PERIOD_MS);
    if (__sim7600__wait_4response("#XHTTPCRSP:0,1", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
    {
        SIM7600_PRINTF("Failed to receive full HTTP response within timeout, trying to parse whatever receive... \n");
    }
    int resp_len = __sim7600__get_resp(resp, sizeof(resp));
    
    #if (TEST_USED_SAMPLE_HTTP_RESP == 1)   // Overwrite resp with sample HTTP response
    memset(resp, 0, AT_BUFFER_SIZE);
    strncpy(resp, HTTP_RESP_EXAMPLE, AT_BUFFER_SIZE);
    #endif /* End of (TEST_USED_SAMPLE_HTTP_RESP == 1) */

    int ret_val = __sim7600__https_parse_response(resp, http_response, maxlength - 1);
    SIM7600_PRINTF("HTTP Response : \n %s\n", http_response);

    return ret_val;
}

volatile uint8_t g_test = 0;
extern const char howmyssl_ca[];
extern const char httpbin_ca[];
void lte_modem_custom_task(void *pvParameters)
{
    SIM7600_PRINTF("Testing HTTP Parser started \r\n");
    SIM7600_PRINTF("************************************* START *************************************");
    SIM7600_PRINTF("\n\r");
    SIM7600_PRINTF("\n\r");
    
    while(1) 
    {
        int temp_var = 1;
        switch(g_test)
        {
            case 0:
            {
                SIM7600_PRINTF("[TESTING] HTTPS GET \r\n");
                char url[] = "howsmyssl.com/a/check";
                char http_resp_buffer[2048] = {0};
                sim7600__httpsGET(url , http_resp_buffer, sizeof(http_resp_buffer));
                SIM7600_PRINTF("\r\n===================================================\r\n");
                SIM7600_PRINTF("HTTP Response: %s\n", http_resp_buffer);
                SIM7600_PRINTF("\r\n===================================================\r\n");
                break;
            }

            case 1:
            {
                SIM7600_PRINTF("[TESTING] HTTPS POST \r\n");
                char http_resp_buffer[2048] = {0};
                char agent[] = "slm";
                char url[] = "httpbin.org/post";
                char json_data[] = "{\"foo1\":\"bar1\",\"foo2\":\"bar2\"}";
                sim7600__httpsPOST(url, json_data, agent, http_resp_buffer, sizeof(http_resp_buffer));
                SIM7600_PRINTF("\r\n===================================================\r\n");
                SIM7600_PRINTF("HTTP Response: %s\n", http_resp_buffer);
                SIM7600_PRINTF("\r\n===================================================\r\n");
                break;
            }

            default:
            {
                SIM7600_PRINTF("Invalid test case \r\n");
                g_test = 0;
                break;
            }
        }
        SIM7600_PRINTF("\r\n===================================================\r\n");
        g_test = (g_test + 1) % 2;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    SIM7600_PRINTF("************************************* END ***************************************");
}
