#ifndef MESSAGE_H
#define MESSAGE_H

typedef enum
{
    NM_INIT,
        
    NM_EL_ON,
    NM_EL_EXTRA,
    NM_EL_OFF,

    NM_SCAN_ON,
    NM_SCAN_OFF,

    NM_STA_ON,
    NM_STA_OFF,
    NM_STA_UP,
    NM_STA_DOWN,

    NM_AP_ON,
    NM_AP_OFF,
    NM_AP_UP,
    NM_AP_DOWN,

    NM_CLOUD_ON,
    NM_CLOUD_OFF,

    NM_DHCP,
    NM_APINFO,

    NM_JOIN_FAIL,

    NM_NEED_REBOOT,
} NetMsg;

extern mico_queue_t g_nm_queue;

typedef struct
{
    uint32_t msg;
    uint32_t param;
} MSG;

typedef mico_queue_t QUEUE;

int SendMessage(QUEUE *queue, MSG *msg);
int PostMessage(QUEUE *queue, MSG *msg);

#endif