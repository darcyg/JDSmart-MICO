#ifndef APPINFO_H
#define APPINFO_H

#include "types.h"

typedef struct
{
    uint8_t feed_id[64];
    uint8_t access_key[64];
    uint8_t domain[64];
    uint16_t port;
} jd_smart_t;

typedef struct
{
    // MAC address
    char mac_str[18];

    // Simple Station Infomation
    char sta_ssid[32];
    char sta_key[64];

    // Advanced Station Infomation
    char adv_sta_ssid[32];
    uint8_t adv_sta_bssid[6];
    uint8_t adv_sta_channel;
    uint8_t adv_sta_security;
    uint8_t adv_sta_key[64];
    uint8_t adv_sta_keylen;
} netconf_t;

/* Upgrade iamge should save this table to flash */
#pragma pack(1)
typedef struct  _boot_table_t {
  uint32_t start_address; // the address of the bin saved on flash.
  uint32_t length; // file real length
  uint8_t version[8];
  uint8_t type; // B:bootloader, P:boot_table, A:application, D: 8782 driver
  uint8_t upgrade_type; //u:upgrade,
  uint8_t reserved[6];
}boot_table_t;
#pragma pack()

#pragma pack(1)
typedef struct
{
    boot_table_t boot_table;
    uint32_t magic;
    uint32_t version;
    netconf_t net;
    jd_smart_t jd;
} appinfo_t;
#pragma pack()

struct otainfo_t
{
    char *buffer;
    char *url;
    int result;
    mico_semaphore_t sem;
};

appinfo_t * appinfo_get(void);

#endif
