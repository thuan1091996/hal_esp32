/*******************************************************************************
* Title                 :   
* Filename              :   wifi_custom.c
* Author                :   ItachiVN
* Origin Date           :   2023/25/05
* Version               :   0.0.0
* Compiler              :   
* Target                :   
* Notes                 :   None
*******************************************************************************/

/*************** MODULE REVISION LOG ******************************************
*
*    Date       Software Version	Initials	Description
*  2023/08/06       0.0.0	         ItachiVN      Module Created.
*
*******************************************************************************/

/** \file wifi_custom.c
 *  \brief This module contains the
 */
/******************************************************************************
* Includes
*******************************************************************************/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "time.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_sntp.h"
#include "esp_smartconfig.h"
#include "esp_sntp.h"


/* User libs */
#include "hal.h"
#include "wifi_custom.h"
/******************************************************************************
* Module Preprocessor Constants
*******************************************************************************/
#define WIFI_HTTPS_DEFAULT_TIMEOUT_MS   (10000)
#define WIFI_RETRY_CONN_MAX             (5)
#define WIFI_SSID_MAX_LEN               (32)
#define WIFI_PASS_MAX_LEN               (64)
#define WIFI_CERT_MAX_LEN               (2048)
#define WIFI_CONFIG_LOAD_CERT_TO_RAM    (1) /* Set to 1 will load cert to "wifi_cert" when write cert*/
#define WIFI_CONFIG_LOAD_CREDENTIAL_NVS (1) /* Set to 1 to load Wi-Fi credential from NVS */


#define WIFI_CONNECTED_BIT 			BIT0
#define WIFI_FAIL_BIT      			BIT1
#define ESPTOUCH_GOT_CREDENTIAL     BIT2


#define EXAMPLE_ESP_WIFI_SSID      "DEFAULT_SSID"
#define EXAMPLE_ESP_WIFI_PASS      "MYPASS"

/******************************************************************************
* Module Preprocessor Macros
*******************************************************************************/

/******************************************************************************
* Module Typedefs
*******************************************************************************/

/******************************************************************************
* Module Variable Definitions
*******************************************************************************/
static uint8_t ssid[WIFI_SSID_MAX_LEN] = { 0 };
static uint8_t password[WIFI_PASS_MAX_LEN] = { 0 };
static char wifi_cert[WIFI_CERT_MAX_LEN] = {0};  
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/******************************************************************************
* Function Prototypes
*******************************************************************************/
void sntp_got_time_cb(struct timeval *tv)
{
    struct tm *time_now = localtime(&tv->tv_sec);
    char time_str[50]={0};
    strftime(time_str, sizeof(time_str), "%c", time_now);
    ESP_LOGW("SNTP", "Curtime: %s \r\n",time_str);
}

void sntp_time_init()
{
    /* SNTP */
    ESP_LOGI("SNTP", "Initializing SNTP");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(&sntp_got_time_cb);
    sntp_init();
}

int smartconfig_init()
{
    esp_err_t err = esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    if (err != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "Smartconfig type set failed with err %d", err);
        return -1;
    }
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    err = esp_smartconfig_start(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "Smartconfig start failed with err %d", err);
        return -1;
    }
    return 0;
}

void wifi_on_connected_cb(void)
{
    ESP_LOGW("wifi_custom", "On Wi-Fi connected callback");
}

