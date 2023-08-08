//Serial LTE driver for nrf9160
//Integrates with hal driver (hal.h)
//nrf9160 is connected via UART. UART is configured in hal.h

//Implements API calls defined here: 
//General Commands: https://infocenter.nordicsemi.com/index.jsp?topic=%2Fref_at_commands%2FREF%2Fat_commands%2Fintro.html
//NRF9160 Serial LTE Commands: https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/AT_commands_intro.html#slm-at-intro


//All commands need to have "\r\n" appended to the end. This is the carriage return and line feed characters.

//uses snprintf for char* formatting and interpretation
//Uses STD C library "string.h" for string manipulation - https://www.tutorialspoint.com/c_standard_library/string_h.htm

//Commands Implemented:

//Public Functions - Meant for direct use - all block for response to return data.
int nrf_slte__power_on(void); //Implements [AT+CFUN=21] (Enables LTE modem.), Waits for "OK" response. Returns 0 if ok. Returns -1 if error.
int nrf_slte__power_off(void); //Implements [AT+CFUN=0] (Disables LTE modem.), Waits for "OK" response. Returns 0 if ok. Returns -1 if error.
int nrf_slte__connected(void); //Implements [AT+COPS?] returns 1 if connected, 0 if not connected, -1 if error //https://infocenter.nordicsemi.com/topic/ref_at_commands/REF/at_commands/nw_service/cops_read.html
long nrf_slte__get_time(void); //Implements [AT#CARRIER="time","read"] to get time, returns seconds since UTC time 0. //https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/CARRIER_AT_commands.html#lwm2m-carrier-library-xcarrier
int nrf_slte__get_rssi(void); //Implements [AT+CESQ] (Signal Quality Report), Waits for Response, Returns RSSI value in dBm. https://infocenter.nordicsemi.com/topic/ref_at_commands/REF/at_commands/mob_termination_ctrl_status/cesq_set.html
int nrf_slte__get_SimPresent(void); //Implements [TBD] command, returns 1 if SIM is present, 0 if not present, -1 if error
int nrf_slte__setCA(char* ca); //calls "__nrf_slte__clearCert()" to clear the certificate, if it exists. Then, Implements [AT%CMNG=0,12354,0,\"<ca>\"] to set CA certificate, where <ca> represents the contents of ca, with the null terminator removed. Then, enables the modem using "nrf_slte__power_on()" Returns 0 if ok. Returns -1 if error. https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/Generic_AT_commands.html#native-tls-cmng-xcmng

int nrf_slte__httpsGET(char* url, char* response, uint16_t maxlength); //if url = "google.com/myurl" Implements [AT#XHTTPCCON=1,\"google.com\",443,12354], waits for a valid reply, then implements [AT#XHTTPCREQ=\"GET\",\"/myurl\"]. Returns 0 if ok. Returns -1 if error. Returns response in response char array. https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/HTTPC_AT_commands.html
int nrf_slte__httpsPOST(char* url, char* JSONdata, char* agent, char* response, uint16_t maxlength); //if url = "google.com/myurl" Implements [AT#XHTTPCCON=1,\"google.com\",443,12354], waits for a valid reply, then implements [AT#XHTTPCREQ=\"POST\",\"/myurl\",\"User-Agent: <agent>\r\n\",\"application/json\","<JSONdata>\"] where <agent> is the contents of agent, with the null terminator removed, and <JSONdata> is the contents of JSONdata, with the null terminator removed. Returns 0 if ok. Returns -1 if error. Returns response in response char array. https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/HTTPC_AT_commands.html

int nrf_slte__getData(char* data, uint16_t maxlength, bool block); //returns number of characters read if ok. if "block" is true, wait for the next HTTP Response. Handles unsolicited [AT#XHTTPCRSP=... notifications] - https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/serial_lte_modem/doc/HTTPC_AT_commands.html#http-response-xhttpcrsp

//Private Functions - Meant for internal use
int __nrf_slte__send_command(char* command); //Sends a command. This is a null terminated char array. For example: "AT+CFUN=1\0"  //This function is blocking, waits for a response.
int __nrf_slte__get_resp(char* resp, uint16_t maxlength); //Gets a response. returns actual length of the response.

int __nrf_slte__clearCert(); //Implements [AT+CFUN=4] (turns off modem), Waits for OK. Implements [AT%CMNG=3,12354,0] Clears certificate #12354 if it exists. Returns 0 if ok. Returns -1 if error.  