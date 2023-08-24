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
#define AT_HTTP_HEADER_MAX_SIZE         (1024UL)
#define AT_HTTP_BODY_MAX_SIZE           (1024UL)
#define AT_FLUSH_RX_BEFORE_WRITE        (1) /* If set to 1, clear data in RX buffer before send AT cmd*/
/******************************************************************************
* Module configurations
*******************************************************************************/
#define TEST_USED_SAMPLE_HTTP_RESP      (0) /* Set to 1 overwrite response data with HTTP_RESP_EXAMPLE[] */
#define TEST_AT_DEBUG_PRINTF            (1) /* Set to 1 to print log msg using printf()*/     
#define TEST_DUMP_DATA                  (1) /* Set to 1 to print data received in mailbox */


#define PORT_DELAY_MS(MS)               (vTaskDelay(MS / portTICK_PERIOD_MS))
#define PORT_GET_SYSTIME_MS()           (xTaskGetTickCount() * portTICK_PERIOD_MS)

//TODO: Remove __nrf_slte_ -> __nrf_slte_, nrf_slte__ -> nrf_slte__, NRF_SLM_PRINTF -> NRF_SLM_PRINTF

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/
#define MIN(a,b) ((a) < (b) ? (a) : (b))


#if (TEST_AT_DEBUG_PRINTF == 1)
#define NRF_SLM_PRINTF(args...)                 printf(args)
#else  /* !(TEST_AT_DEBUG_PRINTF == 1) */
#define NRF_SLM_PRINTF(args...)                 (void)
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
char HTTP_RESP_EXAMPLE[]= "HTTP/1.1 200 OK\n"
                            "Date: Tue, 01 Mar 2022 05:22:28 GMT\n"
                            "Content-Type: application/json; charset=utf-8\n"
                            "Content-Length: 359\n"
                            "Connection: keep-alive\n"
                            "ETag: W/\"167-2YuosrP0ARLW1c5oeDiW7MId014\"\n"
                            "Vary: Accept-Encoding\n"
                            "set-cookie: sails.sid=s%3A_b9-1rOslsmoczQUGjv93SicuBw8f6lb.x%2B6xkThAVld5%2FpykDn7trZ9JGh%2Fir3MVU0izYBfB0Kg; Path=/; HttpOnly\n"
                            "\n"
                            "#XHTTPCRSP:342,1\n"
                            "{\"args\":{},\"data\":{\"hello\":\"world\"},\"files\":{},\"form\":{},\"headers\":{\"x-forwarded-proto\":\"http\",\"x-forwarded-port\":\"80\",\"host\":\"postman-echo.com\",\"x-amzn-trace-id\":\"Root=1-621dad94-2fcac1637dc28f172c6346e6\",\"content-length\":\"17\",\"user-agent\":\"slm\",\"accept\":\"*/*\",\"content-type\":\"application/json\"},\"json\":{\"hello\":\"world\"},\"url\":\"http://postman-echo.com/post\"}\n"
                            "#XHTTPCRSP:359,1\n";
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
        NRF_SLM_PRINTF("mailbox__put_data(), Mailbox overflow, discarded %dB\n", len - actual_len);
        // Print the discarded data
        NRF_SLM_PRINTF("Discarded data:");
        for(int i = actual_len; i < len; i++)
        {
            NRF_SLM_PRINTF("%c", data[i]);
        }
        NRF_SLM_PRINTF("\r\n");
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
    NRF_SLM_PRINTF("Remaining stack: %dB", remaining_stack);
    return remaining_stack;
}
/******************************************************************************
* Internal Function Prototypes
*******************************************************************************/
int __nrf_slte__send_command(char* command);
int __nrf_slte__wait_4response(char *expected_resp, uint16_t timeout_ms);
int __nrf_slte__get_resp(char* resp, uint16_t maxlength);
int __nrf_slte__clearCert();

int __nrf_slte__cal_rssi_from_cesq(char* cesq_response);
int __nrf_slte__get_http_content_len(const char* resp);