void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	static int s_retry_num = 0;
	ESP_LOGI("wifi_custom", "Event handler invoked \r\n");
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
        ESP_LOGI("wifi_custom", "WIFI_EVENT_STA_START");
		ESP_LOGI("wifi_custom", "Wi-Fi STATION started successfully \r\n");
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
        ESP_LOGI("wifi_custom", "WIFI_EVENT_STA_DISCONNECTED");
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW("wifi_custom", "Wi-Fi disconnected, reason: %d", event->reason);        
        switch (event->reason)
        {

            case WIFI_REASON_ASSOC_LEAVE:
            {
                ESP_LOGE("wifi_custom", "STA left");
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            }

            case WIFI_REASON_AUTH_FAIL:
            {
                ESP_LOGE("wifi_custom", "Authentication failed. Wrong credentials provided.");
                ESP_LOGW("wifi_custom", "Start smartconfig to update credentials");
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                smartconfig_init();
                break;
            }
            case WIFI_REASON_NO_AP_FOUND:
            {
                ESP_LOGE("wifi_custom", "STA AP Not found");
            }
            // case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            // case WIFI_REASON_AUTH_EXPIRE:

            default:
            {
                if (s_retry_num < 3)
                {
                    esp_wifi_connect();
                    s_retry_num++;
                    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                    ESP_LOGI("wifi_custom", "retry to connect to the AP");
                }
                else if(s_retry_num < WIFI_RETRY_CONN_MAX)
                {
                    ESP_LOGW("wifi_custom", "Start smartconfig to update credentials");
                    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                    smartconfig_init();
                    s_retry_num++;
                }
                else
                {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
            }
            break;
        }
	}

	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
        ESP_LOGI("wifi_custom", "IP_EVENT_STA_GOT_IP");
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI("wifi_custom", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) 
    {
        ESP_LOGI("wifi_custom", "Scan done");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) 
    {
        ESP_LOGI("wifi_custom", "Found channel");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) 
    {
        ESP_LOGI("wifi_custom", "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI("wifi_custom", "SSID:%s", ssid);
        ESP_LOGI("wifi_custom", "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI("wifi_custom", "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        // Store SSID and password in NVS
        ESP_LOGI("wifi_custom", "Storing SSID: %s, password: %s in NVS", wifi_config.sta.ssid, wifi_config.sta.password);
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_GOT_CREDENTIAL);
        esp_wifi_connect();
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) 
    {
        ESP_LOGI("wifi_custom", "Smartconfig finished send ACK");
        esp_smartconfig_stop();
    }
}

int wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    do
    {
        /* Create and init lwIP related stuffs */
        if(ESP_OK !=esp_netif_init())
            break;

        /* Create default event loop */
        if(ESP_OK !=esp_event_loop_create_default())
            break;

        esp_netif_create_default_wifi_sta();

        /* Create default network interface instance binding to netif */
        if(ESP_OK != esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL) )
            break; 
        if(ESP_OK != esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL) )
            break;
        if(ESP_OK != esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL) )
            break;

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        if(ESP_OK !=esp_wifi_init(&cfg))
            break;

        if(ESP_OK != esp_wifi_set_mode(WIFI_MODE_STA))
            break;

        wifi_config_t wifi_config = {
            .sta = {
                .ssid = EXAMPLE_ESP_WIFI_SSID,
                .password = EXAMPLE_ESP_WIFI_PASS,
                /* Setting a password implies station will connect to all security modes including WEP/WPA.
                * However these modes are deprecated and not advisable to be used. Incase your Access point
                * doesn't support WPA2, these mode can be enabled by commenting below line */
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };

        #if WIFI_CONFIG_LOAD_CREDENTIAL_NVS
            wifi_config_t wifi_config_loaded = {0};
            if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config_loaded) == ESP_OK)
            {
                size_t ssidLen = strlen((char*)wifi_config_loaded.sta.ssid);
                if(ssidLen != 0)
                {
                    memcpy(&wifi_config, &wifi_config_loaded, sizeof(wifi_config_t));
                    memcpy(ssid, wifi_config_loaded.sta.ssid, strlen((char*)wifi_config_loaded.sta.ssid));
                    memcpy(password, wifi_config_loaded.sta.password, strlen((char*)wifi_config_loaded.sta.password));
                    ESP_LOGI("wifi_custom", "SSID loaded from NVS: %s", ssid);
                    ESP_LOGI("wifi_custom", "PASSWORD: %s", password);
                }
                else
                {
                    ESP_LOGE("wifi_custom", "No SSID stored in NVS");
                }
            }
        #endif /* End of WIFI_CONFIG_LOAD_CREDENTIAL_NVS */

            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
            ESP_LOGI("wifi_custom", "wifi_init_sta finished.");
            sntp_time_init();
            return 0;
    } while (0);
    
    return -1;
}

