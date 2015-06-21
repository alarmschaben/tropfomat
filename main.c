//////////////////////////////////////////////////
// Tropfomat control program
// Copyright 2015 Armin F. Gnosa (@alarmschaben)
// Based upon rboot smaple project
// Copyright 2015 Richard A Burton
// richardaburton@gmail.com
//////////////////////////////////////////////////

#define VERSION 21
#include <c_types.h>
#include <osapi.h>
#include <user_interface.h>
#include <time.h>
#include <mem.h>

#include "osapi.h"
#include "main.h"
#include "wifi.h"
//#include "user_config.h"
#include "rboot-ota.h"
#include "uart.h"
#include "gpio.h"
#include "dht.h"

#define DHT_TX_INTERVAL 10000
static os_timer_t dht_tx_timer;

#include "esp_mqtt/mqtt/include/mqtt.h"
BOOL dht_error;

static os_timer_t network_timer;
MQTT_Client mqttClient;

void ICACHE_FLASH_ATTR MQTTinit();

void ICACHE_FLASH_ATTR ProcessCommand(char* str) {}

void ICACHE_FLASH_ATTR network_wait_for_ip() {

    struct ip_info ipconfig;
    os_timer_disarm(&network_timer);
    wifi_get_ip_info(STATION_IF, &ipconfig);
    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
        char page_buffer[40];
        os_sprintf(page_buffer,"ip: %d.%d.%d.%d\r\n",IP2STR(&ipconfig.ip));
        uart0_send(page_buffer);
        MQTTinit();
    } else {
        char page_buffer[40];
        os_sprintf(page_buffer,"network retry, status: %d\r\n",wifi_station_get_connect_status());
        if(wifi_station_get_connect_status() == 3) wifi_station_connect();
        uart0_send(page_buffer);
        os_timer_setfn(&network_timer, (os_timer_func_t *)network_wait_for_ip, NULL);
        os_timer_arm(&network_timer, 2000, 0);
    }
}

void ICACHE_FLASH_ATTR wifi_config_station() {

    struct station_config stationConf;

    wifi_set_opmode(0x1);
    stationConf.bssid_set = 0;
    os_strcpy(&stationConf.ssid, SSID, os_strlen(SSID));
    os_strcpy(&stationConf.password, PASS, os_strlen(PASS));
    wifi_station_set_config(&stationConf);
    uart0_send("wifi connecting...\r\n");
    wifi_station_connect();
    os_timer_disarm(&network_timer);
    os_timer_setfn(&network_timer, (os_timer_func_t *)network_wait_for_ip, NULL);
    os_timer_arm(&network_timer, 5000, 0);
}

void ICACHE_FLASH_ATTR Switch() {
    char msg[50];
    uint8 before, after;
    before = rboot_get_current_rom();
    if (before == 0) after = 1; else after = 0;
    os_sprintf(msg, "Swapping from rom %d to rom %d.\r\n", before, after);
    uart0_send(msg);
    rboot_set_current_rom(after);
    // uart0_send("Restarting...\r\n\r\n");
    // system_restart();
}

static void ICACHE_FLASH_ATTR OtaUpdate_CallBack(void *arg, bool result) {

    char msg[40];
    rboot_ota *ota = (rboot_ota*)arg;

    if(result == true) {
        // success, reboot
        os_sprintf(msg, "Firmware updated, rebooting to rom %d...\r\n", ota->rom_slot);
        uart0_send(msg);
        rboot_set_current_rom(ota->rom_slot);
        system_restart();
    } else {
        // fail, cleanup
        uart0_send("Firmware update failed!\r\n");
        os_free(ota->request);
        os_free(ota);
    }
}

#define HTTP_HEADER "Connection: keep-alive\r\n\
    Cache-Control: no-cache\r\n\
User-Agent: rBoot-Sample/1.0\r\n\
Accept: */*\r\n\r\n"
/**/

static void ICACHE_FLASH_ATTR OtaUpdate() {

    uint8 slot;
    rboot_ota *ota;

    // create the update structure
    ota = (rboot_ota*)os_zalloc(sizeof(rboot_ota));
    os_memcpy(ota->ip, ota_ip, 4);
    ota->port = 80;
    ota->callback = (ota_callback)OtaUpdate_CallBack;
    ota->request = (uint8 *)os_zalloc(512);

    // select rom slot to flash
    slot = rboot_get_current_rom();
    if (slot == 0) slot = 1; else slot = 0;
    ota->rom_slot = slot;

    // actual http request
    os_sprintf((char*)ota->request,
            "GET /%s HTTP/1.1\r\nHost: "IPSTR"\r\n" HTTP_HEADER,
            (slot == 0 ? "rom0.bin" : "rom1.bin"),
            IP2STR(ota->ip));

    // start the upgrade process
    if (rboot_ota_start(ota)) {
        uart0_send("Updating...\r\n");
    } else {
        uart0_send("Updating failed!\r\n\r\n");
        os_free(ota->request);
        os_free(ota);
    }

}

