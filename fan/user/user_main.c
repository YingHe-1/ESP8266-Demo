#include "ets_sys.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "mem.h"
#include "smartconfig.h"
#include "json/jsonparse.h"
#include "json/jsontree.h"
#include "user_interface.h"
#include "upgrade.h"
#include "user_local_storage.h"
#include "mqtt.h"
#include "http_client.h"
#include "driver/gpio16.h"
#include "at_custom.h"
#include "user_config.h"

#if ((SPI_FLASH_SIZE_MAP == 0) || (SPI_FLASH_SIZE_MAP == 1))
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 2)
#define SYSTEM_PARTITION_OTA_SIZE 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0xfb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0xfc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0xfd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0x7c000
#elif (SPI_FLASH_SIZE_MAP == 3)
#define SYSTEM_PARTITION_OTA_SIZE 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0x1fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0x7c000
#elif (SPI_FLASH_SIZE_MAP == 4)
#define SYSTEM_PARTITION_OTA_SIZE 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0x3fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0x7c000
#elif (SPI_FLASH_SIZE_MAP == 5)
#define SYSTEM_PARTITION_OTA_SIZE 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0x1fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0xfc000
#define SYSTEM_PARTITION_AT_PARAMETER_ADDR 0xfd000
#elif (SPI_FLASH_SIZE_MAP == 6)
#define SYSTEM_PARTITION_OTA_SIZE 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR 0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR 0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR 0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR 0x3fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0xfc000
#define SYSTEM_PARTITION_AT_PARAMETER_ADDR 0xfd000
#else
#error "The flash map is not supported"
#endif
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM SYSTEM_PARTITION_CUSTOMER_BEGIN

static const partition_item_t at_partition_table[] = {
	{SYSTEM_PARTITION_BOOTLOADER, 0x0, 0x1000},
	{SYSTEM_PARTITION_OTA_1, 0x1000, SYSTEM_PARTITION_OTA_SIZE},
	{SYSTEM_PARTITION_OTA_2, SYSTEM_PARTITION_OTA_2_ADDR, SYSTEM_PARTITION_OTA_SIZE},
	{SYSTEM_PARTITION_RF_CAL, SYSTEM_PARTITION_RF_CAL_ADDR, 0x1000},
	{SYSTEM_PARTITION_PHY_DATA, SYSTEM_PARTITION_PHY_DATA_ADDR, 0x1000},
	{SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR, 0x3000},
	{SYSTEM_PARTITION_AT_PARAMETER, SYSTEM_PARTITION_AT_PARAMETER_ADDR, 0x3000},
};

static char version[10] = "1.0.0";
static char api_server[100] = "http://gateway.iot.jiepei.com/device_system_api";
// static char api_server[30] = "http://8.130.27.119";
// static char api_server[30] = "http://118.89.105.31";
static int first_run = 1;

static os_timer_t os_timer;
// static os_timer_t quick_timer;
static int wifi_factory_status = 0;
static int is_factory = 0;
static int wifi_config_status = 0;
static uint8_t curr_wifi_status = STATION_IDLE;
static int mqtt_connect_status = 0;
MQTT_Client mqtt_client;
static char topic_stoc[50];
static char topic_ctos[50];
static char ttl_rec_buff[50] = {0};
static int ttl_rec_buff_index = 0;
static int need_send_reconfig = 0;
static int is_ota = 0;
static char check_code[32] = {0};
static int ttl_sleep = 104;

static long sys_run_tick = 0;
static int wifi_config_tick = 0;
static int wifi_config_keep_tick = 0;
static int wifi_config_led_tick = 0;
static int wifi_status_tick = 0;
static int wifi_disconnect_tick = 0;
static int mqtt_send_status_tick = 0;
static int mqtt_connect_status_tick = 0;
static int mqtt_rec_status_tick = 0;
static int ttl_rec_reset_tick = 0;
static int ttl_heart_tick = 0;
static struct USER_LOCAL_CONFIG *plocal_config;
static struct FAN_STATUS *pfan_status;

LOCAL void ICACHE_FLASH_ATTR ota_finished_cb(void *arg)
{
	struct upgrade_server_info *update = arg;
	if (update->upgrade_flag == true)
	{
		os_printf("OTA  Success ! rebooting!\n");
		system_upgrade_reboot();
	}
	else
	{
		os_printf("OTA failed!\n");
	}
	is_ota = 0;
}

