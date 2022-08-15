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
#include "espconn.h"
#include "mem.h"
#include "json/jsonparse.h"
#include "json/jsontree.h"
#include "cJSON.h"
#include "user_webserver.h"
#include "spi_flash.h"
#include "mqtt.h"
#include "debug.h"
#include "wizchip_conf.h"
#include "driver/spi_interface.h"
#include "driver/spi.h"
#include "socket.h"
#include "W5500/wizchip_conf.h"

#define SOCK_TCPS        1
#define DATA_BUF_SIZE			2048
uint8_t gDATABUF[DATA_BUF_SIZE];

uint8_t dest_ip[4] = {10, 1, 1, 144};
uint16_t dest_port = 	5000;

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

const char ssid[32];
const char password[32];
static struct espconn webserver_espconn;
MQTT_Client mqttClient;

SpiAttr spiConfig;//配置SPI
SpiData SpiSend;//配置SPI发送的数据

void WIFI_Init()
{
    // struct softap_config apConfig;
    // wifi_set_opmode(STATIONAP_MODE);
    // apConfig.ssid_len = 10;
    // os_strcpy(apConfig.ssid, "ESP8266Wifi");
    // os_strcpy(apConfig.password, "12345678");
    // apConfig.authmode = 3;
    // apConfig.beacon_interval = 100;
    // apConfig.channel = 1;
    // apConfig.max_connection = 4;
    // apConfig.ssid_hidden = 0;
    // wifi_softap_set_config_current(&apConfig);
    struct station_config stationConf;
    wifi_set_opmode(STATION_MODE);
    os_memcpy(&stationConf.ssid, "YinghedeiPhone", 32);
    os_memcpy(&stationConf.password, "f8vm5uxrwncpx", 32);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();
}

void mqtt_send_status(void)
{ 
	char msg[512];
	os_bzero(msg, 512);
	os_sprintf(msg, "%s","hello");
	MQTT_Publish(&mqttClient, "topic_ctos", msg, os_strlen(msg), 2, 0);
	os_printf("mqtt_send_status ok!\n");
}

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Connected\r\n");
  MQTT_Subscribe(client, "/gqkg59Oh5RE/test_point_1/user/get", 2);
  mqtt_send_status();
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
  os_printf("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}

void ICACHE_FLASH_ATTR W5500_SPI_Init(void){
    //WRITE_PERI_REG(PERIPHS_IO_MUX,0x105);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);  //GPIO12(MISO)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);  //GPIO13(MOSI)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);  //GPIO14 CLOCK
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);  //GPIO15 CS/SS

    spiConfig.mode = SpiMode_Master;
    spiConfig.speed = SpiSpeed_10MHz;
    spiConfig.subMode = SpiSubMode_0;
    spiConfig.bitOrder = SpiBitOrder_MSBFirst;
    
    SpiSend.addr=0;
    SpiSend.addrLen=0;
    SpiSend.cmd=0;
    SpiSend.cmdLen=0;
    SPIInit(SpiNum_HSPI,&spiConfig);//初始化SPI
}

void Send_data8(uint8 Txdata)
{
    system_soft_wdt_feed() ;//喂狗函数
    SpiData pDat;
    SpiSend.cmd = Txdata;	   ///<将第一个命令字节设置为传输数据
    SpiSend.cmdLen = 1;       ///< 1个字节长度
    SpiSend.addr = NULL;      ///< 空
    SpiSend.addrLen = 0; 	   ///< 空
    SpiSend.data = NULL; 	   ///< 空
    SpiSend.dataLen = 0; 	   ///<空
    SPIMasterSendData(SpiNum_HSPI, &SpiSend);  //完成一次数据传输的主机函数
}

static wiz_NetInfo NetConf = {
  {0x0c,0x29,0xab,0x7c,0x04,0x02},  // mac地址
  {192,168,1,133},                  // 本地IP地址
  {255,255,255,0},                  // 子网掩码
  {192,168,1,1},                    // 网关地址
  {0,0,0,0},                        // DNS服务器地址
  NETINFO_STATIC                    // 使用静态IP
};