/*
   void ICACHE_FLASH_ATTR mqtt_print(char* msg) {
   char* end = strchr(msg, 0);
   int len;
   len = (int)(end - msg);
   MQTT_Publish(&mqttClient, msg, len, 0, 0);
   }
   */

void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char msg[50];
    char *topicBuf = (char*)os_zalloc(topic_len+1),
         *dataBuf = (char*)os_zalloc(data_len+1);

    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;

    os_memcpy(dataBuf, data, data_len);
    dataBuf[data_len] = 0;

    os_sprintf(msg, "received %s on topic %s\n", data, topic);
    uart0_send(msg);

    if (!strcmp(dataBuf,"reset")) {
        os_sprintf(msg, "Restarting...");
        MQTT_Publish(&mqttClient, MQTT_REPLTOPIC, msg, 13, 0, 0);
        os_delay_us(3000);
        system_restart();
    } else if (!strcmp(dataBuf, "ota")) {
        os_sprintf(msg, "Performing OTA update...");
        MQTT_Publish(&mqttClient, MQTT_REPLTOPIC, msg, 24, 0, 0);
        OtaUpdate();
    } else if (!strcmp(dataBuf, "enable")) {
        os_sprintf(msg, "on");
        MQTT_Publish(&mqttClient, MQTT_REPLTOPIC, msg, 2, 0, 0);
        GPIO_OUTPUT_SET(5, 1);
    } else if (!strcmp(dataBuf, "disable")) {
        os_sprintf(msg, "off");
        MQTT_Publish(&mqttClient, MQTT_REPLTOPIC, msg, 3, 0, 0);
        GPIO_OUTPUT_SET(5, 0);
    }
    os_free(topicBuf);
    os_free(dataBuf);
}

void ICACHE_FLASH_ATTR MQTTinit() {
    char msg[50];
    MQTT_InitConnection(&mqttClient, MQTT_BROKER, 1883, 0);
    MQTT_InitClient(&mqttClient, "tropfomat%08X", "", "", 120, 1);

    MQTT_InitLWT(&mqttClient, MQTT_STATETOPIC, "offline", 0, 1);

    MQTT_Subscribe(&mqttClient, MQTT_CMDTOPIC, 0);

    MQTT_Publish(&mqttClient, MQTT_STATETOPIC,"online", 6, 0, 1);
    os_sprintf(msg, "Tropfomat v. %03d on ROM %1d", VERSION, rboot_get_current_rom());

    MQTT_Publish(&mqttClient, MQTT_VERTOPIC, msg, 25, 0, 1);

    MQTT_OnData(&mqttClient, mqttDataCb);

    MQTT_Connect(&mqttClient);
}

void DHTtxTimerCb(void *arg) {
    struct sensor_reading* reading = readDHT(0);
    //INFO("Timer fired. ");

    if (reading->data_ready == 1) {
        if (reading->error == 1) {
            // MQTT_Publish(&mqttClient, STATETOPIC, "online", 6, 0, 1);
            reading->error = 0;
            dht_error = 0;
            //INFO("Returning from error state.\n");
        }

        MQTT_Publish(&mqttClient, MQTT_TEMPTOPIC, reading->temperature_string, sizeof reading->temperature_string - 1, 0, 1);
        MQTT_Publish(&mqttClient, MQTT_HUMTOPIC, reading->humidity_string, sizeof reading->humidity_string - 1, 0, 1);
        reading->data_ready = 0;

    } else {
        if ((reading->error == 1) && (dht_error == 0)) {
            //MQTT_Publish(&mqttClient, STATETOPIC, "error", 5, 0, 1);
            dht_error = 1;
            //INFO("Transitioning into error state.\n");
        }
        //INFO("No data.\n");
    }

    os_timer_arm(&dht_tx_timer, DHT_TX_INTERVAL, 0);
}
void ICACHE_FLASH_ATTR user_init(void) {
    // general stuff
    gpio_init();

    // universal message buffer
    char msg[50];

    // UART init
    uart_init(BIT_RATE_115200,BIT_RATE_115200);
    uart0_send("\r\n\r\nBase Project\r\n");
    os_sprintf(msg, "\r\nCurrently running rom %d.\r\n", rboot_get_current_rom());
    uart0_send(msg);

    // relay init
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);
    GPIO_OUTPUT_SET(5, 0);

    // wifi init
    wifi_config_station();

    // dht init
    dht_error = 0; // we hope
    DHTInit(SENSOR_DHT11, 30000);

    // setup timer for DHT reading transmission
    os_timer_disarm(&dht_tx_timer);
    os_timer_setfn(&dht_tx_timer, (os_timer_func_t *) DHTtxTimerCb, NULL);
    os_timer_arm(&dht_tx_timer, DHT_TX_INTERVAL, 0);
}
