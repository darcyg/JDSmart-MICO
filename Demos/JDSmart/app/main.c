#include "application.h"

#define JDSMART_FRAMEWORK_VERSION_MAIN   2
#define JDSMART_FRAMEWORK_VERSION_SUB    2


#if APP_DEBUG
#define app_debug(m, ...) debug_out(m, ##__VA_ARGS__)
#else
#define app_debug(m, ...)
#endif

static const char *jdsmart_version_string =
"\
\r\n\
-----------------------------------------\r\n\
|      JDSmart Framework V%d.%d           |\r\n\
|                                       |\r\n\
|   Copyright 2014, 2015, MXCHIP Inc.   |\r\n\
-----------------------------------------\r\n\
\r\n\
";

static void jdsmart_framwork_information(void)
{
    app_debug(jdsmart_version_string,
        JDSMART_FRAMEWORK_VERSION_MAIN,
        JDSMART_FRAMEWORK_VERSION_SUB);
}

int application_start(void)
{
#if DEBUG_ENABLE
    debug_init();
#endif

    jdsmart_framwork_information();

    print_memory_usage_info("system start");

    msleep(WAIT4DEVICE_UP);

    board_init();

    print_memory_usage_info("board inited");

    appinfo_load();

    thread_network_init();
    thread_monitor_init();
    thread_uart_init();
    thread_search_init();
    thread_remote_init();

    //print_memory_usage_info("all thread created");

    while (1)
    {
        sleep(60);
    }
}