int wifi_custom_init(void)
{
    esp_log_level_set("wifi_custom", ESP_LOG_INFO);
    if ( wifi_custom__getCA(wifi_cert, WIFI_CERT_MAX_LEN) != 0)
    {
        ESP_LOGE("wifi_http", "Failed to get certificate");
    }

    ESP_LOGI("wifi_custom", "Initializing Wi-Fi station \r\n");

    return wifi_init_sta();
}


/******************************************************************************
* Function Definitions
*******************************************************************************/
//Implements esp_wifi functions to cleanly start up the wifi driver. Should automatically connect to a network if credentials are saved. (Provisioning handled elsewhere) Returns 0 if ok. Returns -1 if error.
int wifi_custom__power_on(void)
{
    if(esp_wifi_start() != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "esp_wifi_start() failed");
        return -1;
    }
    esp_err_t status = esp_wifi_connect();
    if(status != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "esp_wifi_connect() failed with error %d", status);
    }
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | ESPTOUCH_GOT_CREDENTIAL,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(15000));

    if ((bits & WIFI_CONNECTED_BIT) || (bits & ESPTOUCH_GOT_CREDENTIAL))
    {
        if(bits & ESPTOUCH_GOT_CREDENTIAL) 
        {
            ESP_LOGI("wifi_custom", "connected to ap SSID:%s password:%s", ssid, password);
        }
        wifi_on_connected_cb();
        return 0;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGE("wifi_custom", "Failed to connect to SSID:%s, password:%s", ssid, password);
    }
    else
    {
        ESP_LOGE("wifi_custom", "Timeout to connect");
    }
    return -1;
} 
//Implements esp_wifi functions to cleanly shutdown the wifi driver. (allows for a future call of wifi_custom_power_on() to work as epxected)
int wifi_custom__power_off(void)
{
    return esp_wifi_disconnect();
}

//Implements esp_wifi functions to determine if currently connected to a network.
int wifi_custom__connected(void)
{
	EventBits_t bit_mask = xEventGroupGetBits(s_wifi_event_group);
    if(bit_mask & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI("wifi_custom", "Wi-Fi is connected");
        return 1;
    }
    else
    {   
        ESP_LOGI("wifi_custom", "Wi-Fi is not connected");
        return 0;
    }
    
}
//Implements esp_wifi functions to get the current time.
long wifi_custom__get_time(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) 
    {
        ESP_LOGI("SNTP", "Time is not set yet. Getting time info");
        int retry=0, retry_count=10;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) 
        {
            ESP_LOGI("SNTP", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            if(retry == (retry_count-1))
            {
                ESP_LOGE("SNTP", "Failed to get time from SNTP server");
                return -1;
            }
        }
            //TODO: Set time zone
        setenv("TZ","UTC+7",1);
        tzset();
        time(&now);
        localtime_r(&now, &timeinfo);
        // update 'now' variable with current time
        time(&now);
    }

    int timestamp = time(NULL);
    ESP_LOGI("SNTP", "Unix timestamp: %d", timestamp);
    return timestamp;
}
//Implements esp_wifi functions to get the RSSI of the current wifi connection.
int wifi_custom__get_rssi(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t status = esp_wifi_sta_get_ap_info(&ap_info);
    if(status == ESP_ERR_WIFI_NOT_CONNECT)
    {
        ESP_LOGE("wifi_custom", "Wi-Fi is not connected");
        return -1;
    }
    else if(status != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "esp_wifi_sta_get_ap_info() failed with error %d", status);
        return -1;
    }
    ESP_LOGI("wifi_custom", "RSSI of SSID %s: %d", ap_info.ssid, ap_info.rssi);
    return ap_info.rssi;
}

