#include "application.h"

#if THREAD_UART_DEBUG
#define uart_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define uart_debug(m, ...)
#endif

#define DEVSTATUS_CHECK_INTERVAL     500

static uint8_t *uart_buffer = NULL;

void check_device_status(void)
{
    static int count = 0;

    SendFrame_CheckStatus();

    if(20 == count++)
    {
        SendFrame_NetLink();
    }
}

void thread_uart(void *arg)
{
    uint32_t ret;
    int i;

    uart_buffer = (uint8_t*)malloc(THREAD_UART_HEAP);

    while (1)
    {
        ret = get_device_frame(uart_buffer, THREAD_UART_HEAP, MICO_WAIT_FOREVER);

        if (-1 == ret)
        {
            //check_device_status();
            
            continue;
        }

        device_cmd_process(uart_buffer, ret);
    }
}

void thread_uart_init(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "thread uart",
                            thread_uart, THREAD_UART_STACK, NULL);
}

