#include "application.h"

#if THREAD_REMOTE_DEBUG
#define remote_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define remote_debug(m, ...)
#endif

static uint32_t g_server_ip = 0;
static int fd_active = -1;
static int fd_active_connected = 0;
static ssl_t ssl_active = NULL;

static uint8_t remote_buffer[2048];

static mico_semaphore_t sem_uart = NULL;

const char HeartTick[] = "\
{\"code\":101,\"device\":{\"feed_id\":\"%s\",\"access_key\":\"%s\",\"firm_version\":%d}}\
\n";

const char control_ack[] =
    "{\"code\": 102,\
\"attribute\":%s,\
\"result\":0,\
\"control_resp\":{\"current_value\":0,\"stream_id\":\"switch\"},\
\"device\":{\"feed_id\":\"%s\",\"accees_key\":\"%s\"}\
}\n";

const char ota_ack[] =
    "{\"code\": 105,\
\"result\":0,\
\"serial\":%d,\
\"device\":{\"feed_id\":\"%s\",\"accees_key\":\"%s\"}\
}\n";

const char ota_end_ack[] =
    "{\"code\": 107,\
\"device\":{\"feed_id\":\"%s\",\"accees_key\":\"%s\"},\
\"firm_version\":%d,\
\"status\":%d,\
\"session_id\":\"%s\"\
}\n";

void ssl_channel_close(void);
void thread_ota(void *arg);

// Helper
static int str2int(char* pStr)
{
    cJSON jValue = {0};
    parse_number(&jValue, pStr);
    return jValue.valueint;
}

void httpdecode(char *p, int isFind)    // isFind:是否找到连续换行
{
    int i = 0;
    int isLine = 0;
    isLine = !isFind;

    while (*(p + i))
    {
        if (isLine == 0)
        {
            if (*(p + i) == '\n')
            {
                if (*(p + i + 2) == '\n')
                {
                    i += 1;
                    isLine = 1;
                }
            }

            i++;
            continue;
        }

        if ((*p = *(p + i)) == '%')
        {
            *p = *(p + i + 1) >= 'A' ? ((*(p + i + 1) & 0XDF) - 'A') + 10 : (*(p + i + 1) - '0');
            *p = (*p) * 16;
            *p += *(p + i + 2) >= 'A' ? ((*(p + i + 2) & 0XDF) - 'A') + 10 : (*(p + i + 2) - '0');
            i += 2;
        }
        else if (*(p + i) == '+')
        {
            *p = ' ';
        }

        p++;
    }

    *p = '\0';
}

int URLEncode(const char* str, const int strSize, char* result, const int resultSize)
{
    int i;
    int j = 0;//for result index
    char ch;

    if ((str == NULL) || (result == NULL) || (strSize <= 0) || (resultSize <= 0))
    {
        return 0;
    }

    for (i = 0; (i < strSize) && (j < resultSize); ++i)
    {
        ch = str[i];

        if (((ch >= 'A') && (ch < 'Z')) ||
                ((ch >= 'a') && (ch < 'z')) ||
                ((ch >= '0') && (ch < '9')))
        {
            result[j++] = ch;
        }
        else if (ch == ' ')
        {
            result[j++] = '+';
        }
        else if (ch == '.' || ch == '-' || ch == '_' || ch == '*')
        {
            result[j++] = ch;
        }
        else
        {
            if (j + 3 < resultSize)
            {
                sprintf(result + j, "%%%02X", (unsigned char)ch);
                j += 3;
            }
            else
            {
                return 0;
            }
        }
    }

    result[j] = '\0';
    return j;
}

int url_signature_encode(char *url, uint32_t urllen, char *dst, uint32_t len)
{
    char *signature_start;
    char *get_page_start;

    char *signature_buffer = (char*)malloc(128);

    if (NULL == signature_buffer)
    {
        return -1;
    }

    memset(signature_buffer, 0, 128);

    signature_start = strstr(url, "Signature");

    if (NULL == signature_start)
    {
        return -1;
    }

    URLEncode(signature_start, urllen - (signature_start - url), signature_buffer, 128);

    get_page_start = strstr(url, "/devpro");
    memcpy(dst, get_page_start, urllen - (get_page_start - url));
    signature_start = strstr(dst, "Signature");

    if (NULL == signature_start)
    {
        return -1;
    }

    memcpy(signature_start, signature_buffer, strlen(signature_buffer));

    free(signature_buffer);

    return 0;
}

