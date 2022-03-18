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

extern u8  Usart1ReadBuff[Usart1ReadLen];//接收数据的数组
extern u32 Usart1ReadCnt;//串口1接收到的数据个数
extern u32 Usart1ReadCntCopy;//串口1接收到的数据个数拷贝
extern u8  Usart1ReadFlage;//串口1接收到一条完整数据
const char ssid[32] = "YinghedeiPhone";
const char password[32] = "f8vm5uxrwncpx";
const uint16_t port = 8080;
const char *host = "172.16.21.69";
// TCP客户端
struct espconn TcpClient;
esp_tcp esptcp;

// void ICACHE_FLASH_ATTR
// tcpclient_discon_cb(void *arg)//正常断开回调
// {
// 	struct espconn *pespconn = (struct espconn *)arg;
//     uart0_sendStr("断开连接\r\n");
// }

// void ICACHE_FLASH_ATTR
// tcpclient_recon_cb(void *arg, sint8 errType)//连接失败/异常回调
// {
// 	struct espconn *pespconn = (struct espconn *)arg;
// 	uart0_sendStr("连接失败\r\n");
// }

// void ICACHE_FLASH_ATTR
// tcpclient_sent_cb(void *arg)//发送回调
// {
// 	struct espconn *pespconn = (struct espconn *)arg;
// 	uart0_sendStr("发送成功\r\n");
// }

// void ICACHE_FLASH_ATTR
// tcpclient_recv(void *arg, char *pdata, unsigned short len)//接收函数
// {
// 	struct espconn *pespconn = (struct espconn *)arg;
// 	uart0_tx_buffer(pdata, len);//打印接收到的数据
// }

// void ICACHE_FLASH_ATTR
// tcpclient_connect_cb(void *arg)//连接成功回调
// {
// 	struct espconn *pespconn = (struct espconn *)arg;

// 	espconn_regist_disconcb(pespconn, tcpclient_discon_cb);//正常断开回调
// 	espconn_regist_recvcb(pespconn,tcpclient_recv);//接收到数据回调
// 	espconn_regist_sentcb(pespconn, tcpclient_sent_cb);//发送成功回调
// }

// void ICACHE_FLASH_ATTR
// tcp_client(void) //链接服务器
// {
// 	struct espconn tcpclient;
// 	uint8 ip[] = {172, 16, 21, 69}; //服务器的IP地址

// 	tcpclient.proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));

// 	os_memcpy(&tcpclient.proto.tcp->remote_ip, ip, 4);
// 	tcpclient.proto.tcp->local_port = 8080; //服务器的端口号

// 	tcpclient.type = ESPCONN_TCP;
// 	tcpclient.state = ESPCONN_NONE;
// 	// espconn_regist_connectcb(&tcpclient, tcpclient_connect_cb); //连接成功回调
// 	// espconn_regist_reconcb(&tcpclient, tcpclient_recon_cb);//连接失败回调

// 	espconn_connect(&tcpclient); //链接
// }

// es

// 	void ICACHE_FLASH_ATTR
// 	sys_init_cb(void) //初始化完成后回调函数
// {
// 	tcp_client();
// }

//串口调用此函数就说明接收到了一条完整的数据,就可以去处理了
void UartReadCallback()//定义一个函数
{
    espconn_send(&TcpClient,Usart1ReadBuff,Usart1ReadCntCopy);//串口接收的数据发给网络
}

//网络接收到数据
void TcpClientRecv(void *arg, char *pdata, unsigned short len)
{
    while(len--)
    {
        uart0_write_char(*(pdata++));//发送到串口
    }
}

//断开了连接
void TcpClientDisCon(void *arg)
{
    os_printf("\nTcpClientDisCon\n");
    espconn_connect(&TcpClient);//重新连接服务器
}

void TcpConnected(void *arg)
{
    os_printf("\nTcpConnected\n");


    //设置启用心跳包
    os_printf("\nespconn_set_opt=%d\n",espconn_set_opt(&TcpClient,ESPCONN_KEEPALIVE));//成功:0  失败:其它
    //客户端断开直接释放内存
    os_printf("\nespconn_set_opt=%d\n",espconn_set_opt(&TcpClient,ESPCONN_REUSEADDR));//成功:0  失败:其它


    //每隔ESPCONN_KEEPIDLE 开始启动心跳包探测,
    //如果探测失败,则每每隔  ESPCONN_KEEPINTVL  发送一次探测,
    //探测  ESPCONN_KEEPCNT  次以后还是无相应,则进入  espconn_reconnect_callback 回调函数  (espconn_regist_reconcb(&TcpClient, TcpConnectErr);//注册连接出错函数)

    keep_alive_sec = 30;//每隔30S开始一次探测
    espconn_set_keepalive(&TcpClient,ESPCONN_KEEPIDLE,&keep_alive_sec);

    keep_alive_sec = 1;//开始探测后,心跳包每隔1S发送一次
    espconn_set_keepalive(&TcpClient,ESPCONN_KEEPINTVL,&keep_alive_sec);

    keep_alive_sec = 3;//心跳包总共发送3次
    espconn_set_keepalive(&TcpClient,ESPCONN_KEEPCNT,&keep_alive_sec);

    espconn_regist_recvcb(&TcpClient, TcpClientRecv);//设置接收回调
    espconn_regist_disconcb(&TcpClient, TcpClientDisCon);//设置断开连接回调
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	os_printf("start...\n");
	struct station_config stationConf;
	wifi_set_opmode(STATION_MODE);
	os_memcpy(&stationConf.ssid, ssid, 32);
	os_memcpy(&stationConf.password, password, 32);
	wifi_station_set_config(&stationConf);
	wifi_station_connect();

	espconn_init();
	TcpClient.type = ESPCONN_TCP;
	TcpClient.state = ESPCONN_NONE;
	TcpClient.proto.tcp = &esptcp;

	TcpClient.proto.tcp->remote_ip[0] = 172;
	TcpClient.proto.tcp->remote_ip[1] = 16;
	TcpClient.proto.tcp->remote_ip[2] = 21;
	TcpClient.proto.tcp->remote_ip[3] = 69;

	TcpClient.proto.tcp->remote_port = 8080; //连接端口号

	espconn_connect(&TcpClient);

	espconn_regist_connectcb(&TcpClient, TcpConnected); //注册连接函数
	//espconn_regist_reconcb(&TcpClient, TcpConnectErr);	//注册连接出错函数
}