void ICACHE_FLASH_ATTR ota_start_upgrade(char *server_ip, uint16_t port, char *path)
{
	const char *file;
	uint8_t userBin = system_upgrade_userbin_check();
	switch (userBin)
	{
	case UPGRADE_FW_BIN1:
		if (SPI_FLASH_SIZE_MAP == 5)
		{
			file = "user2.2048.new.5.bin";
		}
		else
		{
			file = "user2.4096.new.6.bin";
		}
		break;
	case UPGRADE_FW_BIN2:
		if (SPI_FLASH_SIZE_MAP == 5)
		{
			file = "user1.2048.new.5.bin";
		}
		else
		{
			file = "user1.4096.new.6.bin";
		}
		break;
	default:
		os_printf("Fail read system_upgrade_userbin_check! \n\n");
		return;
	}
	struct upgrade_server_info *update = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));
	update->pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));
	os_memcpy(&update->ip[3], &server_ip[0], 1);
	os_memcpy(&update->ip[2], &server_ip[1], 1);
	os_memcpy(&update->ip[1], &server_ip[2], 1);
	os_memcpy(&update->ip[0], &server_ip[3], 1);
	update->port = port;
	update->check_cb = ota_finished_cb;
	update->check_times = 10000;
	update->url = (uint8 *)os_zalloc(4096);
	os_printf("Http Server Address:%d.%d.%d.%d ,port: %d,filePath: %s,fileName: %s \n", IP2STR(update->ip), update->port, path, file);
	os_sprintf((char *)update->url, "GET /%s%s HTTP/1.1\r\n"
									"Host: " IPSTR ":%d\r\n"
									"Connection: keep-alive\r\n"
									"\r\n",
			   path, file, IP2STR(update->ip), update->port);
	if (system_upgrade_start(update) == false)
	{
		os_printf(" Could not start upgrade\n");
		os_free(update->pespconn);
		os_free(update->url);
		os_free(update);
	}
	else
	{
		os_printf(" Upgrading...\n");
	}
}

void get_char_bit(char chr, char *char_bit)
{
	char_bit[0] = (chr & 0b10000000) > 0 ? 1 : 0;
	char_bit[1] = (chr & 0b01000000) > 0 ? 1 : 0;
	char_bit[2] = (chr & 0b00100000) > 0 ? 1 : 0;
	char_bit[3] = (chr & 0b00010000) > 0 ? 1 : 0;
	char_bit[4] = (chr & 0b00001000) > 0 ? 1 : 0;
	char_bit[5] = (chr & 0b00000100) > 0 ? 1 : 0;
	char_bit[6] = (chr & 0b00000010) > 0 ? 1 : 0;
	char_bit[7] = (chr & 0b00000001) > 0 ? 1 : 0;
}

void send_ttl_cmd(char *cmd, int length)
{
	int i = 0;
	int j = 0;
	char char_bit[8] = {0};
	int sum = 0;
	for (i = 0; i < length; i++)
	{
		sum += cmd[i];
		get_char_bit(cmd[i], (char *)char_bit);
		GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 0);
		os_delay_us(ttl_sleep);
		for (j = 7; j >= 0; j--)
		{
			GPIO_OUTPUT_SET(GPIO_ID_PIN(12), char_bit[j]);
			os_delay_us(ttl_sleep);
		}
		GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
		os_delay_us(ttl_sleep);
	}
	sum = sum % 255;
	get_char_bit((char)sum, (char *)char_bit);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 0);
	os_delay_us(ttl_sleep);
	for (j = 7; j >= 0; j--)
	{
		GPIO_OUTPUT_SET(GPIO_ID_PIN(12), char_bit[j]);
		os_delay_us(ttl_sleep);
	}
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
}

void mqtt_send_reconfig(void)
{
	if (mqtt_connect_status != 2)
	{
		return;
	}
	char msg[512];
	os_bzero(msg, 512);
	os_sprintf(msg, "{\"action\":\"update_reconfig_time\",\"data\":{\"check_code\":\"%s\"}}", check_code);
	MQTT_Publish(&mqtt_client, topic_ctos, msg, os_strlen(msg), 2, 0);
	os_printf("mqtt_send_reconfig ok!\n");
}