static int JDCmdProcess(char *buffer)
{
    int ret;

    appinfo_t *info = appinfo_get();

    cJSON* pRoot = cJSON_Parse(buffer);

    if (pRoot == NULL) goto ERR2;

    cJSON* pJCode = cJSON_GetObjectItem(pRoot, "code");
    int code_id = pJCode->valueint;

    remote_debug("|<< Internet\r\n%s\r\n", buffer);

    switch (code_id)
    {
    case 1002:

        cJSON* pJCon = cJSON_GetObjectItem(pRoot, "control");

        if (pJCon == NULL) goto ERR1;

        int iCount = cJSON_GetArraySize(pJCon);
        int i = 0;

        for (i = 0;  i < iCount; ++i)
        {
            cJSON* pItem = cJSON_GetArrayItem(pJCon, i);

            if (NULL == pItem)
            {
                continue;
            }

            cJSON* pTmp = cJSON_GetObjectItem(pItem, "stream_id");
            char* sKey = pTmp->valuestring;

            pTmp = cJSON_GetObjectItem(pItem, "current_value");
            int iValue = 0;

            if (pTmp->type == cJSON_String)
            {
                iValue = str2int(pTmp->valuestring);
            }
            else
            {
                iValue = pTmp->valueint;
            }

            remote_debug("|<< Internet\r\n%s\r\n", sKey);

            app_cmd_proc(sKey, iValue);
        }

        cJSON* pJatt = cJSON_GetObjectItem(pRoot, "attribute");
        char* out = cJSON_PrintUnformatted(pJatt);

        int length = sprintf(remote_buffer, control_ack, out, info->jd.feed_id, info->jd.access_key);

        free(out);

        if (ssl_active)
        {
            ret = ssl_send(ssl_active, remote_buffer, length);

            if (ret > 0)
            {
                remote_debug("|>> Internet\r\n%s\r\n", remote_buffer);
            }
            else
            {
                ssl_channel_close();
            }
        }
        break;

    case 1005:
        int serial;
        char* url;
        char *session_id;
        int version;

        // Step 1: parse ota paramters
        pJCon = cJSON_GetObjectItem(pRoot, "serial");

        if (pJCon == NULL)
        {
            remote_debug("[ x ] json format error\r\n");
            cJSON_Delete(pRoot);
            return -1;
        }
        else
        {
            serial = pJCon->valueint;
        }

        pJCon = cJSON_GetObjectItem(pRoot, "session_id");

        if (pJCon == NULL)
        {
            remote_debug("[ x ] json format error\r\n");
            cJSON_Delete(pRoot);
            return -1;
        }
        else
        {
            session_id = pJCon->valuestring;
        }

        pJCon = cJSON_GetObjectItem(pRoot, "update");

        if (pJCon == NULL)
        {
            remote_debug("[ x ] json format error\r\n");
            cJSON_Delete(pRoot);
            return -1;
        }
        else
        {
            int iCount = cJSON_GetArraySize(pJCon);
            int i = 0;
            cJSON* pItem = cJSON_GetArrayItem(pJCon, 1);
            url = pItem->valuestring;

            pItem = cJSON_GetArrayItem(pJCon, 2);
            version = pItem->valueint;
        }

        // Step 2: OTA Response
        length = sprintf(remote_buffer, ota_ack,
                             serial, info->jd.feed_id, info->jd.access_key);

        if (ssl_active)
        {
            ret = ssl_send(ssl_active, remote_buffer, length);

            if (ret > 0)
            {
                remote_debug("|>> Internet\r\n%s\r\n", remote_buffer);
            }
        }

        // Step 3: Get Binary through HTTP channel
        struct otainfo_t otainfo;
        memset(&otainfo, 0, sizeof(otainfo));

        otainfo.buffer = remote_buffer;
        otainfo.url = url;
        mico_rtos_init_semaphore(&otainfo.sem, 1);

        mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "thread ota", thread_ota, THREAD_OTA_STACK, &otainfo);
        mico_rtos_get_semaphore(&otainfo.sem, MICO_WAIT_FOREVER);

        mico_rtos_deinit_semaphore(&otainfo.sem);

        // Step 4: OTA Result
        length = sprintf(remote_buffer, ota_end_ack,
                         info->jd.feed_id,
                         info->jd.access_key,
                         // use the firmware version which apeared in ota request
                         version,
                         otainfo.result,
                         session_id);

        if (ssl_active)
        {
            ret = ssl_send(ssl_active, remote_buffer, length);

            if (ret > 0)
            {
                remote_debug("|>> Internet\r\n%s\r\n", remote_buffer);
            }
            else
            {
                remote_debug("[ x ] ssl_send(active) ota result failed\r\n");
            }
        }

        // Step 5: Reboot
        board_reboot(REBOOT_OTA);

        break;
    }

    cJSON_Delete(pRoot);

    return 0;
ERR1:
    cJSON_Delete(pRoot);
ERR2:
    remote_debug("[ x ] Server json error before: [%s]\r\n", cJSON_GetErrorPtr());
    return 0;
}

