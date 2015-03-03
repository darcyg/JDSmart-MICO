#include "application.h"

#if THREAD_SEARCH_DEBUG
#define srch_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define srch_debug(m, ...)
#endif

// <jinfeng> added temp
#pragma pack(1)
typedef struct
{
    unsigned int magic;
    unsigned int len;
    unsigned int enctype;
    unsigned char checksum;
} common_header_t;

typedef struct
{
    unsigned int type;
    unsigned char cmd[2];
} cmd_header;
#pragma pack()
// </jinfeng>

static uint8_t *search_buffer = NULL;

static int PacketBuild(char* pBuf, int enctype, int type, cJSON *root)
{
    common_header_t* pCommon = (common_header_t*)pBuf;
    pCommon->magic = 0x55AA;
    pCommon->enctype = 0; // in debug mode, no encryptocol

    pBuf += sizeof(common_header_t);
    cmd_header* pCmd = (cmd_header*)(pBuf);
    pCmd->type = type;
    memcpy(pCmd->cmd, "OK", 2);

    char* pData = pBuf + sizeof(cmd_header);

    char* psJson = cJSON_PrintUnformatted(root);
    int length = strlen(psJson);
    memcpy(pData, psJson, length);

    srch_debug("|>> Internet\r\n%s\r\n", psJson);


    free(psJson);

    pCommon->len = length + sizeof(cmd_header);

    int i = 0;
    unsigned char sum = 0;

    for (i = 0; i < pCommon->len; i++)
    {
        sum += *(pBuf + i);
    }

    pCommon->checksum = sum;
    return pCommon->len + sizeof(common_header_t);
}

static cJSON* PacketAnalyse(cmd_header**pCmd, char *pBuf, int length)
{
    common_header_t* pCommon = (common_header_t*)pBuf;

    if ((pCommon->magic & 0xFFFFFF00) == 0x30303000)
    {
        static cmd_header Cmd;
        Cmd.type = (pCommon->magic & 0xFF) - '0';
        *pCmd = &Cmd;
        cJSON* pRet = cJSON_Parse(pBuf + 4);
        return pRet;
    }
    else if ((pCommon->magic != 0x55AA) || (length != (pCommon->len + sizeof(common_header_t))))
    {
        return NULL;
    }
    else
    {
        pBuf +=  sizeof(common_header_t);
        int i = 0;
        char sum = 0;

        for (i = 0; i < pCommon->len; i++)
        {
            sum +=  *(pBuf + i);
        }

        *pCmd = (cmd_header*)pBuf;
        pBuf +=  sizeof(cmd_header);

        if (pCommon->checksum != sum)
        {
            *pCmd = NULL;
            return NULL;
        }

        srch_debug("|<< Internet\r\n%s\r\n", pBuf);

        cJSON* pRet = cJSON_Parse(pBuf);

        return pRet;
    }
}


static void udp_data_process(unsigned char *buffer, unsigned int len, int srchfd, struct sockaddr_t *dest)
{
    int ret;

    int fd;
    struct sockaddr_t addr;

    appinfo_t *appinfo;

    cmd_header* pCmd = NULL;
    cJSON* jDevice = NULL;

    fd = srchfd;
    memcpy(&addr, dest, sizeof(addr));

    appinfo = appinfo_get();

    jDevice = PacketAnalyse(&pCmd, buffer, len);

    if (jDevice == NULL)
    {
        srch_debug("PacketAnalyse error\n\r");
        return;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL)
    {
        srch_debug("cJSON_CreateObject failed\n\r");
        cJSON_Delete(jDevice);
        return;
    }

    switch (pCmd->type)
    {
    case 1:

        cJSON *pItem = cJSON_GetObjectItem(jDevice, "productuuid");

        if ((strcmp(PRODUCT_UUID, pItem->valuestring)) ||
                (pItem->valuestring[0] == 0) ||
                (strcmp("0", pItem->valuestring)))
        {
            if (appinfo->jd.feed_id[0] == '\0')
            {
                cJSON_AddStringToObject(root, "feedid", "0");
            }
            else
            {
                cJSON_AddStringToObject(root, "feedid", appinfo->jd.feed_id);
            }

            cJSON_AddStringToObject(root, "mac", appinfo->net.mac_str);
            cJSON_AddStringToObject(root, "productuuid", PRODUCT_UUID);
            int length = PacketBuild(buffer, 2, 2, root);

            sendto(fd, buffer, length, 0, &addr, sizeof(addr));
        }

        break;

    case 3:

        pItem = cJSON_GetObjectItem(jDevice, "feedid");
        strcpy(appinfo->jd.feed_id, pItem->valuestring);
        pItem = cJSON_GetObjectItem(jDevice, "accesskey");
        strcpy(appinfo->jd.access_key, pItem->valuestring);
        pItem = cJSON_GetObjectItem(jDevice, "server");
        char * pStr = cJSON_PrintUnformatted(pItem);

        char tmpstr[64];
        int i;
        memcpy(tmpstr, &pStr[2], strlen(pStr)-4);
        for(i=0; i<strlen(pStr); i++)
        {
            if(':' == tmpstr[i])
            {
                break;
            }

            appinfo->jd.domain[i] = tmpstr[i];
        }

        appinfo->jd.domain[i] = 0;

        sscanf(&tmpstr[i+1], "%d", &appinfo->jd.port);

        free(pStr);

        appinfo_update();

        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(root, "msg", "write feed_id and accesskey successfully!");
        int txLength = PacketBuild(buffer, 2, 4, root);

        sendto(fd, buffer, txLength, 0, &addr,  sizeof(addr));

        char* p = cJSON_PrintUnformatted(jDevice);
        free(p);
        break;
    }

    cJSON_Delete(root);
    cJSON_Delete(jDevice);
}


void thread_search(void *arg)
{
    int fd = -1;
    int ret;
    struct sockaddr_t addr;
    socklen_t addrLen;
    int len;

    search_buffer = (uint8_t*)malloc(THREAD_SRCH_HEAP);
    if(NULL == search_buffer)
    {
        srch_debug("can't malloc for search thread, reboot\r\n");
        mico_rtos_delete_thread(NULL);
    }

    while (1)
    {
        if (!is_router_connected())
        {
            msleep(100);
            continue;
        }

        if (fd == -1)
        {
            fd = socket(AF_INET, SOCK_DGRM, IPPROTO_UDP);
            addr.s_ip = INADDR_ANY;
            addr.s_port = 80;
            ret = bind(fd, &addr, sizeof(addr));

            if (-1 == ret)
            {
                close(fd);
                fd = -1;
            }
        }
        else
        {
            len = recvfrom(fd, search_buffer, THREAD_SRCH_HEAP, 0, &addr, &addrLen);

            if (-1 == ret)
            {
                close(fd);
                fd = -1;
            }
            else
            {
                udp_data_process(search_buffer, len, fd, &addr);
            }
        }
    }
}

void thread_search_init()
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "thread search",
                            thread_search, THREAD_SEARCH_STACK, NULL);
}