void mqtt_send_status(void)
{ 
	if (mqtt_connect_status != 2)
	{
		return;
	}
	mqtt_send_status_tick = 0;
	char msg[512];
	os_bzero(msg, 512);
	if(wifi_factory_status == 1){
		is_factory = 1;
	}
	os_sprintf(msg, "{\"action\":\"update_client_status\",\"data\":{\"status_info\":{\"server\":\"%s\",\"version\":\"%s\",\"sys_run_tick\":%d,\"power\":%d,\"speed\":%d,\"mode\":%d,\"is_cus_mode\":%d,\"wave\":%d,\"light\":%d,\"hour\":%d,\"is_factory\":%d}}}",
				mqtt_client.host, version, sys_run_tick, pfan_status->power, pfan_status->speed, pfan_status->mode, pfan_status->is_cus_mode, pfan_status->wave, pfan_status->light, pfan_status->hour, is_factory);
	MQTT_Publish(&mqtt_client, topic_ctos, msg, os_strlen(msg), 2, 0);
	os_printf("mqtt_send_status ok!\n");
	if (need_send_reconfig == 1)
	{
		need_send_reconfig = 0;
		mqtt_send_reconfig();
	}
}

void mqtt_send_ip_city(char *content)
{
	if (mqtt_connect_status != 2)
	{
		return;
	}
	char msg[512];
	os_bzero(msg, 512);
	os_sprintf(msg, "{\"action\":\"update_ip_city\",\"data\":{\"ip_city\":\"%s\"}}", content);
	MQTT_Publish(&mqtt_client, topic_ctos, msg, os_strlen(msg), 2, 0);
	os_printf("mqtt_send_ip_city ok!\n", content);
}

void ICACHE_FLASH_ATTR get_ip_city_cb(char *response, int http_status, char *full_response)
{
	if (http_status != HTTP_STATUS_GENERIC_ERROR)
	{
		char *pcontent = (char *)os_strstr(full_response, "\r\n\r\n");
		if (pcontent == NULL)
		{
			return;
		}
		pcontent = pcontent + 4;
		char *pcontent_end = pcontent;
		while (pcontent_end[0] > 0)
		{
			if (pcontent_end[0] == '\r' || pcontent_end[0] == '\n')
			{
				pcontent_end[0] = 0;
			}
			pcontent_end++;
		}
		mqtt_send_ip_city(pcontent);
	}
}

void mqtt_connected_cb(uint32_t *args)
{
	MQTT_Client *client = (MQTT_Client *)args;
	mqtt_connect_status = 2;
	MQTT_Subscribe(client, topic_stoc, 2);
	os_printf("MQTT: Connected, MQTT_Subscribe: %s\n", topic_stoc);
	mqtt_send_status();
	if (first_run == 1)
	{
		first_run = 0;
		http_get("http://ip-api.com/csv/?fields=country,regionName,city,query", "Accept-Encoding: deflate\r\n", get_ip_city_cb);
	}
	char cmd[6] = {0x5A, 0x5A, 0x00, 0x02, 0x01, 0x01};
	send_ttl_cmd(cmd, 6);
}

void mqtt_disconnected_cb(uint32_t *args)
{
	mqtt_connect_status = 0;
	os_printf("MQTT: Disconnected\n");
	char cmd[6] = {0x5A, 0x5A, 0x00, 0x02, 0x01, 0x00};
	send_ttl_cmd(cmd, 6);
}

void mqtt_published_cb(uint32_t *args)
{
	os_printf("MQTT: Published\n");
}

void format_cus_mode(char *buff, char *config)
{
	int i;
	char *config_start = config;
	int last_i = 0;
	int curr_buff_index = 6;
	for (i = 0; i < 60; i++)
	{
		if (config[i] == 0)
		{
			if (i > last_i)
			{
				buff[curr_buff_index] = atoi(config_start);
				curr_buff_index++;
			}
			break;
		}
		else if (config[i] == ',' || config[i] == ';')
		{
			config[i] = 0;
			buff[curr_buff_index] = atoi(config_start);
			curr_buff_index++;
			config_start = config_start + i - last_i + 1;
			last_i = i + 1;
		}
	}
}