int wifi_custom__getCA(char* cert, uint32_t cert_max_len)
{
    // Open the "certs" namespace in read-only mode
    nvs_handle handle;
    if (nvs_open("certs", NVS_READONLY, &handle) != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "Failed to open NVS");
        return -1;
    }

    // Load the certificate
    ESP_LOGI("wifi_custom", "Loading certificate");

    char* value = NULL;
    do
    {
        size_t value_size;

        // Try to get the size of the item
        if(nvs_get_str(handle, "certificate", NULL, &value_size) != ESP_OK){
            ESP_LOGE("wifi_custom", "Failed to get size of key: %s", "certificate");
            break;
        }

        if(value_size > cert_max_len)
        {
            ESP_LOGE("wifi_custom", "Certificate size is too large");
            break;
        }

        value = malloc(value_size);
        if (value == NULL)
        {
            ESP_LOGE("wifi_custom", "Failed to allocate memory for certificate");
            nvs_close(handle);
            return -1;
        }

        if(nvs_get_str(handle, "certificate", value, &value_size) != ESP_OK) {
            ESP_LOGE("wifi_custom", "Failed to load key: %s", "certificate");
            break;
        }

        if(value == NULL){
            ESP_LOGE("wifi_custom", "Certificate could not be loaded");
            break;
        }

        nvs_close(handle);

        // Print the certificate
        ESP_LOGI("wifi_custom", "Certificate: %s", value);
        memcpy(cert, value, value_size);
        free(value);
        return 0;

    }while(0);

    free(value);
    return -1;
}

int wifi_custom__setCA(char* cert)
{
     // Open the "certs" namespace
    nvs_handle handle;
    if(nvs_open("certs", NVS_READWRITE, &handle) != ESP_OK)
    {
        ESP_LOGE("wifi_custom", "Failed to open NVS");
        return -1;
    }

    do
    {
        ESP_LOGI("wifi_custom", "Writing certificate");
        if (nvs_set_str(handle, "certificate", cert) != ESP_OK)
        {
            ESP_LOGE("wifi_custom", "Failed to write key: %s", "certificate");
            break;
        }

        // Commit written value and close
        if (nvs_commit(handle) != ESP_OK)
        {
            ESP_LOGE("wifi_custom", "Failed to commit NVS");
            break;
        }

        ESP_LOGI("wifi_custom", "Certificate written successfully");

#if (WIFI_CONFIG_LOAD_CERT_TO_RAM != 0) 
        memset(wifi_cert, 0, sizeof(wifi_cert));
        strcpy(wifi_cert, cert);
#endif /*(WIFI_CONFIG_LOAD_CERT_TO_RAM != 0) */

        nvs_close(handle);
        return 0;
        
    }while(0);
    // Write the certificate
    nvs_close(handle);
    return -1;
}

/* ===================================== HTTP  =====================================*/
//TODO: DELETE
int netmanaddOTA_Data(char *ota_write_data, int len){
    // LOG input data
    ESP_LOGE("wifi_ota", "==== WIFI OTA data received: %d bytes ==== ", len);
    ESP_LOG_BUFFER_HEXDUMP("wifi_ota", ota_write_data, len, ESP_LOG_INFO);
    return 0;
}

#include <esp_http_client.h>
typedef struct
{
	uint8_t* 	p_payload;
	uint32_t	payload_len;
}http_payload_t;

static esp_err_t https_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI("wifi_http", "HTTPS_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI("wifi_http", "HTTPS_EVENT_ON_CONNECTED");
            ESP_LOGI("wifi_http", "Client handler to: 0x%X", (unsigned int)evt->client);
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI("wifi_http", "HTTPS_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI("wifi_http", "Received header data: %s: %s", evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGI("wifi_http", "HTTPS_EVENT_ON_DATA, len=%d", evt->data_len);
			ESP_LOGI("wifi_http", "HTTP response data: %s \r\n", (char*)evt->data);
        	/* Get data from event into response buffer */
            http_payload_t* recv_data = (http_payload_t*)evt->user_data;

            /* Alloc new buffer with appropriate size */
            recv_data->p_payload = realloc(recv_data->p_payload, recv_data->payload_len + evt->data_len + 1);
            assert(NULL != recv_data->p_payload);

            /* Move data to new buffer */
            memmove(&recv_data->p_payload[recv_data->payload_len], evt->data, evt->data_len);
            recv_data->payload_len += evt->data_len;

            /* Update NULL character */
            recv_data->p_payload[recv_data->payload_len] = 0;
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("wifi_http", "HTTPS_EVENT_ON_FINISH");
            break;

		case HTTP_EVENT_REDIRECT:
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI("wifi_http", "HTTPS_EVENT_DISCONNECTED");
            break;

    }
    return ESP_OK;
}

