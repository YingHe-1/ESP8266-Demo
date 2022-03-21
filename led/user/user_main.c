/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#include "gpio.h"
#include "eagle_soc.h"
#include "mqtt.h"

#if ((SPI_FLASH_SIZE_MAP == 0) || (SPI_FLASH_SIZE_MAP == 1))
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 2)
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 3)
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 4)
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 5)
#define SYSTEM_PARTITION_OTA_SIZE 0xE0000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0x1fd000
#define SYSTEM_PARTITION_AT_PARAMETER_ADDR 0xfd000
#define SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY_ADDR 0xfc000
#define SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR 0xfb000
#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY_ADDR 0xfa000
#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CA_ADDR 0xf9000
#elif (SPI_FLASH_SIZE_MAP == 6)
#define SYSTEM_PARTITION_OTA_SIZE 0xE0000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0x3fd000
#define SYSTEM_PARTITION_AT_PARAMETER_ADDR 0xfd000
#define SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY_ADDR 0xfc000
#define SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR 0xfb000
#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY_ADDR 0xfa000
#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CA_ADDR 0xf9000
#else
#error "The flash map is not supported"
#endif

#ifdef CONFIG_ENABLE_IRAM_MEMORY
uint32 user_iram_memory_is_enabled(void)
{
    return CONFIG_ENABLE_IRAM_MEMORY;
}
#endif

static const partition_item_t at_partition_table[] = {
    {SYSTEM_PARTITION_BOOTLOADER, 0x0, 0x1000},
    {SYSTEM_PARTITION_OTA_1, 0x1000, SYSTEM_PARTITION_OTA_SIZE},
    {SYSTEM_PARTITION_OTA_2, SYSTEM_PARTITION_OTA_2_ADDR, SYSTEM_PARTITION_OTA_SIZE},
    {SYSTEM_PARTITION_RF_CAL, SYSTEM_PARTITION_RF_CAL_ADDR, 0x1000},
    {SYSTEM_PARTITION_PHY_DATA, SYSTEM_PARTITION_PHY_DATA_ADDR, 0x1000},
    {SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR, 0x3000},
    {SYSTEM_PARTITION_AT_PARAMETER, SYSTEM_PARTITION_AT_PARAMETER_ADDR, 0x3000},
    {SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY, SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY_ADDR, 0x1000},
    {SYSTEM_PARTITION_SSL_CLIENT_CA, SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR, 0x1000},
#ifdef CONFIG_AT_WPA2_ENTERPRISE_COMMAND_ENABLE
    {SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY, SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY_ADDR, 0x1000},
    {SYSTEM_PARTITION_WPA2_ENTERPRISE_CA, SYSTEM_PARTITION_WPA2_ENTERPRISE_CA_ADDR, 0x1000},
#endif
};

void ICACHE_FLASH_ATTR user_pre_init(void)
{
    if (!system_partition_table_regist(at_partition_table, sizeof(at_partition_table) / sizeof(at_partition_table[0]), SPI_FLASH_SIZE_MAP))
    {
        os_printf("system_partition_table_regist fail\r\n");
    }
}

#define SCK_GPIO_PIN 12 //移位寄存器时钟线引脚
#define RCK_GPIO_PIN 13 //存储寄存器时钟线引脚
#define SDA_GPIO_PIN 14 //数据引脚

#define _74HC595_LEVEL 4 // 74HC595级联数

#define HC595_SCK_Low() GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 0)  // SCK置低
#define HC595_SCK_High() GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1) // SCK置高

#define HC595_RCK_Low() GPIO_OUTPUT_SET(GPIO_ID_PIN(13), 0)  // RCK置低
#define HC595_RCK_High() GPIO_OUTPUT_SET(GPIO_ID_PIN(13), 1) // RCK置高

#define HC595_Data_Low() GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 0)  //输入低电平
#define HC595_Data_High() GPIO_OUTPUT_SET(GPIO_ID_PIN(14), 1) //输入高电平

static uint8_t off_light[4] = {0x00, 0x00, 0x00, 0x00};
static uint8_t on_light[4] = {0xff, 0xff, 0xff, 0xff};
static uint8_t blue_light[4] = {0x12, 0x49, 0x12, 0x49};
static uint8_t green_light[4] = {0x24, 0x92, 0x24, 0x92};
static uint8_t red_light[4] = {0x49, 0x24, 0x49, 0x24};

MQTT_Client mqttClient;

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Connected\r\n");
  MQTT_Subscribe(client, "/mqtt/topic/0", 0);
  MQTT_Subscribe(client, "/mqtt/topic/1", 1);
  MQTT_Subscribe(client, "/mqtt/topic/2", 2);

  MQTT_Publish(client, "/mqtt/topic/0", "hello0", 6, 0, 0);
  MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
  MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);

}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Disconnected\r\n");
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Published\r\n");
}

static void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
  char *topicBuf = (char*)os_zalloc(topic_len + 1),
        *dataBuf = (char*)os_zalloc(data_len + 1);

  MQTT_Client* client = (MQTT_Client*)args;
  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;
  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;
  INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}

void _74hc595_init()
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTMS_U); 
    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDI_U);
    gpio_output_set(0, 0, GPIO_ID_PIN(12) | GPIO_ID_PIN(13) | GPIO_ID_PIN(14), 0);
    os_printf("hc595Iinit\r\n");
}

void HC595_Save(void)
{
    HC595_RCK_Low();
    os_delay_us(100);
    HC595_RCK_High();
}

void HC595_Send_Byte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++) 
    {
        HC595_SCK_Low();
        if (byte & 0x80)
        {                      
            HC595_Data_High(); 
        }
        else
        { 
            HC595_Data_Low();
        }
        os_delay_us(100);
        byte <<= 1;       
        HC595_SCK_High(); 
    }
}

void HC595_Send_Multi_Byte(uint8_t *data, uint16_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        HC595_Send_Byte(data[i]);
    }
    HC595_Save();
}

void ICACHE_FLASH_ATTR delay_ms(u32 C_time)
{	for(;C_time>0;C_time--)
	{ os_delay_us(1000);}
}

void RGB(void){
    HC595_Send_Multi_Byte(blue_light,_74HC595_LEVEL);
    delay_ms(1000);
    HC595_Send_Multi_Byte(red_light,_74HC595_LEVEL);
    delay_ms(1000);	
    HC595_Send_Multi_Byte(green_light,_74HC595_LEVEL);
    delay_ms(1000);
}

void ICACHE_FLASH_ATTR
user_init(void)
{
    _74hc595_init();
    os_printf("startWiFi...\n");
    struct station_config stationConf;
    wifi_set_opmode(STATION_MODE);
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 32);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();
    os_printf("conn...\n");
  
    
    MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, 0);
    MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, 60, 0);
    MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    MQTT_OnData(&mqttClient, mqttDataCb);
    os_printf("START LED\n");
    while (1)
    {
        RGB();
    }
}