void mqtt_data_cb(uint32_t *args, const char *topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	mqtt_rec_status_tick = 0;
	int need_send_status = 1;
	char *topic_buf = (char *)os_zalloc(topic_len + 1);
	char *data_buf = (char *)os_zalloc(data_len + 1);
	MQTT_Client *client = (MQTT_Client *)args;
	os_memcpy(topic_buf, topic, topic_len);
	topic_buf[topic_len] = 0;
	os_memcpy(data_buf, data, data_len);
	data_buf[data_len] = 0;
	os_printf("Receive topic: %s, data: %s \r\n", topic_buf, data_buf);

	struct jsonparse_state js;
	jsonparse_setup(&js, data_buf, os_strlen(data_buf));
	int type;
	int action = 0;
	uint32 server_ip;
	int port = 0;
	char path[200] = {0};
	int power = 0;
	int speed = 0;
	int mode = 0;
	int wave = 0;
	int light = 0;
	int hour = 0;
	char config[60] = {0};
	while ((type = jsonparse_next(&js)) != 0)
	{
		if (type == JSON_TYPE_PAIR_NAME)
		{
			if (jsonparse_strcmp_value(&js, "action") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				if (jsonparse_strcmp_value(&js, "keep_alive") == 0)
				{
					action = 0;
					need_send_status = 0;
				}
				else if (jsonparse_strcmp_value(&js, "do_upgrade") == 0)
				{
					action = 1;
					need_send_status = 0;
				}
				else if (jsonparse_strcmp_value(&js, "set_power") == 0)
				{
					action = 2;
					need_send_status = 1;
				}
				else if (jsonparse_strcmp_value(&js, "set_speed") == 0)
				{
					action = 3;
					need_send_status = 1;
				}
				else if (jsonparse_strcmp_value(&js, "set_mode") == 0)
				{
					action = 4;
					need_send_status = 1;
				}
				else if (jsonparse_strcmp_value(&js, "set_cus_mode") == 0)
				{
					action = 5;
					need_send_status = 1;
				}
				else if (jsonparse_strcmp_value(&js, "set_wave") == 0)
				{
					action = 6;
					need_send_status = 1;
				}
				else if (jsonparse_strcmp_value(&js, "set_light") == 0)
				{
					action = 7;
					need_send_status = 1;
				}
				else if (jsonparse_strcmp_value(&js, "set_countdown") == 0)
				{
					action = 8;
					need_send_status = 1;
				}
			}
			else if (jsonparse_strcmp_value(&js, "server_ip") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				server_ip = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "port") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				port = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "path") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				jsonparse_copy_value(&js, path, sizeof(path));
			}
			else if (jsonparse_strcmp_value(&js, "power") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				power = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "speed") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				speed = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "mode") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				mode = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "wave") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				wave = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "light") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				light = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "hour") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				hour = jsonparse_get_value_as_int(&js);
			}
			else if (jsonparse_strcmp_value(&js, "config") == 0)
			{
				jsonparse_next(&js);
				jsonparse_next(&js);
				jsonparse_copy_value(&js, config, sizeof(config));
			}
		}
	}
	if (action == 1 && is_ota == 0)
	{
		is_ota = 1;
		ota_start_upgrade((char *)&server_ip, port, path);
	}
	else if (action == 2)
	{
		char cmd[7] = {0x5A, 0x5A, 0x00, 0x06, 0x02, 0x01};
		cmd[6] = power;
		send_ttl_cmd(cmd, 7);
		pfan_status->power = power;
	}
	else if (action == 3)
	{
		char cmd[7] = {0x5A, 0x5A, 0x00, 0x06, 0x02, 0x03};
		cmd[6] = speed;
		send_ttl_cmd(cmd, 7);
		pfan_status->speed = speed;
	}
	else if (action == 4)
	{
		char cmd[7] = {0x5A, 0x5A, 0x00, 0x06, 0x02, 0x02};
		cmd[6] = mode;
		send_ttl_cmd(cmd, 7);
		pfan_status->is_cus_mode = 0;
		pfan_status->mode = mode;
	}
	else if (action == 5)
	{
		char cmd[26] = {0x5A, 0x5A, 0x00, 0x06, 0x15, 0x07};
		format_cus_mode((char *)cmd, (char *)config);
		send_ttl_cmd(cmd, 26);
		pfan_status->is_cus_mode = 1;
		pfan_status->mode = mode;
	}
	else if (action == 6)
	{
		char cmd[7] = {0x5A, 0x5A, 0x00, 0x06, 0x02, 0x04};
		cmd[6] = wave;
		send_ttl_cmd(cmd, 7);
		pfan_status->wave = wave;
	}
	else if (action == 7)
	{
		char cmd[7] = {0x5A, 0x5A, 0x00, 0x06, 0x02, 0x06};
		cmd[6] = light;
		send_ttl_cmd(cmd, 7);
		pfan_status->light = light;
	}
	else if (action == 8)
	{
		char cmd[7] = {0x5A, 0x5A, 0x00, 0x06, 0x02, 0x05};
		cmd[6] = hour;
		send_ttl_cmd(cmd, 7);
		pfan_status->hour = hour;
	}
	if (need_send_status == 1)
	{
		mqtt_send_status();
	}
	os_free(topic_buf);
	os_free(data_buf);
}

