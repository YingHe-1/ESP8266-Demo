#ifndef __USER_LOCAL_STORAGE_H__
#define __USER_LOCAL_STORAGE_H__

struct USER_LOCAL_CONFIG
{
    char client_id[32];
    char username[32];
    char auth_key[32];
    char check_code[32];
    char default_ssid[32]; // shebei
    char default_wifi_password[64]; // jiepei@123
};

struct USER_LOCAL_CONFIG *user_read_local_storage(void);
void user_save_local_storage(struct USER_LOCAL_CONFIG *plocal_config);

#endif