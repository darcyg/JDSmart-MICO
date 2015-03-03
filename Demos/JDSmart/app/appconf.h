#ifndef APPCONFIG_H
#define APPCONFIG_H

// Low Power
#define RF_LOW_POWER    1
#define UART_PWR_SAVE   1

// TCP/IP

// Thread Stack Size
#define THREAD_MONITOR_STACK    1024
#define THREAD_SYSTEM_STACK     1024
#define THREAD_REMOTE_STACK     (20*1024)
#define THREAD_OTA_STACK        1024
#define THREAD_SEARCH_STACK     1024
#define THREAD_UART_STACK       2048

// Thread Heap Size
#define THREAD_SRCH_HEAP        1024
#define THREAD_UART_HEAP        1024

// Device Specified
#define WAIT4DEVICE_UP  0

// System
#define SWITCH_WATCHDOG         0
#define EASYLINK_TIMEOUT        (2*60)

// Thread Remote
#define HEART_TICK_INTERVAL     5000

// Thread Monitor
#define MONITOR_MEMORY_CHECK    5000

// Debug Information Switcher
#define DEBUG_ENABLE            1

#define CYASSL_DEBUG            0

#define MEMORY_DEBUG            1

#define APP_DEBUG               1

#define THREAD_MONITOR_DEBUG    1
#define THREAD_REMOTE_DEBUG     1
#define THREAD_OTA_DEBUG        1
#define THREAD_SEARCH_DEBUG     1
#define THREAD_NETWORK_DEBUG    1
#define THREAD_UART_DEBUG       1

#define PORT_DEBUG              1

#endif