void ICACHE_FLASH_ATTR get_mqtt_server_cb(char *response, int http_status, char *full_response)
{
	if (http_status != HTTP_STATUS_GENERIC_ERROR)
	{
		char *pcontent = (char *)os_strstr(full_response, "\r\n\r\n");
		if (pcontent == NULL)
		{
			mqtt_connect_status = 0;
			return;
		}
		bool success;
		char public_ip[16];
		os_bzero(public_ip, 16);
		pcontent += 4;
		char *psuccess = (char *)os_strstr(pcontent, ":true,");
		if (pcontent == NULL)
		{
			psuccess = (char *)os_strstr(pcontent, ":false,");
			if (pcontent == NULL)
			{
				mqtt_connect_status = 0;
				return;
			}
			psuccess[1] = '"';
			psuccess[5] = '"';
		}
		else
		{
			psuccess[1] = '"';
			psuccess[4] = '"';
		}
		psuccess = NULL;
		struct jsonparse_state js;
		jsonparse_setup(&js, pcontent, os_strlen(pcontent));
		int type;
		while ((type = jsonparse_next(&js)) != 0)
		{
			if (type == JSON_TYPE_PAIR_NAME)
			{
				if (jsonparse_strcmp_value(&js, "success") == 0)
				{
					jsonparse_next(&js);
					jsonparse_next(&js);
					if (jsonparse_strcmp_value(&js, "ru") == 0)
					{
						success = true;
					}
					else
					{
						success = false;
						break;
					}
				}
				else if (jsonparse_strcmp_value(&js, "public_ip") == 0)
				{
					jsonparse_next(&js);
					jsonparse_next(&js);
					jsonparse_copy_value(&js, public_ip, sizeof(public_ip));
				}
			}
		}
		if (success == 1 && os_strlen(public_ip) > 0)
		{
			if (mqtt_client.host != NULL)
			{
				os_free(mqtt_client.host);
			}
			mqtt_client.host = (uint8_t *)os_zalloc(os_strlen(public_ip) + 1);
			os_strcpy(mqtt_client.host, public_ip);
			mqtt_client.host[os_strlen(public_ip)] = 0;
			mqtt_connect_status = 1;
			os_printf("get mqtt server ip: %s\r\n", public_ip);
		}
		else
		{
			mqtt_connect_status = 0;
		}
	}
	else
	{
		// mqtt_connect_status = 0;
		if (check_code[0] != 0 && plocal_config->check_code[0] == 0)
		{
			os_bzero(plocal_config->check_code, 32);
			os_memcpy(plocal_config->check_code, check_code, os_strlen(check_code));
			user_save_local_storage(plocal_config);
		}
		system_restart();
	}
}

void ICACHE_FLASH_ATTR user_pre_init(void)
{
	if (!system_partition_table_regist(at_partition_table, sizeof(at_partition_table) / sizeof(at_partition_table[0]), SPI_FLASH_SIZE_MAP))
	{
		os_printf("system_partition_table_regist fail\r\n");
	}
}