static esp_err_t ota_https_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI("wifi_ota", "HTTPS_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI("wifi_ota", "HTTPS_EVENT_ON_CONNECTED");
            ESP_LOGI("wifi_ota", "Client handler to: 0x%X", (unsigned int)evt->client);
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI("wifi_ota", "HTTPS_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI("wifi_ota", "Received header data: %s: %s", evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGI("wifi_ota", "HTTPS_EVENT_ON_DATA, len=%d", evt->data_len);
        	//TODO: Store receive data into NVS DATA PARTITION
            if ( netmanaddOTA_Data(evt->data, evt->data_len) != 0)
            {
                ESP_LOGE("wifi_ota", "Failed to store OTA data");
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("wifi_ota", "HTTPS_EVENT_ON_FINISH");
            ESP_LOGW("wifi_ota", "====== OTA Finish ====== ");
            break;

		case HTTP_EVENT_REDIRECT:
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI("wifi_ota", "HTTPS_EVENT_DISCONNECTED");
            break;

    }
    return ESP_OK;
}

int wifi_custom__httpsGET(char* url, char* response, uint16_t maxlength)
{
    param_check(url != NULL);
    param_check(response != NULL);
    param_check(maxlength > 0);

    http_payload_t recv_payload = {
			.p_payload = NULL,
			.payload_len = 0,
	};

	esp_http_client_config_t https_request_conf =
	{
		.event_handler = https_event_handle,
		.user_data = (void*)&recv_payload,
        .url = url,
	};
    ESP_LOGI("wifi_http", "URL: %s", https_request_conf.url);

    // Config certificate 
    if(strlen(wifi_cert) > 0)
    {
        // https_request_conf.cert_pem = wifi_cert;
        https_request_conf.cert_pem = wifi_cert;
        https_request_conf.cert_len = strlen(wifi_cert) + 1;
    }
    else
    {
        ESP_LOGE("wifi_http", "Certificate is not valid");
        return -1;
    }
    ESP_LOGI("wifi_http", "Certificate: %s", https_request_conf.cert_pem);

	esp_http_client_handle_t client = esp_http_client_init(&https_request_conf);
	if(NULL == client)
    {
        ESP_LOGE("wifi_http", "Failed to initialize HTTP connection");
        return -1;
    }

    int http_status = -1;
    do
    {
        recv_payload.p_payload = malloc(50); // Allocate 50 bytes for initial response
        if(NULL == recv_payload.p_payload)  /* Check allocation data */
        {
            ESP_LOGE("wifi_http", "Failed to allocate memory for response payload");
            esp_http_client_cleanup(client);
            return -1;
        }
        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK)
        {
            ESP_LOGE("wifi_http", "HTTPS GET request failed: %s", esp_err_to_name(err));
            http_status = -1;
            break;
        }
        ESP_LOGI("wifi_http", "Status = %d, content_length = %d", esp_http_client_get_status_code(client), (int)recv_payload.payload_len);
        ESP_LOGI("wifi_http", "HTTPS response data: %.*s \r\n", (int)recv_payload.payload_len, (char*)recv_payload.p_payload);
        http_status = 0;
        // Copy response to response buffer with appropriate size
        memcpy(response, recv_payload.p_payload, (recv_payload.payload_len > maxlength) ? maxlength : recv_payload.payload_len);

    }while (0);
	esp_http_client_cleanup(client);
    free(recv_payload.p_payload);
    return http_status;
}