void configNet(){
  wiz_NetInfo conf;
  // 配置网络地址
  //ctlnetwork(CN_SET_NETINFO, (void *)&NetConf);
  // 回读
  //sctlnetwork(CN_GET_NETINFO, (void *)&conf);
  if(memcmp(&conf,&NetConf,sizeof(wiz_NetInfo)) == 0){
    os_printf("net config success...\n");// 配置成功
  }else{
    os_printf("net config fail...\n");// 配置失败
  }
}

// reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);// 注册片选函数

/**
  * @brief  TCP客户端事件处理函数
  */
int32_t loopback_tcpc(uint8_t sn, uint8_t* buf, uint8_t* destip, uint16_t destport)
{   
   int32_t ret; // return value for SOCK_ERRORs
   uint16_t size = 0, sentsize=0;
 
   // Port number for TCP client (will be increased)
   uint16_t any_port = 	5000;
 
   // Socket Status Transitions
   // Check the W5500 Socket n status register (Sn_SR, The 'Sn_SR' controlled by Sn_CR command or Packet send/recv status)
   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED :
         if(getSn_IR(sn) & Sn_IR_CON)	// Socket n interrupt register mask; TCP CON interrupt = connection with peer is successful
         {
#ifdef _LOOPBACK_DEBUG_
			printf("%d:Connected to - %d.%d.%d.%d : %d\r\n",sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
			setSn_IR(sn, Sn_IR_CON);  // this interrupt should be write the bit cleared to '1'
         }
 
         //
         // Data Transaction Parts; Handle the [data receive and send] process
         //
		 if((size = getSn_RX_RSR(sn)) > 0) // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
         {
			if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE; // DATA_BUF_SIZE means user defined buffer size (array)
			ret = recv(sn, buf, size); // Data Receive process (H/W Rx socket buffer -> User's buffer)
 
			if(ret <= 0) return ret; // If the received data length <= 0, receive failed and process end
			size = (uint16_t) ret;
			sentsize = 0;
 
			// Data sentsize control
			while(size != sentsize)
			{
				ret = send(sn, buf+sentsize, size-sentsize); // Data send process (User's buffer -> Destination through H/W Tx socket buffer)
				
				printf("%s\r\n",buf);
				
				if(ret < 0) // Send Error occurred (sent data length < 0)
				{
					close(sn); // socket close
					return ret;
				}
				sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
			}
         }
		 //
         break;
 
      case SOCK_CLOSE_WAIT :
#ifdef _LOOPBACK_DEBUG_
         //printf("%d:CloseWait\r\n",sn);
#endif
         if((ret=disconnect(sn)) != SOCK_OK) return ret;
#ifdef _LOOPBACK_DEBUG_
         printf("%d:Socket Closed\r\n", sn);
#endif
         break;
 
      case SOCK_INIT :
#ifdef _LOOPBACK_DEBUG_
    	 printf("%d:Try to connect to the %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
    	 if( (ret = connect(sn, destip, destport)) != SOCK_OK) return ret;	//	Try to TCP connect to the TCP server (destination)
         break;
 
      case SOCK_CLOSED:
    	  close(sn);
    	  if((ret=socket(sn, Sn_MR_TCP, any_port++, SF_TCP_NODELAY)) != sn) return ret; // TCP socket open with 'any_port' port number
#ifdef _LOOPBACK_DEBUG_
    	 printf("%d:TCP client loopback start\r\n",sn);
         //printf("%d:Socket opened\r\n",sn);
#endif
         break;
      default:
			{
         break;
			}
   }
   return 1;
}

void ICACHE_FLASH_ATTR
user_init(void)
{
    // os_printf("start...\n");
    // // AP初始化
    // WIFI_Init();
    // // TCP初始化
    // user_webserver_init(80);

    // MQTT_InitConnection(&mqttClient, MQTT_HOST, 1883, 0);
    // MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, 60, 0);
    // MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    // MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    // MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    // MQTT_OnData(&mqttClient, mqttDataCb);

    W5500_SPI_Init();
    configNet();

	while(1)
	{
		//TCP客户端回环测试
		//loopback_tcpc(SOCK_TCPS, gDATABUF, dest_ip, dest_port);
 
		//delay_ms(20);
	}

}