void ICACHE_FLASH_ATTR smartconfig_done_my(sc_status status, void *pdata)
{
	char *check_code_tmp = NULL;
	int last_split_pos = 0;
	int i = 0;
	switch (status)
	{
	case SC_STATUS_WAIT:
		os_printf("SC_STATUS_WAIT\n");
		break;
	case SC_STATUS_FIND_CHANNEL:
		os_printf("SC_STATUS_FIND_CHANNEL\n");
		break;
	case SC_STATUS_GETTING_SSID_PSWD:
		os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
		sc_type *type = pdata;
		if (*type == SC_TYPE_ESPTOUCH)
		{
			os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
		}
		else
		{
			os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
		}
		break;
	case SC_STATUS_LINK:
		os_printf("SC_STATUS_LINK\n");
		struct station_config *sta_conf = pdata;
		while (true)
		{
			if (i > 63 || sta_conf->password[i] == 0)
			{
				break;
			}
			if (sta_conf->password[i] == '|')
			{
				last_split_pos = i;
			}
			i++;
		}
		if (last_split_pos > 0)
		{
			sta_conf->password[last_split_pos] = 0;
			check_code_tmp = sta_conf->password + last_split_pos + 1;
			os_bzero(check_code, 32);
			os_memcpy(check_code, check_code_tmp, os_strlen(check_code_tmp));
			os_printf("check_code:%s\n", check_code);
		}
		wifi_station_set_config(sta_conf);
		wifi_station_disconnect();
		wifi_station_connect();
		break;
	case SC_STATUS_LINK_OVER:
		wifi_factory_status = 0;
		is_factory = 0;
		os_printf("SC_STATUS_LINK_OVER\n");
		if (pdata != NULL)
		{
			//SC_TYPE_ESPTOUCH
			uint8 phone_ip[4] = {0};
			os_memcpy(phone_ip, (uint8 *)pdata, 4);
			os_printf("Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
		}
		smartconfig_stop();
		wifi_config_status = 0;
		GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
		need_send_reconfig = 1;
		break;
	}
}

void ttl_rec(char *cmd)
{
	if (cmd[3] == 0x00)
	{
	}
	else if (cmd[3] == 0x01)
	{
		pfan_status->power = cmd[6];
		if (cmd[18] == 0x01)
		{
			pfan_status->is_cus_mode = 1;
		}
		else
		{
			pfan_status->is_cus_mode = 0;
			pfan_status->mode = cmd[8];
		}
		pfan_status->speed = cmd[10];
		pfan_status->wave = cmd[12];
		pfan_status->hour = cmd[14];
		pfan_status->light = cmd[16];
		mqtt_send_status_tick = 500;
	}
	else if (cmd[3] == 0x02)
	{
	}
	else if (cmd[3] == 0x03)
	{
		char cmd_send[5] = {0x5A, 0x5A, 0x00, 0x03, 0x00};
		send_ttl_cmd(cmd_send, 5);
		if (mqtt_connect_status == 2)
		{
			mqtt_connect_status = 3;
			os_printf("MQTT_Disconnect for wifi_config\n");
			MQTT_Disconnect(&mqtt_client);
		}
		wifi_config_keep_tick = 0;
		wifi_config_tick = 0;
		wifi_config_status = 1;
		wifi_config_led_tick = 0;
		wifi_station_disconnect();
		smartconfig_set_type(SC_TYPE_ESPTOUCH);
		wifi_set_opmode_current(STATION_MODE);
		smartconfig_stop();
		smartconfig_start(smartconfig_done_my);
		os_printf("Start wifi config\n");
	}
	else if (cmd[3] == 0x04)
	{
		char cmd_send[6] = {0x5A, 0x5A, 0x00, 0x02, 0x01, 0x01};
		if (mqtt_connect_status == 2)
		{
			send_ttl_cmd(cmd_send, 6);
		}
		else
		{
			cmd_send[5] = 0;
			send_ttl_cmd(cmd_send, 6);
		}
	}
	else if (cmd[3] == 0x07)
	{
		if (cmd[5] == 0x01)
		{
			pfan_status->power = cmd[6];
		}
		else if (cmd[5] == 0x02)
		{
			pfan_status->is_cus_mode = 0;
			pfan_status->mode = cmd[6];
		}
		else if (cmd[5] == 0x03)
		{
			pfan_status->speed = cmd[6];
		}
		else if (cmd[5] == 0x04)
		{
			pfan_status->wave = cmd[6];
		}
		else if (cmd[5] == 0x05)
		{
			pfan_status->hour = cmd[6];
		}
		else if (cmd[5] == 0x06)
		{
			pfan_status->light = cmd[6];
		}
		else if (cmd[5] == 0x07)
		{
			pfan_status->is_cus_mode = cmd[6];
		}
		mqtt_send_status_tick = 500;
	}
}

void ICACHE_FLASH_ATTR os_timer_cb()
{
	int i;
	sys_run_tick++;
	if (GPIO_INPUT_GET(0) == 0 && wifi_config_status == 0)
	{
		wifi_config_tick++;
		if (wifi_config_tick > 20)
		{
			if (mqtt_connect_status == 2)
			{
				mqtt_connect_status = 3;
				os_printf("MQTT_Disconnect for wifi_config\n");
				MQTT_Disconnect(&mqtt_client);
			}
			wifi_config_keep_tick = 0;
			wifi_config_tick = 0;
			wifi_config_status = 1;
			wifi_config_led_tick = 0;
			wifi_station_disconnect();
			smartconfig_set_type(SC_TYPE_ESPTOUCH);
			wifi_set_opmode_current(STATION_MODE);
			smartconfig_stop();
			smartconfig_start(smartconfig_done_my);
			os_printf("Start wifi config\n");
		}
	}
	else
	{
		wifi_config_tick = 0;
	}
	if (wifi_config_status == 1)
	{
		if (wifi_config_keep_tick < 1000)
		{
			wifi_config_keep_tick++;
			if (wifi_config_led_tick < 5)
			{
				wifi_config_led_tick++;
			}
			else
			{
				wifi_config_led_tick = 0;
				GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1 - GPIO_INPUT_GET(GPIO_ID_PIN(4)));
			}
		}
		else
		{
			wifi_config_keep_tick = 0;
			smartconfig_stop();
			wifi_config_status = 0;
			GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
			wifi_set_opmode(STATION_MODE);
			wifi_station_disconnect();
			wifi_station_connect();
		}
	}
	else
	{
		if (GPIO_INPUT_GET(GPIO_ID_PIN(4)) != GPIO_INPUT_GET(GPIO_ID_PIN(5)))
		{
			GPIO_OUTPUT_SET(GPIO_ID_PIN(4), GPIO_INPUT_GET(GPIO_ID_PIN(5)));
		}
	}
	if (wifi_config_status == 0)
	{
		if (wifi_status_tick < 10)
		{
			wifi_status_tick++;
		}
		else
		{
			wifi_status_tick = 0;
			curr_wifi_status = wifi_station_get_connect_status();
			struct ip_info ip_config;
			wifi_get_ip_info(STATION_IF, &ip_config);
			if (curr_wifi_status == STATION_GOT_IP && ip_config.ip.addr != 0)
			{
				if (mqtt_connect_status == 0)
				{
					mqtt_connect_status = 3;
					http_post(api_server, "action=emqx_server:recommend_server", "content-type: application/x-www-form-urlencoded\r\n", get_mqtt_server_cb);
				}
				else if (mqtt_connect_status == 1)
				{
					mqtt_connect_status = 3;
					os_printf("MQTT_Connect\n");
					MQTT_Connect(&mqtt_client);
				}
			}
			else
			{
				if (mqtt_connect_status == 2)
				{
					mqtt_connect_status = 3;
					os_printf("MQTT_Disconnect for wifi_status\n");
					MQTT_Disconnect(&mqtt_client);
				}
			}
		}
	}
	if (curr_wifi_status == STATION_GOT_IP && wifi_config_status == 0)
	{
		if (mqtt_send_status_tick < 500)
		{
			mqtt_send_status_tick++;
			if (mqtt_send_status_tick == 498)
			{
				char cmd[5] = {0x5A, 0x5A, 0x00, 0x01, 0x00};
				send_ttl_cmd(cmd, 5);
			}
		}
		else
		{
			mqtt_send_status_tick = 0;
			mqtt_send_status();
		}
		if (mqtt_rec_status_tick < 700)
		{
			mqtt_rec_status_tick++;
		}
		else
		{
			mqtt_rec_status_tick = 0;
			mqtt_connect_status = 3;
			os_printf("MQTT_Disconnect for mqtt_rec_status_tick\n");
			MQTT_Disconnect(&mqtt_client);
		}
	}
	if (mqtt_connect_status == 3)
	{
		if (mqtt_connect_status_tick > 30)
		{
			mqtt_connect_status = 0;
			mqtt_connect_status_tick = 0;
		}
		else
		{
			mqtt_connect_status_tick++;
		}
	}
	else
	{
		mqtt_connect_status_tick = 0;
	}
	if (wifi_config_status == 0)
	{
		if (curr_wifi_status != STATION_GOT_IP)
		{
			wifi_disconnect_tick++;
			if (wifi_disconnect_tick > 1800)
			{
				wifi_disconnect_tick = 0;
				wifi_station_disconnect();
				wifi_station_connect();
			}
		}
		else
		{
			wifi_disconnect_tick = 0;
		}
	}
	if (ttl_heart_tick < 150)
	{
		ttl_heart_tick++;
	}
	else
	{
		ttl_heart_tick = 0;
		char cmd[5] = {0x5A, 0x5A, 0x00, 0x00, 0x00};
		send_ttl_cmd(cmd, 5);
	}
	if (ttl_rec_buff_index > 0)
	{
		if (ttl_rec_reset_tick < 2)
		{
			ttl_rec_reset_tick++;
		}
		else
		{
			int ttl_sum = 0;
			int ttl_checked = 0;
			if (ttl_rec_buff_index > 4)
			{

				for (i = 0; i < ttl_rec_buff_index - 1; i++)
				{
					ttl_sum += ttl_rec_buff[i];
				}
				if (ttl_sum % 255 == ttl_rec_buff[ttl_rec_buff_index - 1])
				{
					ttl_checked = 1;
				}
			}
			if (ttl_checked == 1)
			{
				// os_printf("ttl_rec:");
				// for (i = 0; i < ttl_rec_buff_index; i++)
				// {
				// 	os_printf(" %d", ttl_rec_buff[i]);
				// }
				// os_printf("\n");
				ttl_rec((char *)ttl_rec_buff);
			}
			ttl_rec_reset_tick = 0;
			os_bzero(ttl_rec_buff, 50);
			ttl_rec_buff_index = 0;
		}
	}
}

