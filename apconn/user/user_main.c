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
static void webconfig_get_wifi_ssid_pwd(char *urlparam);

LOCAL void ICACHE_FLASH_ATTR
parse_url(char *precv, URL_Frame *purl_frame)
{
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbufer = NULL;

    if (purl_frame == NULL || precv == NULL)
    {
        return;
    }

    pbuffer = (char *)os_strstr(precv, "Host:");

    if (pbuffer != NULL)
    {
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        os_memcpy(pbuffer, precv, length);
        os_memset(purl_frame->pSelect, 0, URLSize);
        os_memset(purl_frame->pCommand, 0, URLSize);
        os_memset(purl_frame->pFilename, 0, URLSize);

        if (os_strncmp(pbuffer, "GET ", 4) == 0)
        {
            purl_frame->Type = GET;
            pbuffer += 4;
        }
        else if (os_strncmp(pbuffer, "POST ", 5) == 0)
        {
            purl_frame->Type = POST;
            pbuffer += 5;
        }

        pbuffer++;
        str = (char *)os_strstr(pbuffer, "?");

        if (str != NULL)
        {
            length = str - pbuffer;
            os_memcpy(purl_frame->pSelect, pbuffer, length);
            str++;
            pbuffer = (char *)os_strstr(str, "=");

            if (pbuffer != NULL)
            {
                length = pbuffer - str;
                os_memcpy(purl_frame->pCommand, str, length);
                pbuffer++;
                str = (char *)os_strstr(pbuffer, "&");

                if (str != NULL)
                {
                    length = str - pbuffer;
                    os_memcpy(purl_frame->pFilename, pbuffer, length);
                }
                else
                {
                    str = (char *)os_strstr(pbuffer, " HTTP");

                    if (str != NULL)
                    {
                        length = str - pbuffer;
                        os_memcpy(purl_frame->pFilename, pbuffer, length);
                    }
                }
            }
        }

        os_free(pbufer);
    }
    else
    {
        return;
    }
}

LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;
LOCAL bool ICACHE_FLASH_ATTR
save_data(char *precv, uint16 length)
{
    bool flag = false;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    uint16 headlength = 0;
    static uint32 totallength = 0;

    ptemp = (char *)os_strstr(precv, "\r\n\r\n");

    if (ptemp != NULL)
    {
        length -= ptemp - precv;
        length -= 4;
        totallength += length;
        headlength = ptemp - precv + 4;
        pdata = (char *)os_strstr(precv, "Content-Length: ");

        if (pdata != NULL)
        {
            pdata += 16;
            precvbuffer = (char *)os_strstr(pdata, "\r\n");

            if (precvbuffer != NULL)
            {
                os_memcpy(length_buf, pdata, precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
            }
        }
        else
        {
            if (totallength != 0x00)
            {
                totallength = 0;
                dat_sumlength = 0;
                return false;
            }
        }
        if ((dat_sumlength + headlength) >= 1024)
        {
            precvbuffer = (char *)os_zalloc(headlength + 1);
            os_memcpy(precvbuffer, precv, headlength + 1);
        }
        else
        {
            precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
            os_memcpy(precvbuffer, precv, os_strlen(precv));
        }
    }
    else
    {
        if (precvbuffer != NULL)
        {
            totallength += length;
            os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
        }
        else
        {
            totallength = 0;
            dat_sumlength = 0;
            return false;
        }
    }

    if (totallength == dat_sumlength)
    {
        totallength = 0;
        dat_sumlength = 0;
        return true;
    }
    else
    {
        return false;
    }
}

LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend)
{
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK)
    {
        os_sprintf(httphead,
                   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
                   psend ? os_strlen(psend) : 0);

        if (psend)
        {
            os_sprintf(httphead + os_strlen(httphead),
                       "Content-type: Content-Type\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        }
        else
        {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    }
    else
    {
        os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend)
    {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(ptrespconn, pbuf, length);
#else
        espconn_sent(ptrespconn, pbuf, length);
#endif
    }
    else
    {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(ptrespconn, httphead, length);
#else
        espconn_sent(ptrespconn, httphead, length);
#endif
    }

    if (pbuf)
    {
        os_free(pbuf);
        pbuf = NULL;
    }
}

LOCAL void ICACHE_FLASH_ATTR
response_send(void *arg, bool responseOK)
{
    struct espconn *ptrespconn = arg;

    data_send(ptrespconn, responseOK, NULL);
}

void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length)
{
    URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    char *index = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;
    SpiFlashOpResult ret = 0;

    os_printf("len:%u\r\n", length);
    os_printf("Webserver recv:-------------------------------\r\n%s\r\n", pusrdata);

    parse_flag = save_data(pusrdata, length);
    if (parse_flag == false)
    {
        response_send(ptrespconn, false);
    }

    pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
    parse_url(precvbuffer, pURL_Frame);

    switch (pURL_Frame->Type)
    {
    case GET:
        os_printf("We have a GET request.\r\n");

        if (pURL_Frame->pFilename[0] == 0)
        {
            index = (char *)os_zalloc(457);
            if (index == NULL)
            {
                os_printf("os_zalloc error!\r\n");
                goto _temp_exit;
            }

            // Flash read/write has to be aligned to the 4-bytes boundary
            ret = spi_flash_read(500 * 4096, (uint32 *)index, 457);
            if (ret != SPI_FLASH_RESULT_OK)
            {
                os_printf("spi_flash_read err:%d\r\n", ret);
                os_free(index);
                index = NULL;
                goto _temp_exit;
            }

            // index[HTML_FILE_SIZE] = 0;   // put 0 to the end
            data_send(arg, true, index);

            os_free(index);
            index = NULL;
        }
        break;

    case POST:
        os_printf("We have a POST request.\r\n");

        //pParseBuffer = (char *)os_strstr(pusrdata, "\r\n\r\n");
        //os_printf("pParseBuffer%s\r\n",pParseBuffer);
        //if (pParseBuffer == NULL)
        //{
            webconfig_get_wifi_ssid_pwd(pusrdata);
            os_printf("We have a connect\r\n");
            data_send(arg, false, NULL);
            break;
        //}
        // Prase the POST data ...
        break;
    }

_temp_exit:;
    if (pURL_Frame != NULL)
    {
        os_free(pURL_Frame);
        pURL_Frame = NULL;
    }
}

static void ICACHE_FLASH_ATTR
webconfig_get_wifi_ssid_pwd(char *urlparam)
{
    char *p = NULL, *q = NULL;

    os_memset(ssid, 0, sizeof(ssid));

    p = (char *)os_strstr(urlparam, "ssid=");
    q = (char *)os_strstr(urlparam, "password=");
    os_printf("ssid:%s\r\n",p);
    os_printf("password:%s\r\n",q);
    if (p == NULL || q == NULL)
    {
        return;
    }
    os_memcpy(ssid, p + 5, q - p - 6);
    os_memcpy(password, q + 9, os_strlen(urlparam) - (q - urlparam) - 9);
    os_printf("ssid[%s]password[%s]\r\n", ssid, password);

    wifi_set_opmode(STATION_MODE);
    struct station_config stConf;
    stConf.bssid_set = 0;
    os_memset(&stConf.ssid, 0, sizeof(stConf.ssid));
    os_memset(&stConf.password, 0, sizeof(stConf.password));

    os_memcpy(&stConf.ssid, ssid, os_strlen(ssid));
    os_memcpy(&stConf.password, password, os_strlen(password));
    wifi_station_set_config(&stConf);
    //重启
    system_restart();
}

void ICACHE_FLASH_ATTR
webserver_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
              pesp_conn->proto.tcp->remote_ip[1], pesp_conn->proto.tcp->remote_ip[2],
              pesp_conn->proto.tcp->remote_ip[3], pesp_conn->proto.tcp->remote_port, err);
}