int wifi_custom_OTA_httpsGET(char* url)
{
    param_check(url != NULL);

	esp_http_client_config_t https_request_conf =
	{
		.event_handler = ota_https_event_handle,
        .url = url,
        .timeout_ms = WIFI_HTTPS_DEFAULT_TIMEOUT_MS,
	};
    ESP_LOGI("wifi_ota", "URL: %s", https_request_conf.url);

    // Config certificate 
    if(strlen(wifi_cert) > 0)
    {
        // https_request_conf.cert_pem = wifi_cert;
        https_request_conf.cert_pem = wifi_cert;
        https_request_conf.cert_len = strlen(wifi_cert) + 1;
    }
    else
    {
        ESP_LOGE("wifi_ota", "Certificate is not valid");
        return -1;
    }
    ESP_LOGI("wifi_ota", "Certificate: %s", https_request_conf.cert_pem);

	esp_http_client_handle_t client = esp_http_client_init(&https_request_conf);
	if(NULL == client)
    {
        ESP_LOGE("wifi_ota", "Failed to initialize HTTP connection");
        return -1;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE("wifi_ota", "Perform HTTPS failed: %s", esp_err_to_name(err));
    }
	esp_http_client_cleanup(client);
    return SUCCESS;
}

int wifi_custom__httpsPOST(char* url, char* JSONdata, char* agent, char* response, uint16_t maxlength)
{
    param_check(url != NULL);
    param_check(JSONdata != NULL);
    param_check(response != NULL);
    param_check(maxlength > 0);

    http_payload_t recv_payload = {
			.p_payload = NULL,
			.payload_len = 0,
	};

	esp_http_client_config_t https_request_conf =
	{
		.event_handler = https_event_handle,
		.user_data = (void*)&recv_payload,
        .timeout_ms = WIFI_HTTPS_DEFAULT_TIMEOUT_MS,
        .url = url,
        .user_agent = agent,
	};

    // Config certificate 
    if(strlen(wifi_cert) > 0)
    {   
        // https_request_conf.cert_pem = wifi_cert;
        https_request_conf.cert_pem = wifi_cert;
        https_request_conf.cert_len = strlen(wifi_cert) + 1;
    }
    else
    {
        ESP_LOGE("wifi_http", "Certificate is not valid");
        return -1;
    }
    //Debug log for certificate and url
    ESP_LOGI("wifi_http", "Certificate: %s", https_request_conf.cert_pem);
    ESP_LOGI("wifi_http", "URL: %s", https_request_conf.url);

	esp_http_client_handle_t client = esp_http_client_init(&https_request_conf);
	if(NULL == client)
    {
        ESP_LOGE("wifi_http", "Failed to initialize HTTP connection");
        return -1;
    }

    int http_status = -1;
    do
    {

        recv_payload.p_payload = malloc(50); // Allocate 50 bytes for initial response
        if(NULL == recv_payload.p_payload) /* Check allocation data */
        {
            ESP_LOGE("wifi_http", "Failed to allocate memory for response payload");
            esp_http_client_cleanup(client);
            return -1;
        }

        if (esp_http_client_set_method(client, HTTP_METHOD_POST) != ESP_OK)
        {
            ESP_LOGE("wifi_http", "Failed to set HTTP method");
            http_status = -1;
            break;
        }
        
        if (esp_http_client_set_header(client, "Content-Type", "application/json") != ESP_OK)
        {
            ESP_LOGE("wifi_http", "Failed to set HTTP header");
            http_status = -1;
            break;
        }

        if( esp_http_client_set_post_field(client, JSONdata, strlen(JSONdata)) != ESP_OK)
        {
            ESP_LOGE("wifi_http", "Failed to set post field");
            http_status = -1;
            break;
        }

        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK)
        {
            ESP_LOGE("wifi_http", "HTTPS GET request failed: %s", esp_err_to_name(err));
            http_status = -1;
            break;
        }
        ESP_LOGI("wifi_http", "Status = %d, content_length = %d", esp_http_client_get_status_code(client), (int)recv_payload.payload_len);
        ESP_LOGI("wifi_http", "HTTPS response data: %.*s \r\n", (int)recv_payload.payload_len, (char*)recv_payload.p_payload);
        http_status = 0;
        // Copy response to response buffer with appropriate size
        memcpy(response, recv_payload.p_payload, (recv_payload.payload_len > maxlength) ? maxlength : recv_payload.payload_len);

    }while (0);
	esp_http_client_cleanup(client);
    free(recv_payload.p_payload);
    return http_status;
}

