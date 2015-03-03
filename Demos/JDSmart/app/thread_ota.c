#include "application.h"

#if THREAD_OTA_DEBUG
#define ota_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define ota_debug(m, ...)
#endif

#define OTA_START_ACK    "200 OK"

enum
{
    OTA_RESULT_DOWNLOAD_OK = 7001,
    OTA_RESULT_DOWNLOAD_FAILED,
    OTA_RESULT_SUCCESS,
    OTA_RESULT_FAILED,
};

// Jinfeng-Bug: end with "\r\n", not "\r\n\r\n", cause an receive error
const char http_head_get_bin[] =
    "GET %s HTTP/1.1\r\n\
Host: storage.jd.com\r\n\
Connection: keep-alive\r\n\r\n";


static int fd = -1;

uint32_t flash_addr = UPDATE_START_ADDRESS;

void ota_exit(struct otainfo_t *otainfo, int result)
{
    appinfo_t *info = appinfo_get();

    close(fd);
    fd = -1;
    otainfo->result = result;

    FLASH_Lock();

    if (OTA_RESULT_SUCCESS == result)
    {
        memset(&info->boot_table, 0, sizeof(boot_table_t));
        info->boot_table.length = flash_addr - UPDATE_START_ADDRESS;
        info->boot_table.start_address = UPDATE_START_ADDRESS;
        info->boot_table.type = 'A';
        info->boot_table.upgrade_type = 'U';
        appinfo_update();
    }

    if (OTA_RESULT_SUCCESS == result)
    {
        ota_debug("OTA >> reboot now\r\n", result);
    }
    else
    {
        ota_debug("OTA >> exit with failure: %d\r\n", result);
    }

    mico_rtos_set_semaphore(&otainfo->sem);
    mico_rtos_delete_thread(NULL);
}

void thread_ota(void *arg)
{
    int ret;
    struct otainfo_t *otainfo = (struct otainfo_t*)arg;
    char *file_addr;
    uint32_t total = 0;
    int first_frame = 1;

    int block = 0;
    int keepalive = 1;
    int timeout = 5000;
    struct sockaddr_t addr;

    flash_addr = UPDATE_START_ADDRESS;

    addr.s_ip = inet_addr("211.152.122.157");
    addr.s_port = 80;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (-1 == fd)
    {
        ota_exit(otainfo, 1);
    }
    else
    {
        setsockopt(fd, SOL_SOCKET, SO_BLOCKMODE, &block, block);
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    }

    ret = connect(fd, &addr, sizeof(addr));

    if (-1 == ret)
    {
        ota_exit(otainfo, 1);
    }

    file_addr = strstr(otainfo->url, "/devpro");

    if (NULL == file_addr)
    {
        ota_exit(otainfo, 1);
    }

    sprintf(otainfo->buffer, http_head_get_bin, file_addr);

    ret = send(fd, otainfo->buffer, strlen(otainfo->buffer), 0);

    if (ret <= 0)
    {
        ota_exit(otainfo, 1);
    }
    else
    {
        ota_debug("|>> Internet\r\n%s\r\n", otainfo->buffer);
    }

    MicoFlashInitialize(MICO_FLASH_FOR_UPDATE);

    while (1)
    {
        ret = recv(fd, otainfo->buffer, 1024, 0);

        if (-1 == ret)
        {
            uint32_t sockerr = 0;
            int sockerrlen;

            getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &sockerrlen);

            ota_debug("[ x ] recv -1 as error %d\r\n", sockerr);

            first_frame = 1;

            ota_exit(otainfo, 1);
        }
        else if (0 == ret)
        {
            ota_debug("OTA >> end with %d bytes\r\n", total);

            first_frame = 1;

            ota_exit(otainfo, OTA_RESULT_SUCCESS);
        }
        else
        {
            // first frame check
            if (first_frame)
            {
                int i;

                first_frame = 0;

                if (NULL == (strstr(otainfo->buffer, OTA_START_ACK)))
                {
                    ota_exit(otainfo, 1);
                }

                for (i = 0; i < ret - 4; i++)
                {
                    if (('\r' == otainfo->buffer[i]) &&
                            ('\n' == otainfo->buffer[i + 1]) &&
                            ('\r' == otainfo->buffer[i + 2]) &&
                            ('\n' == otainfo->buffer[i + 3]))
                    {
                        break;
                    }
                }

                if (i == ret - 4)
                {
                    // the first frame is wrong
                    ota_exit(otainfo, 5);
                }
                else
                {
                    // store the first frame
                    total = ret - (i + 4);

                    ota_debug("|<< Internet %d Bytes\r\n", total);

                    MicoFlashWrite(MICO_FLASH_FOR_UPDATE, &flash_addr, (uint8_t *)(otainfo->buffer + (i + 4)), total);

                    continue;
                }
            }

            total += ret;

            MicoFlashWrite(MICO_FLASH_FOR_UPDATE, &flash_addr, (uint8_t *)(otainfo->buffer), ret);

            ota_debug("|<< Internet %d Bytes\r\n", ret);
        }
    }
}


