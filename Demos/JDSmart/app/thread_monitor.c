#include "application.h"

#if THREAD_MONITOR_DEBUG
#define monitor_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define monitor_debug(m, ...)
#endif

static mico_semaphore_t sem_check_memory = NULL;

static void monitor_memory_print(void)
{
    micoMemInfo_t *info = mico_memory_info();

    monitor_debug("MEM: %d %d %d\r\n",
                  info->total_memory,
                  info->allocted_memory,
                  info->free_memory);
}

static void thread_monitor(void *arg)
{
#if SWITCH_WATCHDOG
    iwatchdog_init();
#endif

    mico_rtos_init_semaphore(&sem_check_memory, 1);

    while (1)
    {
#if SWITCH_WATCHDOG
        iwatchdog_feed();
#endif

        mico_rtos_get_semaphore(&sem_check_memory, MONITOR_MEMORY_CHECK);

        monitor_memory_print();
    }
}

void thread_monitor_init(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "thread monitor",
                            thread_monitor, THREAD_MONITOR_STACK, NULL);
}