const char howmyssl_ca[] = 
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";



const char httpbin_ca[] = 
"-----BEGIN CERTIFICATE-----\n" \
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
"rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
"-----END CERTIFICATE-----\n";


const char google_drive_cert[] =
"-----BEGIN CERTIFICATE-----\n" \
"MIIFljCCA36gAwIBAgINAgO8U1lrNMcY9QFQZjANBgkqhkiG9w0BAQsFADBHMQsw\n" \
"CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n" \
"MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMjAwODEzMDAwMDQyWhcNMjcwOTMwMDAw\n" \
"MDQyWjBGMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n" \
"Y2VzIExMQzETMBEGA1UEAxMKR1RTIENBIDFDMzCCASIwDQYJKoZIhvcNAQEBBQAD\n" \
"ggEPADCCAQoCggEBAPWI3+dijB43+DdCkH9sh9D7ZYIl/ejLa6T/belaI+KZ9hzp\n" \
"kgOZE3wJCor6QtZeViSqejOEH9Hpabu5dOxXTGZok3c3VVP+ORBNtzS7XyV3NzsX\n" \
"lOo85Z3VvMO0Q+sup0fvsEQRY9i0QYXdQTBIkxu/t/bgRQIh4JZCF8/ZK2VWNAcm\n" \
"BA2o/X3KLu/qSHw3TT8An4Pf73WELnlXXPxXbhqW//yMmqaZviXZf5YsBvcRKgKA\n" \
"gOtjGDxQSYflispfGStZloEAoPtR28p3CwvJlk/vcEnHXG0g/Zm0tOLKLnf9LdwL\n" \
"tmsTDIwZKxeWmLnwi/agJ7u2441Rj72ux5uxiZ0CAwEAAaOCAYAwggF8MA4GA1Ud\n" \
"DwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwEgYDVR0T\n" \
"AQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQUinR/r4XN7pXNPZzQ4kYU83E1HScwHwYD\n" \
"VR0jBBgwFoAU5K8rJnEaK0gnhS9SZizv8IkTcT4waAYIKwYBBQUHAQEEXDBaMCYG\n" \
"CCsGAQUFBzABhhpodHRwOi8vb2NzcC5wa2kuZ29vZy9ndHNyMTAwBggrBgEFBQcw\n" \
"AoYkaHR0cDovL3BraS5nb29nL3JlcG8vY2VydHMvZ3RzcjEuZGVyMDQGA1UdHwQt\n" \
"MCswKaAnoCWGI2h0dHA6Ly9jcmwucGtpLmdvb2cvZ3RzcjEvZ3RzcjEuY3JsMFcG\n" \
"A1UdIARQME4wOAYKKwYBBAHWeQIFAzAqMCgGCCsGAQUFBwIBFhxodHRwczovL3Br\n" \
"aS5nb29nL3JlcG9zaXRvcnkvMAgGBmeBDAECATAIBgZngQwBAgIwDQYJKoZIhvcN\n" \
"AQELBQADggIBAIl9rCBcDDy+mqhXlRu0rvqrpXJxtDaV/d9AEQNMwkYUuxQkq/BQ\n" \
"cSLbrcRuf8/xam/IgxvYzolfh2yHuKkMo5uhYpSTld9brmYZCwKWnvy15xBpPnrL\n" \
"RklfRuFBsdeYTWU0AIAaP0+fbH9JAIFTQaSSIYKCGvGjRFsqUBITTcFTNvNCCK9U\n" \
"+o53UxtkOCcXCb1YyRt8OS1b887U7ZfbFAO/CVMkH8IMBHmYJvJh8VNS/UKMG2Yr\n" \
"PxWhu//2m+OBmgEGcYk1KCTd4b3rGS3hSMs9WYNRtHTGnXzGsYZbr8w0xNPM1IER\n" \
"lQCh9BIiAfq0g3GvjLeMcySsN1PCAJA/Ef5c7TaUEDu9Ka7ixzpiO2xj2YC/WXGs\n" \
"Yye5TBeg2vZzFb8q3o/zpWwygTMD0IZRcZk0upONXbVRWPeyk+gB9lm+cZv9TSjO\n" \
"z23HFtz30dZGm6fKa+l3D/2gthsjgx0QGtkJAITgRNOidSOzNIb2ILCkXhAd4FJG\n" \
"AJ2xDx8hcFH1mt0G/FX0Kw4zd8NLQsLxdxP8c4CU6x+7Nz/OAipmsHMdMqUybDKw\n" \
"juDEI/9bfU1lcKwrmz3O2+BtjjKAvpafkmO8l7tdufThcV4q5O8DIrGKZTqPwJNl\n" \
"1IXNDw9bg1kWRxYtnCQ6yICmJhSFm/Y3m6xv+cXDBlHz4n/FsRC6UfTd\n" \
"-----END CERTIFICATE-----\n";