void ICACHE_FLASH_ATTR
webserver_discon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
              pesp_conn->proto.tcp->remote_ip[1], pesp_conn->proto.tcp->remote_ip[2],
              pesp_conn->proto.tcp->remote_ip[3], pesp_conn->proto.tcp->remote_port);
}

LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, webserver_recv);
    espconn_regist_reconcb(pesp_conn, webserver_recon);
    espconn_regist_disconcb(pesp_conn, webserver_discon);
    //espconn_regist_sentcb(pesp_conn, webserver_sent);
}

void ICACHE_FLASH_ATTR
user_webserver_init(uint32_t Local_port) //链接服务器
{
    LOCAL struct espconn user_tcp_espconn;
    LOCAL esp_tcp esptcp;
    user_tcp_espconn.type = ESPCONN_TCP;
    user_tcp_espconn.state = ESPCONN_NONE;
    user_tcp_espconn.proto.tcp = &esptcp;
    user_tcp_espconn.proto.tcp->local_port = Local_port;
    espconn_regist_connectcb(&user_tcp_espconn, webserver_listen);
    //  espconn_regist_reconcb(&webserver_espconn,webserver_recon);
    espconn_accept(&user_tcp_espconn);
}

void WIFI_Init()
{
    struct softap_config apConfig;
    wifi_set_opmode(STATIONAP_MODE);
    apConfig.ssid_len = 10;
    os_strcpy(apConfig.ssid, "ESP8266Wifi");
    os_strcpy(apConfig.password, "12345678");
    apConfig.authmode = 3;
    apConfig.beacon_interval = 100;
    apConfig.channel = 1;
    apConfig.max_connection = 4;
    apConfig.ssid_hidden = 0;
    wifi_softap_set_config_current(&apConfig);
}

void ICACHE_FLASH_ATTR
user_init(void)
{
    os_printf("start...\n");
    // struct station_config stationConf;
    // wifi_set_opmode(STATION_MODE);
    // os_memcpy(&stationConf.ssid, ssid, 32);
    // os_memcpy(&stationConf.password, password, 32);
    // wifi_station_set_config(&stationConf);
    // wifi_station_connect();
    // AP初始化
    WIFI_Init();
    // TCP初始化
    user_webserver_init(80);
}