// void ICACHE_FLASH_ATTR quick_timer_cb()
// {
// }

int bstoi(char *ps, int size)
{
	int n = 0;
	int i = 0;
	for (i = 0; i < size; ++i)
	{
		n = n * 2 + (ps[i] - 0);
	}
	return n;
}

void gpio_intr_handler()
{
	uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	ETS_GPIO_INTR_DISABLE();
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
	int i = 0;
	if (gpio_status & BIT(14))
	{
		char tmp_bit[8] = {0};
		os_delay_us(109);
		for (i = 0; i < 8; i++)
		{
			tmp_bit[7 - i] = GPIO_INPUT_GET(GPIO_ID_PIN(14));
			if (i < 7)
			{
				os_delay_us(109);
			}
		}
		ttl_rec_buff[ttl_rec_buff_index] = bstoi((char *)&tmp_bit, 8);
		ttl_rec_buff_index++;
		ttl_rec_reset_tick = 0;
	}
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(14));
	ETS_GPIO_INTR_ENABLE();
}

void ICACHE_FLASH_ATTR init_user_at()
{
	// TX
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(12), 1);
	// RX
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	GPIO_DIS_OUTPUT(GPIO_ID_PIN(14));
	ETS_GPIO_INTR_DISABLE();
	ETS_GPIO_INTR_ATTACH(&gpio_intr_handler, NULL);
	gpio_pin_intr_state_set(GPIO_ID_PIN(14), GPIO_PIN_INTR_NEGEDGE);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(14));
	ETS_GPIO_INTR_ENABLE();
}