int wifi_custom_test_https_get()
{
    char url[] = "https://drive.google.com/uc?id=1Q5vSlZQfWKRGBR-wJglhjyeqiBs9X_Ss&export=download";
    char resp_buff[2048] = {0};
    // Print certificate
    wifi_custom__setCA(google_drive_cert);
    memset(wifi_cert, 0, sizeof(wifi_cert));

    if ( wifi_custom__getCA(wifi_cert, WIFI_CERT_MAX_LEN) != 0)
    {
        ESP_LOGE("wifi_http", "Failed to get certificate");
        wifi_custom__setCA(google_drive_cert);
        memset(wifi_cert, 0, sizeof(wifi_cert));
        if ( wifi_custom__getCA(wifi_cert, WIFI_CERT_MAX_LEN) != 0)
        {
            ESP_LOGE("wifi_http", "Failed to get certificate");
            return -1;
        }
    }
    ESP_LOGI("wifi_http", "Certificate length: %d", strlen(wifi_cert));
    if (wifi_custom_OTA_httpsGET(url) != 0)
    {
        ESP_LOGE("wifi_http", "Failed to get response");
        return -1;
    }
    ESP_LOGI("wifi_http", "Response: %s", resp_buff);
    return 0;
}
int wifi_custom_test_https_post()
{
    char url[] = "https://httpbin.org/post";
    char resp_buff[2048] = {0};
    char JSONdata[] = "{\"foo1\":\"bar1\",\"foo2\":\"bar2\"}";
    char agent[] = "esp32";
    // Print certificated
    wifi_custom__setCA(httpbin_ca);
    memset(wifi_cert, 0, sizeof(wifi_cert));
    if ( wifi_custom__getCA(wifi_cert, WIFI_CERT_MAX_LEN) != 0)
    {
        ESP_LOGE("wifi_http", "Failed to get certificate");
        wifi_custom__setCA(httpbin_ca);
        memset(wifi_cert, 0, sizeof(wifi_cert));
        if ( wifi_custom__getCA(wifi_cert, WIFI_CERT_MAX_LEN) != 0)
        {
            ESP_LOGE("wifi_http", "Failed to get certificate");
            return -1;
        }
    }
    ESP_LOGI("wifi_http", "Certificate length: %d", strlen(wifi_cert));
    if (wifi_custom__httpsPOST(url, JSONdata, agent, resp_buff, 2048) != 0)
    {
        ESP_LOGE("wifi_http", "Failed to get response");
        return -1;
    }
    ESP_LOGI("wifi_http", "Response: %s", resp_buff);
    return 0;
}

void wifi_custom_http__task(void *pvParameters)
{
    while(1)
    {

        // Re-connect to Wi-Fi when disconnect
        do
        {
            wifi_custom__power_on();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }while(false == wifi_custom__connected());

        while(wifi_custom__connected())
        {
            // if (wifi_custom_test_https_post() != 0)
            // {
            //     ESP_LOGE("wifi_http", "Failed to get response");
            // }
            // vTaskDelay(5000 / portTICK_PERIOD_MS);

            if (wifi_custom_test_https_get() != 0)
            {
                ESP_LOGE("wifi_http", "Failed to get response");
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS);


        }


    }
}