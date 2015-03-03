#include "application.h"

#if MEMORY_DEBUG
#define memory_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define memory_debug(m, ...)
#endif

/*----------------------------------------
    How to print 50% in this format ?
    1. %d%      Right
    2. %d\% ?  Wrong
------------------------------------------*/
static const char *memory_usage_info_module =
"_______________________________\r\n\
Total: %dK\r\n\
Used:  %dK\t%d%\r\n\
Free:  %dK\t%d%\r\n\
\r\n";

void print_memory_usage_info(char *title)
{
    micoMemInfo_t *info = mico_memory_info();

    memory_debug("%s\r\n", title);

    memory_debug(memory_usage_info_module,

                 info->total_memory >> 10,

                 info->allocted_memory >> 10,
                 100 * info->allocted_memory / info->total_memory,

                 info->free_memory >> 10,
                 100 * info->free_memory / info->total_memory);
}

uint8_t calc_sum_8(uint8_t *src, uint8_t len)
{
    uint8_t i, tmp;

    tmp = 0;

    for (i = 0; i < len; i++)
        tmp += src[i];

    tmp = ~tmp + 1;

    return tmp;
}

void formatMACAddr(void *destAddr, void *srcAddr)
{
    sprintf((char *)destAddr, "%c%c-%c%c-%c%c-%c%c-%c%c-%c%c", \
            toupper(*(char *)srcAddr), toupper(*((char *)(srcAddr) + 1)), \
            toupper(*((char *)(srcAddr) + 2)), toupper(*((char *)(srcAddr) + 3)), \
            toupper(*((char *)(srcAddr) + 4)), toupper(*((char *)(srcAddr) + 5)), \
            toupper(*((char *)(srcAddr) + 6)), toupper(*((char *)(srcAddr) + 7)), \
            toupper(*((char *)(srcAddr) + 8)), toupper(*((char *)(srcAddr) + 9)), \
            toupper(*((char *)(srcAddr) + 10)), toupper(*((char *)(srcAddr) + 11)));
}

void core_dump(void)
{
    uint32_t flash_ota_addr = (uint32_t)0x08060000;
    uint32_t ram_start_addr = (uint32_t)0x20000000;
    uint32_t ram_size = 0x20000; // 128K

    FLASH_If_Init();

    FLASH_If_Write(&flash_ota_addr, (uint32_t*)ram_start_addr, ram_size);
}


