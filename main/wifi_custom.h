//Wifi Driver (Custom)
//Integrates with ESP-IDF HAL (wifi.h)
//runs on the ESP32 - WIFI hardware is on-chip.

//Commands Implemented:

//Public Functions - Meant for direct use - all block for response to return data.
int wifi_custom_init();
int wifi_custom__power_on(void); //Implements esp_wifi functions to cleanly start up the wifi driver. Should automatically connect to a network if credentials are saved. (Provisioning handled elsewhere) Returns 0 if ok. Returns -1 if error.
int wifi_custom__power_off(void); //Implements esp_wifi functions to cleanly shutdown the wifi driver. (allows for a future call of wifi_custom_power_on() to work as epxected)
int wifi_custom__connected(void); //Implements esp_wifi functions to determine if currently connected to a network.
long wifi_custom__get_time(void); //Implements esp_wifi functions to get the current time.
int wifi_custom__get_rssi(void); //Implements esp_wifi functions to get the RSSI of the current wifi connection.
int wifi_custom__setCA(char* ca); //Implements esp_wifi functions to set the HTTPS CA cert. Returns 0 if ok. Returns -1 if error.
int wifi_custom__printCA();
int wifi_custom__httpsGET(char* url, char* response, uint16_t maxlength); //if url = "google.com/myurl"Implements esp_wifi functions to send a GET request via HTTPS. Returns 0 if ok. Returns -1 if error. Returns response in response char array.
int wifi_custom__httpsPOST(char* url, char* JSONdata, char* agent, char* response, uint16_t maxlength); //if url = "google.com/myurl" Implements esp_wifi functions to send a POST request via HTTPS. Returns 0 if ok. Returns -1 if error. Returns response in response char array.

int wifi_custom__getData(char* data, uint16_t maxlength, bool block); //returns number of characters read if ok. if "block" is true, wait for the next HTTP Response. Handles HTTPS Responses. 

//Private Functions - Meant for internal use