int __nrf_slte_httpsGET_send_req(char* url);
int __nrf_slte_httpsPOST_send_req(char* url, char* JSONdata, char* agent);
int __nrf_slte_https_parse_resp(char* resp_input, char* response_header, int header_max_size, char* response_body, int body_max_size);
/******************************************************************************
* Internal Function Definitions
*******************************************************************************/
/**
 * @brief Get the rssi from CESQ response string
 * @param cesq_response 
 * @return RSSI in dBm
 * @note: More on: https://devzone.nordicsemi.com/f/nordic-q-a/71958/how-know-rssi
 */
int __nrf_slte__cal_rssi_from_cesq(char* cesq_response) 
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
long __nrf_slte__get_unix_timestamp(const char* datetime)
{
    int year, month, day, hour, minute, second;
    param_check(datetime != NULL);
    if(sscanf(datetime, "UTC_TIME: %d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) != 6)
    {
        NRF_SLM_PRINTF("Invalid datetime format\n");
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
        NRF_SLM_PRINTF("Error: unable to make time using mktime\n");
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
int __nrf_slte__parse_url(const char* url, char* host, char* path)
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
int __nrf_slte__get_http_content_len(const char* resp)
{
    param_check(resp != NULL);
    int content_length;
    char* content_length_str = strstr(resp, "Content-Length: ");
    if(content_length_str == NULL)
        return FAILURE; // Invalid response
    if (sscanf(content_length_str, "Content-Length: %d", &content_length) != 1)
    {
        NRF_SLM_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }
    return content_length;
}


int __nrf_slte__send_command(char* command)
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
 * @param expected_resp The expected response from the SIM7600 module.
 * @param timeout_ms The maximum time to wait for the response in milliseconds.
 * @return int Returns SUCCESS if the expected response is received within the timeout period, otherwise returns FAILURE.
 */

int __nrf_slte__wait_4response(char* expected_resp, uint16_t timeout_ms)
{
    uint8_t rcv_buf[AT_BUFFER_SIZE] = {0};  // Temp buffer to receive data from UART
    uint16_t recv_idx = 0;
    uint64_t max_recv_timeout = PORT_GET_SYSTIME_MS() + timeout_ms;
    // Wait until maximum AT_DEFAULT_TIMEOUT_MS to receive expected_resp
#if (TEST_DUMP_DATA == 1)
SIM7600_INFO_PRINTF(" ======================================================================== ");
#endif /* End of (TEST_DUMP_DATA == 1) */
    while (PORT_GET_SYSTIME_MS() < max_recv_timeout)
    {
        uint16_t resp_len = hal__UARTAvailable(AT_DEFAULT_UART_PORT);
        if(recv_idx + resp_len > sizeof(rcv_buf) )
        {
            NRF_SLM_PRINTF("__nrf_slte__wait_4response(), Buffer overflow, discarded %dB\n", recv_idx + resp_len - sizeof(rcv_buf));
            // Read until full and discard newest data
            resp_len = sizeof(rcv_buf) - recv_idx;
        }
        if(resp_len > 0)
        {
            hal__UARTRead(AT_DEFAULT_UART_PORT, &rcv_buf[recv_idx], resp_len);
            recv_idx += resp_len;
#if (TEST_DUMP_DATA == 1)
            SIM7600_INFO_PRINT_HEX(&rcv_buf[recv_idx - resp_len], resp_len);
SIM7600_INFO_PRINTF(" ======================================================================== ");
#endif /* End of (TEST_DUMP_DATA == 1) */
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
                NRF_SLM_PRINTF("__nrf_slte__wait_4response(), Buffer full without receiving \"%s\"\n",expected_resp);
                NRF_SLM_PRINTF("Try to increase buffer, current buffer payload: ");
                for(uint8_t idx=0; idx<recv_idx; idx++)
                {
                    NRF_SLM_PRINTF("%c",rcv_buf[idx]);
                }
                NRF_SLM_PRINTF("\r\n");
                return FAILURE;
            }
        }
        PORT_DELAY_MS(100); // Wait for UART buffer to fill up
    }
    return FAILURE;
}


int __nrf_slte__get_resp(char* resp, uint16_t maxlength)
{
    param_check(resp != NULL);
    uint16_t cur_len = mailbox__get_len(&at_rx_data);
    if( cur_len != 0)
    {
        cur_len = mailbox__get_data(&at_rx_data, resp, cur_len);
        mailbox__flush(&at_rx_data);
    }
    return cur_len;
}

int __nrf_slte__clearCert()
{
    if (__nrf_slte__send_command("AT+CFUN=4\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    // Check (list) to see if cert exists
    if (__nrf_slte__send_command("AT%CMNG=1,12354,0\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));
    char* cert_str = strstr(resp, "%CMNG: 12354, 0");
    if(cert_str == NULL)
        return SUCCESS; // Cert does not exist, return success
    else
    {
        if (__nrf_slte__send_command("AT%CMNG=3,12354,0\r\n") != SUCCESS)
            return FAILURE; // Failed to send command

        if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
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
int nrf_slte__power_on(void)
{
    if (__nrf_slte__send_command("AT+CFUN=1\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    return SUCCESS;
} 

/*
 * Implements [AT+CFUN=0] (Disables LTE modem.)
 * Waits for "OK" response. Returns SUCCESS if ok. Returns FAILURE if error.
 */
int nrf_slte__power_off(void)
{
    // CFUN=0 causes writing to NVM. When using CFUN=0, take NVM wear into account
    //  => Ready curent mode first to avoid unnecessary NVM writes  
    // Check the current functional mode, if already in power off mode, return SUCCESS. Otherwise force to power off
    do
    {
        if (__nrf_slte__send_command("AT+CFUN?\r\n") != SUCCESS)
            break;
        if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
            break;

        char resp[AT_BUFFER_SIZE] = {0};
        int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

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

    if (__nrf_slte__send_command("AT+CFUN=0\r\n") != SUCCESS)
        return FAILURE; // Failed to send command
    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
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
int nrf_slte__get_rssi(void)
{
    if (__nrf_slte__send_command("AT+CESQ\r\n") != SUCCESS)
        return FAILURE; // Failed to send command
    
    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

    char* cesq_response_str = strstr(resp, "+CESQ: ");
    if(cesq_response_str == NULL)
        return FAILURE; // Invalid response

    //Get first token to extract RSSI
    cesq_response_str = strtok(cesq_response_str, "\n");
    if(cesq_response_str == NULL)
        return FAILURE; // Invalid response
    
    int rssi = __nrf_slte__cal_rssi_from_cesq(cesq_response_str);

    return rssi;
}

/*
 * TODO: Implement
 * Implements [AT+COPS?] 
 * Expected Response: +COPS: (<status>,"long","short","numeric",<AcT>) // AcT - Access Technology
 * returns 1 if connected, 0 if not connected, -1 if error 
 * //https://infocenter.nordicsemi.com/topic/ref_at_commands/REF/at_commands/nw_service/cops_read.html
 */
int nrf_slte__connected(void)
{
    if (__nrf_slte__send_command("AT+COPS?\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

    char* cops_response_str = strstr(resp, "ERROR");
    if(cops_response_str != NULL)
        return FAILURE; // Invalid response

    cops_response_str = strstr(resp, "+COPS: ");
    if(cops_response_str == NULL)
        return FAILURE; // Invalid response

    // TODO: Need more work
    // Retrieve network status
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
int nrf_slte__get_SimPresent(void)
{
    if (__nrf_slte__send_command("AT+CPIN?\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if (__nrf_slte__wait_4response("READY", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "READY" received within timeout")

    return 1;
}

/*
 * TODO: Implement
 * Implements [AT#CARRIER="time","read"] to get time returns seconds since UTC time 0. 
 * https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/CARRIER_AT_commands.html#lwm2m-carrier-library-xcarrier
 */
long nrf_slte__get_time(void)
{
    if (__nrf_slte__send_command("AT#XCARRIER=\"time\"\r\n") != SUCCESS)
        return FAILURE; // Failed to send command

    if(__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    char resp[AT_BUFFER_SIZE] = {0};
    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

    // Parse response given example: #XCARRIER: UTC_TIME: 2022-12-30T14:56:46Z, UTC_OFFSET: 60, TIMEZONE: Europe/Paris
    char* utc_date_time_str = strstr(resp, "UTC_TIME: ");
    if(utc_date_time_str == NULL)
        return FAILURE; // Invalid response

    long unix_timestamp = __nrf_slte__get_unix_timestamp(utc_date_time_str);
    if(unix_timestamp == FAILURE)
        return FAILURE; // Invalid response

    return unix_timestamp;
}

/*
 * calls "__nrf_slte__clearCert()" to clear the certificate, if it exists. 
 * Then, Implements [AT%CMNG=0,12354,0,\"<ca>\"] to set CA certificate,
 *  where <ca> represents the contents of ca, with the null terminator removed.
 *  Then, enables the modem using "nrf_slte__power_on()" Returns 0 if ok. Returns -1 if error. 
 * https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/Generic_AT_commands.html#native-tls-cmng-xcmng
 */
int nrf_slte__setCA(char* ca)
{
    if (__nrf_slte__clearCert() != SUCCESS)
        return FAILURE; // Failed to clear certificate 

    char at_tx_buffer[AT_BUFFER_SIZE] = {0};
    snprintf(at_tx_buffer, sizeof(at_tx_buffer), "AT%%CMNG=0,12354,0,%s\r\n", ca); //FAULT
    if (__nrf_slte__send_command(at_tx_buffer) != SUCCESS)
        return FAILURE; // Failed to send command
        
    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")
    if ( nrf_slte__power_on() != SUCCESS)
        return FAILURE; // Failed to power on
    return SUCCESS;
}

/**
 * @brief Parse the response from the server and extract the header and body
 * 
 * @param resp_input: The response from the modem
 * @param response_header: The HTTP response header
 * @param header_max_size: The maximum size of the header
 * @param response_body: The HTTP response body
 * @param body_max_size: The maximum size of the body
 */
int __nrf_slte_https_parse_resp(char* resp_input, char* response_header, int header_max_size, char* response_body, int body_max_size) 
{
    int processed_header = 0;  // Flag to indicate if processing the header or body, 1 = processing body
    int header_len = 0;
    int body_len = 0;

    // Split the response by lines
    char *line = strtok((char *)resp_input, "\n");

    while (line != NULL) {
        if (strstr(line, "#XHTTPCRSP:")) 
        {
            // Check if processing HTTP header line or body
            int state, http_crsp_len;
            if (sscanf(line, "#XHTTPCRSP:%d,%d", &http_crsp_len, &state) != 2)
                return FAILURE; // Invalid response

            if(processed_header == 0)
            {
                // Processing header lines
                processed_header = 1;
            }
            else
            {
                // Processing body
                NRF_SLM_PRINTF("HTTP Response Body Len:%d \n", http_crsp_len);
            }
        }
        else
        {
            if(processed_header == 0)
            {
                // Append the header line to the httpHeader array
                strncat(response_header, line, MIN(strlen(line), header_max_size - header_len));
                header_len += strlen(line);
                // Add endline for each header line
                strncat(response_header, "\n", MIN(strlen("\n"), header_max_size - header_len));
                header_len += strlen("\n");
                
            }
            else
            {
                // Processing body
                strncat(response_body, line, MIN(strlen(line), body_max_size - body_len));
                body_len += strlen(line);
            }
        }
        line = strtok(NULL, "\n");
    }

    int expected_content_len = __nrf_slte__get_http_content_len(response_header);
    int recv_content_len = strlen(response_body);
    NRF_SLM_PRINTF("Expected Content Length: %d \n", expected_content_len);
    NRF_SLM_PRINTF("Received Content Length: %d \n", recv_content_len);
    if(expected_content_len != recv_content_len)
    {
        NRF_SLM_PRINTF("WARNING: Content length mismatch \n");
        return FAILURE;
    }
    return SUCCESS;
}

/**
 * @brief Connect and Send HTTPS GET request to the server
 * 
 * @param url: The URL to send the GET request to
 * @return int SUCCESS if ok. Returns FAILURE if error.
 */
int __nrf_slte_httpsGET_send_req(char* url)
{
    int status;
    char resp[AT_BUFFER_SIZE] = {0};
    char at_send_buffer[255] = {0};
    char host[100] = {0};
    char path[100] = {0};
    char* response_data;

    /* Extract hostname and path from URL */
    __nrf_slte__parse_url(url, host, path);
    NRF_SLM_PRINTF("Host: %s\n", host);
    NRF_SLM_PRINTF("Path: %s\n", path);

    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCCON=1,\"%s\",443,12354\r\n", host);
    // Connect to HTTPS server using IPv4
    if (__nrf_slte__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command
	
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

    response_data = strstr(resp, "#XHTTPCCON");
    if(response_data == NULL)
        return FAILURE; // Invalid response

    if (sscanf(response_data, "#XHTTPCCON: %d", &status) != 1)
    {
        NRF_SLM_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }
    if(status != 1)
        return FAILURE; // Failed to connect to server

    // Connected to server, send GET request
    memset(at_send_buffer, 0, strlen(at_send_buffer));
    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCREQ=\"GET\",\"%s\"\r\n", path);
    if (__nrf_slte__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__nrf_slte__wait_4response("#XHTTPCREQ", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

    response_data = strstr(resp, "XHTTPCREQ");
    if(response_data == NULL)
    	return FAILURE;
    if (sscanf(response_data, "XHTTPCREQ: %d", &status) != 1)
    {
        NRF_SLM_PRINTF("Invalid response\n");
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
int __nrf_slte_httpsPOST_send_req(char* url, char* JSONdata, char* agent)
{
    int status;
    char resp[AT_BUFFER_SIZE] = {0};
    char at_send_buffer[255] = {0};
    char host[100] = {0};
    char path[100] = {0};
    char* response_data;

    /* Extract hostname and path from URL */
    __nrf_slte__parse_url(url, host, path);
    NRF_SLM_PRINTF("Host: %s\n", host);
    NRF_SLM_PRINTF("Path: %s\n", path);

    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCCON=1,\"%s\",443,12354\r\n", host);
    // Connect to HTTPS server using IPv4
    if (__nrf_slte__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__nrf_slte__wait_4response("OK", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));

    response_data = strstr(resp, "#XHTTPCCON");
    if(response_data == NULL)
        return FAILURE; // Invalid response

    if (sscanf(response_data, "#XHTTPCCON: %d", &status) != 1)
    {
        NRF_SLM_PRINTF("Invalid response\n");
        return FAILURE; // Invalid response
    }

    if(status != 1)
        return FAILURE; // Failed to connect to server

    // Connected to server, send GET request
    memset(at_send_buffer, 0, strlen(at_send_buffer));
    snprintf(at_send_buffer, sizeof(at_send_buffer), "AT#XHTTPCREQ=\"POST\",\"%s\",\"User-Agent: %s\r\n\",\"application/json\",\"%s\"\r\n", path, agent, JSONdata);
    if (__nrf_slte__send_command(at_send_buffer) != SUCCESS)
        return FAILURE; // Failed to send command

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (__nrf_slte__wait_4response("#XHTTPCREQ", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    resp_len = __nrf_slte__get_resp(resp, sizeof(resp));
    response_data = strstr(resp, "XHTTPCREQ");
    if(response_data == NULL)
        return FAILURE;
    if(sscanf(response_data, "XHTTPCREQ: %d", &status) != 1)
    {
        NRF_SLM_PRINTF("Invalid response\n");
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
int nrf_slte__httpsPOST(char* url, char* JSONdata, char* agent, char* response, uint16_t maxlength)
{
    if(__nrf_slte_httpsPOST_send_req(url, JSONdata, agent) != SUCCESS)
        return FAILURE; // Failed to send request

    char resp[AT_BUFFER_SIZE] = {0};
    char http_resp_header[AT_HTTP_HEADER_MAX_SIZE] = {0};
    char http_resp_body[AT_HTTP_BODY_MAX_SIZE] = {0};
    
    mailbox__flush(&at_rx_data);
	
	vTaskDelay(3000 / portTICK_PERIOD_MS);

    //TODO: Check if it's succifient OR need to wait for "OK" OR
    //TODO: wait for full HTTP response message (receive header, body and the #XHTTPCRSP for body should have state = 1)
    if (__nrf_slte__wait_4response("#XHTTPCRSP", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));
        
    #if (TEST_USED_SAMPLE_HTTP_RESP == 1)   // Overwrite resp with sample HTTP response
    memset(resp, 0, AT_BUFFER_SIZE);
    strncpy(resp, HTTP_RESP_EXAMPLE, AT_BUFFER_SIZE);
    #endif /* End of (TEST_USED_SAMPLE_HTTP_RESP == 1) */

    int ret_val = __nrf_slte_https_parse_resp(resp, http_resp_header, AT_HTTP_HEADER_MAX_SIZE, http_resp_body, AT_HTTP_BODY_MAX_SIZE);
    NRF_SLM_PRINTF("HTTP Response Header: \n%s\n", http_resp_header);
    NRF_SLM_PRINTF("HTTP Response Body: \n%s\n", http_resp_body);

    if (ret_val != SUCCESS)
        return FAILURE;
    else
    {
        // Copy HTTP body to response buffer
        strncpy(response, http_resp_body, maxlength);    
        return SUCCESS;
    }
}

int nrf_slte__httpsGET(char* url, char* response, uint16_t maxlength)
{
    
    if(__nrf_slte_httpsGET_send_req(url) != SUCCESS)
        return FAILURE; // Failed to send request

    char resp[AT_BUFFER_SIZE] = {0};
    char http_resp_header[AT_HTTP_HEADER_MAX_SIZE] = {0};
    char http_resp_body[AT_HTTP_BODY_MAX_SIZE] = {0};


    mailbox__flush(&at_rx_data);
	
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    //TODO: Check if it's succifient OR need to wait for "OK" OR
    //TODO: wait for full HTTP response message (receive header, body and the #XHTTPCRSP for body should have state = 1)
    if (__nrf_slte__wait_4response("#XHTTPCRSP", AT_DEFAULT_TIMEOUT_MS) != SUCCESS)
        return FAILURE; // Failed to receive resp (no "OK" received within timeout")

    int resp_len = __nrf_slte__get_resp(resp, sizeof(resp));
    
    #if (TEST_USED_SAMPLE_HTTP_RESP == 1)   // Overwrite resp with sample HTTP response
    memset(resp, 0, AT_BUFFER_SIZE);
    strncpy(resp, HTTP_RESP_EXAMPLE, AT_BUFFER_SIZE);
    #endif /* End of (TEST_USED_SAMPLE_HTTP_RESP == 1) */

    int ret_val = __nrf_slte_https_parse_resp(resp, http_resp_header, AT_HTTP_HEADER_MAX_SIZE, http_resp_body, AT_HTTP_BODY_MAX_SIZE);
    NRF_SLM_PRINTF("HTTP Response Header: \n%s\n", http_resp_header);
    NRF_SLM_PRINTF("HTTP Response Body: \n%s\n", http_resp_body);

    if (ret_val != SUCCESS)
        return FAILURE;
    else
    {
        // Copy HTTP body to response buffer
        strncpy(response, http_resp_body, maxlength);    
        return SUCCESS;
    }  
}

volatile uint8_t g_test = 0;
extern const char howmyssl_ca[];
extern const char httpbin_ca[];
void lte_modem_custom_task(void *pvParameters)
{
    NRF_SLM_PRINTF("Testing HTTP Parser started \r\n");
    NRF_SLM_PRINTF("************************************* START *************************************");
    NRF_SLM_PRINTF("\n\r");
    NRF_SLM_PRINTF("\n\r");
    
    while(1) 
    {
        int temp_var = 1;
        switch(g_test)
        {
            case 10:
            {
                // Clear certificate
                if( (temp_var = __nrf_slte__clearCert()) != SUCCESS)
                {
                    NRF_SLM_PRINTF("Failed to clear certificate \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Certificate cleared \r\n");
                }
                // set CA
                if( (temp_var = nrf_slte__setCA(howmyssl_ca)) != SUCCESS)
                {
                    NRF_SLM_PRINTF("Failed to set CA \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("CA set \r\n");
                }
                char url[] = "howsmyssl.com/a/check";
                char http_resp_buffer[2048] = {0};
                nrf_slte__httpsGET(url , http_resp_buffer, sizeof(http_resp_buffer));
                NRF_SLM_PRINTF("\r\n===================================================\r\n");
                NRF_SLM_PRINTF("HTTP Response: %s\n", http_resp_buffer);
                NRF_SLM_PRINTF("\r\n===================================================\r\n");
                break;
            }

            case 11:
            {
                // Clear certificate
                if( (temp_var = __nrf_slte__clearCert()) != SUCCESS)
                {
                    NRF_SLM_PRINTF("Failed to clear certificate \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Certificate cleared \r\n");
                }
                // set CA
                if( (temp_var = nrf_slte__setCA(httpbin_ca)) != SUCCESS)
                {
                    NRF_SLM_PRINTF("Failed to set CA \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("CA set \r\n");
                }

                char http_resp_buffer[2048] = {0};
                char agent[] = "slm";
                char url[] = "httpbin.org/post";
                char json_data[] = "{\"foo1\":\"bar1\",\"foo2\":\"bar2\"}";
                nrf_slte__httpsPOST(url, json_data, agent, http_resp_buffer, sizeof(http_resp_buffer));
                NRF_SLM_PRINTF("\r\n===================================================\r\n");
                NRF_SLM_PRINTF("HTTP Response: %s\n", http_resp_buffer);
                NRF_SLM_PRINTF("\r\n===================================================\r\n");
                break;
            }

            case 1:
            {
                if ( nrf_slte__power_on() != SUCCESS ) {
                    NRF_SLM_PRINTF("Failed to power on modem \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Modem powered on \r\n ");
                }
                break;
            }

            case 2:
            {
                if ( nrf_slte__power_off() != SUCCESS ) {
                    NRF_SLM_PRINTF("Failed to power off modem \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Modem powered off \r\n ");
                }
                break;
            }

            case 3:
            {
                if( (temp_var = nrf_slte__get_rssi()) == FAILURE)
                {
                    NRF_SLM_PRINTF("Failed to get RSSI \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("RSSI: %d \r\n ", temp_var);
                }
                break;
            }

            case 4: //TODO: Need more work
            {
                if( (temp_var = nrf_slte__connected()) < 0)
                {
                    NRF_SLM_PRINTF("Failed to get connection status \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Connection status %d \r\n", temp_var);
                }
                break;
            }

            case 5: 
            {
                if( (temp_var = nrf_slte__get_SimPresent()) != 1)
                {
                    NRF_SLM_PRINTF("Failed to get SIM status \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("SIM status %d \r\n", temp_var);
                }
                break;
            }

            case 6:
            {
                if( (temp_var = nrf_slte__get_time()) == FAILURE)
                {
                    NRF_SLM_PRINTF("Failed to get time \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Time: %d \r\n", temp_var);
                }
                break;
            }

            case 7:
            {
                // Clear certificate
                if( (temp_var = __nrf_slte__clearCert()) != SUCCESS)
                {
                    NRF_SLM_PRINTF("Failed to clear certificate \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("Certificate cleared \r\n");
                }
            }

            case 8:
            {
                char cacert_test[] = "-----BEGIN CERTIFICATE-----\n";
                if( (temp_var = nrf_slte__setCA(cacert_test)) != SUCCESS)
                {
                    NRF_SLM_PRINTF("Failed to set CA \r\n ");
                }
                else
                {
                    NRF_SLM_PRINTF("CA set \r\n");
                }
                break;
            }
        }
    }
    NRF_SLM_PRINTF("************************************* END ***************************************");
}