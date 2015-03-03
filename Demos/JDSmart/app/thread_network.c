#include "application.h"

#if THREAD_NETWORK_DEBUG
#define net_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define net_debug(m, ...)
#endif

static int g_router_connected = 0;

static mico_queue_t g_nm_queue = NULL;

//############################################################
// Help Functions

void station_start(void)
{
    appinfo_t *appinfo = appinfo_get();

    network_InitTypeDef_st wNetConfig;

    memset(&wNetConfig, 0, sizeof(wNetConfig));

    wNetConfig.wifi_mode = Station;
    strcpy(wNetConfig.wifi_ssid, appinfo->net.sta_ssid);
    strcpy(wNetConfig.wifi_key, appinfo->net.sta_key);
    wNetConfig.dhcpMode = DHCP_Client;

    net_debug("connect to %s ( %s )\r\n", wNetConfig.wifi_ssid, wNetConfig.wifi_key);

    StartNetwork(&wNetConfig);
}

// C8:93:46:40:A5:16
static char *mac_bin2str(uint8_t *mac)
{
    static char strmac[18];
    int i;
    uint8_t high, low;

    for (i = 0; i < 6; i++)
    {
        high = mac[i] >> 4;
        low = mac[i] & 0xF;

        if (high < 0x0a)
        {
            strmac[i * 3 + 0] = high + '0';
        }
        else
        {
            strmac[i * 3 + 0] = high - 0xa + 'A';
        }

        if (low < 0x0a)
        {
            strmac[i * 3 + 1] = low + '0';
        }
        else
        {
            strmac[i * 3 + 1] = low - 0xa + 'A';
        }

        strmac[i * 3 + 2] = (5 == i) ? '\0' : ':';
    }

    return strmac;
}

//############################################################
// Callbacks

void WifiStatusHandler(int event)
{
    MSG msg;

    msg.param = NULL;

    switch (event)
    {
    case 1:
        net_debug("station up\r\n");
        msg.msg = NM_STA_UP;
        break;

    case 2:
        net_debug("station down\r\n");
        msg.msg = NM_STA_DOWN;
        break;
    }

    SendMessage(&g_nm_queue, &msg);
}

void connected_ap_info(apinfo_adv_t *ap_info, char *key, int key_len)
{
    net_debug("get router advanced information\r\n");

    appinfo_t *appinfo = (appinfo_t*)appinfo_get();

    memcpy(appinfo->net.adv_sta_ssid, ap_info->ssid, 32);
    memcpy(appinfo->net.adv_sta_bssid, ap_info->bssid, 6);
    appinfo->net.adv_sta_channel = ap_info->channel;
    appinfo->net.adv_sta_security = ap_info->security;
    memcpy(appinfo->net.adv_sta_key, key, 64);
    appinfo->net.adv_sta_keylen = key_len;

    appinfo_update();
}

void NetCallback(net_para_st *pnet)
{
    net_debug("IP = %s\r\n", pnet->ip);

    MSG msg;

    msg.msg = NM_DHCP;
    msg.param = (uint32_t)pnet;

    SendMessage(&g_nm_queue, &msg);
}

void RptConfigmodeRslt(network_InitTypeDef_st *nwkpara)
{
    net_debug("%s ( %s )\r\n", nwkpara->wifi_ssid, nwkpara->wifi_key);

    MSG msg;

    msg.msg = NM_EL_OFF;
    msg.param = (uint32_t)nwkpara;

    SendMessage(&g_nm_queue, &msg);

    msg.msg = NM_STA_ON;
    msg.param = NULL;

    SendMessage(&g_nm_queue, &msg);
}

void easylink_user_data_result(int datalen, char*data)
{
    net_debug("extra data: %s\r\n", data);

    MSG msg;

    msg.msg = NM_EL_EXTRA;
    msg.param = (uint32_t)data;

    SendMessage(&g_nm_queue, &msg);
}

void join_fail(int err)
{
    net_debug("join router failed : %d\r\n", err);

    MSG msg;

    msg.msg = NM_JOIN_FAIL;
    msg.param = err;

    SendMessage(&g_nm_queue, &msg);
}

void wifi_reboot_event(void)
{
    net_debug("[Error] network error, need reboot\r\n");

    MSG msg;

    msg.msg = NM_NEED_REBOOT;
    msg.param = NULL;

    SendMessage(&g_nm_queue, &msg);
}

void ApListCallback(ScanResult_adv *pApList)
{
    net_debug("scan over with %d AP found\r\n", pApList->ApNum);

    MSG msg;

    msg.msg = NM_SCAN_OFF;
    msg.param = (uint32_t)pApList;

    SendMessage(&g_nm_queue, &msg);
}

void notify_remote_connected(void)
{
    net_debug("cloud connected\r\n");

    MSG msg;

    msg.msg = NM_CLOUD_ON;
    msg.param = NULL;

    SendMessage(&g_nm_queue, &msg);
}

//############################################################

static void thread_network(void *arg)
{
    MSG msg;
    appinfo_t *info = appinfo_get();

    mico_rtos_init_queue(&g_nm_queue, "network", sizeof(MSG), 1);

    msg.msg = NM_STA_ON;
    msg.param = NULL;
    SendMessage(&g_nm_queue, &msg);

    while(1)
    {
        mico_rtos_pop_from_queue(&g_nm_queue, &msg, MICO_WAIT_FOREVER);

        netlink_message_process(&msg);

        switch(msg.msg)
        {
            case NM_EL_ON:
                net_debug("open easylink2\r\n");
                OpenEasylink2(EASYLINK_TIMEOUT);
                break;
            case NM_EL_OFF:
                network_InitTypeDef_st *param = (network_InitTypeDef_st*)msg.param;
                memcpy(info->net.sta_ssid, param->wifi_ssid, sizeof(param->wifi_ssid));
                memcpy(info->net.sta_key, param->wifi_key, sizeof(param->wifi_key));
                appinfo_update();
                break;
            case NM_STA_ON:
                station_start();
                break;
            case NM_STA_UP:
                g_router_connected = 1;
                break;
            case NM_STA_DOWN:
                g_router_connected = 0;
                break;
            case NM_SCAN_ON:
                net_debug("start scan\r\n");
                mxchipStartScan();
                break;
        }
    }
}

//############################################################
// User API

void thread_network_init(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "thread network",
                            thread_network, THREAD_SYSTEM_STACK, NULL);
}

void User_Start_Easylink(void)
{
    MSG msg;

    msg.msg = NM_EL_ON;
    msg.param = NULL;

    SendMessage(&g_nm_queue, &msg);
}

void User_Start_Scan(void)
{
    MSG msg;

    msg.msg = NM_SCAN_ON;
    msg.param = NULL;

    SendMessage(&g_nm_queue, &msg);
}

uint8_t Get_WIFI_Strength(void)
{
    uint8_t stength;

    LinkStatusTypeDef ap_state;

    CheckNetLink(&ap_state);

    if(ap_state.is_connected)
    {
        stength = ap_state.wifi_strength;
    }
    else
    {
        stength = 0;
    }

    return stength;
}

int is_router_connected(void)
{
    return g_router_connected;
}

void network_init(void)
{
    mxchipInit();

#if RF_LOW_POWER
    ps_enable();
#endif

    set_tcp_keepalive(3, 5);
}