void ICACHE_FLASH_ATTR user_init(void)
{
	plocal_config = user_read_local_storage();
	pfan_status = (struct FAN_STATUS *)os_zalloc(sizeof(struct FAN_STATUS));
	if (plocal_config->check_code[0] != 0)
	{
		os_bzero(check_code, 32);
		os_memcpy(check_code, plocal_config->check_code, os_strlen(plocal_config->check_code));
		os_bzero(plocal_config->check_code, 32);
		user_save_local_storage(plocal_config);
		need_send_reconfig = 1;
	}
	uint8_t user_bin = system_upgrade_userbin_check();
	os_printf("system start version:%s, user_bin:%d, SPI_FLASH_SIZE_MAP:%d, client_id:%s\n", version, user_bin, SPI_FLASH_SIZE_MAP, plocal_config->client_id);
	wifi_set_opmode(STATION_MODE);
	char ssid[32] = "YinghedeiPhone";
	char password[64] = "f8vm5uxrwncpx";
	struct station_config stationConf;
	wifi_station_get_config_default(&stationConf);
	if (os_strlen(stationConf.ssid) == 0 || strcmp(stationConf.ssid,ssid)!=0)
	{
		stationConf.bssid_set = 0;
		os_memcpy(&stationConf.ssid, ssid, 32);
		os_memcpy(&stationConf.password, password, 64);
		bool result = wifi_station_set_config(&stationConf);
		os_printf("test2022-3-15");
		os_printf("user_set_station_config result:%d\n", result);
		wifi_factory_status = 1;
	}
	wifi_station_connect();
	os_timer_disarm(&os_timer);
	os_timer_setfn(&os_timer, (os_timer_func_t *)os_timer_cb, NULL);
	os_timer_arm(&os_timer, 100, 1);
	// os_timer_disarm(&quick_timer);
	// os_timer_setfn(&quick_timer, (os_timer_func_t *)quick_timer_cb, NULL);
	// os_timer_arm(&quick_timer, 5, 1);
	os_bzero(topic_stoc, 50);
	os_bzero(topic_ctos, 50);
	os_sprintf(topic_stoc, "%s%s", "stoc/", plocal_config->client_id);
	os_sprintf(topic_ctos, "%s%s", "ctos/", plocal_config->client_id);
	MQTT_InitConnection(&mqtt_client, "127.0.0.1", 1883, 0);
	MQTT_InitClient(&mqtt_client, plocal_config->client_id, plocal_config->username, plocal_config->auth_key, 60, 0);
	MQTT_OnConnected(&mqtt_client, mqtt_connected_cb);
	MQTT_OnDisconnected(&mqtt_client, mqtt_disconnected_cb);
	MQTT_OnPublished(&mqtt_client, mqtt_published_cb);
	MQTT_OnData(&mqtt_client, mqtt_data_cb);
	GPIO_DIS_OUTPUT(GPIO_ID_PIN(0));
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
	init_user_at();
	char cmd[5] = {0x5A, 0x5A, 0x00, 0x01, 0x00};
	send_ttl_cmd(cmd, 5);
}