void ssl_channel_close(void)
{
    int opt, optlen;

    if((-1 == fd_active) && (NULL == ssl_active))
    {
        return;
    }

    getsockopt(fd_active, SOL_SOCKET, SO_ERROR, &opt, &optlen);

    remote_debug("close ssl channel, error = %d\r\n", opt);

    if (NULL != ssl_active)
    {
        ssl_close(ssl_active);
        ssl_active = NULL;
    }

    if (-1 != fd_active)
    {
        close(fd_active);
        fd_active = -1;
    }
}

void build_ssl_channel_to_cloud(void)
{
    struct sockaddr_t addr;
    int err = 0;
    int opt = 1;
    int timeout = 5000;
    int ret;

    appinfo_t *appinfo = appinfo_get();

    // Step 1: check if domain and port
    if((NULL == appinfo->jd.domain) || (0 == appinfo->jd.port))
    {
        return;
    }

    // Step 2
    if(0 == g_server_ip)
    {
        char str_ip[16];
        ret = gethostbyname(appinfo->jd.domain, str_ip, 16);
        if(-1 == ret)
        {
            return;
        }
        else
        {
            remote_debug("DNS: %s -> %s\r\n", appinfo->jd.domain, str_ip);

            g_server_ip = inet_addr(str_ip);
        }
    }

    // Step 1
    if (-1 == fd_active)
    {
        fd_active = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        setsockopt(fd_active, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
        setsockopt(fd_active, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        addr.s_ip = g_server_ip;
        addr.s_port = appinfo->jd.port;
        ret = connect(fd_active, &addr, sizeof(addr));

        if (-1 == ret)
        {
            ssl_channel_close();
        }
        else
        {
            remote_debug("tcp channel builded\r\n");
        }
    }

    if((-1 != fd_active) && (NULL == ssl_active))
    {
        ssl_active = ssl_connect(fd_active, 0, 0, &err);

        if (NULL == ssl_active)
        {
            ssl_channel_close();
        }
        else
        {
            remote_debug("ssl channel builded\r\n");

            notify_remote_connected();;
        }
    }
}

void report_something_to_cloud(void)
{
    int ret;
    int length;

    if (NULL == ssl_active)
    {
        return;
    }

    if (NULL == sem_uart)
    {
        mico_rtos_init_semaphore(&sem_uart, 1);
    }
    else
    {
        ret = mico_rtos_get_semaphore(&sem_uart, 100);

        if (0 != ret)
        {
            return;
        }

        length = generate_network_stream(remote_buffer, 2048);

        ret = ssl_send(ssl_active, remote_buffer, length);

        if(ret > 0)
        {
            remote_debug("|>> Internet\r\n%s\r\n", remote_buffer);
        }
        else
        {
            ssl_channel_close();
        }
    }
}

void receive_something_from_cloud(void)
{
    int ret;
    fd_set readfds;
    struct timeval_t timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;

    if (NULL == ssl_active)
    {
        return;
    }

    FD_ZERO(&readfds);
    FD_SET(fd_active, &readfds);
    select(fd_active, &readfds, NULL, NULL, &timeout);

    if (FD_ISSET(fd_active, &readfds))
    {
        ret = ssl_recv(ssl_active, remote_buffer, 2048);

        if (ret <= 0)
        {
            ssl_channel_close();
        }
        else
        {
            remote_buffer[ret] = '\0';

            httpdecode(remote_buffer, 0);

            JDCmdProcess(remote_buffer);
        }
    }
}

void report_heartick(void)
{
    int ret;
    appinfo_t *info = appinfo_get();

    if (NULL == ssl_active)
    {
        return;
    }

    sprintf(remote_buffer, HeartTick, info->jd.feed_id, info->jd.access_key, FIRMWARE_VERSION_MAIN);

    ret = ssl_send(ssl_active, remote_buffer, strlen(remote_buffer));

    if (ret <= 0)
    {
        ssl_channel_close();
    }
    else
    {
        remote_debug("|>> Internet\r\n%s\r\n", remote_buffer);
    }
}

#if CYASSL_DEBUG
static void ssl_debug_window(const int level, const char *const msg)
{
    remote_debug("%s\r\n", msg);
}
#endif

void thread_remote(void *arg)
{
    static uint32_t nextime = 0;

#if CYASSL_DEBUG
    CyaSSL_SetLoggingCb(ssl_debug_window);
    CyaSSL_Debugging_ON();
#endif

    memset(remote_buffer, 0, sizeof(remote_buffer));

    while (1)
    {
        if (!is_router_connected())
        {
            msleep(100);
            continue;
        }

        if(nextime < mico_get_time())
        {
            report_heartick();

            nextime = mico_get_time() + HEART_TICK_INTERVAL;
        }

        build_ssl_channel_to_cloud();

        report_something_to_cloud();

        receive_something_from_cloud();
    }
}

void thread_remote_init(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "thread remote",
                            thread_remote, THREAD_REMOTE_STACK, NULL);
}

// User API
void uart_notify_remote(void)
{
    if (sem_uart)
    {
        mico_rtos_set_semaphore(&sem_uart);
    }
}


