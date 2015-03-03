#include "application.h"

#define APPINFO_MAGIC   0x12345678
#define APPINFO_VERSION 2

#define APPINFO_ADDR_START    0x08004000
#define APPINFO_ADDR_END      0x08007FFF

#define DEFAULT_ROUTER  3

#if (1 == DEFAULT_ROUTER)
#define DEFAULT_STATION_SSID    "jinfengshare"
#define DEFAULT_STATION_KEY     "12345687"
#elif(2 == DEFAULT_ROUTER)
#define DEFAULT_STATION_SSID    "homeinn"
#define DEFAULT_STATION_KEY     "87989222"
#else
#define DEFAULT_STATION_SSID    "dd-wrt"
#define DEFAULT_STATION_KEY     "88888888"
#endif

#define DEFAULT_ACTIVE_SERVER_IP        "111.206.227.37"
#define DEFAULT_POST_SERVER_IP          "111.206.227.243"
#define DEFAULT_ACTIVE_SERVER_PORT      2001
#define DEFAULT_POST_SERVER_PORT        443


static appinfo_t g_appinfo;

// Helper

// "C89346407676" => C8 93 46 40 76 76
// suppose the charactors are upper mode
static void mac_str2bin(uint8_t *macbin, char *macstr)
{
    int i;

    uint8_t low, high, byte;

    for (i = 0; i < 6; i++)
    {
        high = macstr[i << 1 + 0];
        low = macstr[i << 1 + 1];

        if ((high >= '0') || (high <= '9'))
        {
            high = high - '0';
        }
        else
        {
            high = high - 'A';
        }

        if ((low >= '0') || (low <= '9'))
        {
            low = low - '0';
        }
        else
        {
            low = low - 'A';
        }

        macbin[i] = high << 4 | low;
    }
}

void appinfo_update(void)
{
    uint32_t start = PARA_START_ADDRESS;
    uint32_t end = PARA_END_ADDRESS;

    MicoFlashInitialize(MICO_FLASH_FOR_PARA);
    MicoFlashErase(MICO_FLASH_FOR_PARA, start, end);
    MicoFlashWrite(MICO_FLASH_FOR_PARA, &start, (uint8_t*)&g_appinfo, sizeof(appinfo_t));
    FLASH_Lock();
}

void appinfo_restore(void)
{
    uint32_t start = PARA_START_ADDRESS;
    uint32_t end = PARA_END_ADDRESS;

    memset(&g_appinfo, 0, sizeof(appinfo_t));

    g_appinfo.magic = APPINFO_MAGIC;
    g_appinfo.version = APPINFO_VERSION;

    memcpy(g_appinfo.net.sta_ssid, DEFAULT_STATION_SSID, 32);
    memcpy(g_appinfo.net.sta_key, DEFAULT_STATION_KEY, 64);

    MicoFlashInitialize(MICO_FLASH_FOR_PARA);
    MicoFlashErase(MICO_FLASH_FOR_PARA, start, end);
    MicoFlashWrite(MICO_FLASH_FOR_PARA, &start, (uint8_t*)&g_appinfo, sizeof(appinfo_t));
    FLASH_Lock();
}


void appinfo_load(void)
{
    uint32_t addr = PARA_START_ADDRESS;

    MicoFlashRead(MICO_FLASH_FOR_PARA, &addr, (uint8_t*)&g_appinfo, sizeof(g_appinfo));

    if (APPINFO_MAGIC != g_appinfo.magic)
    {
        appinfo_restore();

        board_reboot(REBOOT_FLASH_ERROR);
    }

    if (APPINFO_VERSION != g_appinfo.version)
    {
        appinfo_restore();

        board_reboot(REBOOT_APPINFO_VER);
    }

    net_para_st netpara;
    getNetPara(&netpara, Station);
    memset(g_appinfo.net.mac_str, 0, 16);
    formatMACAddr(g_appinfo.net.mac_str, netpara.mac);
}

/* Jinfeng, MXCHIP, 2014.11.30
________________________________________________________________________________
                                            User Interface
*/
appinfo_t * appinfo_get(void)
{
    return &g_appinfo;
}

