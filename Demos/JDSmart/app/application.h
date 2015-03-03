#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdio.h>

#include "MICO.h"
#include "MICORTOS.h"
#include "MicoSocket.h"

#include "types.h"
#include "appinfo.h"
#include "appconf.h"
#include "cjson.h"

#include "board.h"

#include "stm32f4xx.h"

#include "message.h"

extern mico_mutex_t printf_mutex;

#if DEBUG_ENABLE
#define debug_out(m, ...) \
    do{\
        if(printf_mutex){\
            mico_rtos_lock_mutex(&printf_mutex);\
            printf(m, ##__VA_ARGS__);\
            mico_rtos_unlock_mutex(&printf_mutex);\
        } else {\
            printf(m, ##__VA_ARGS__);\
        }\
    } while(0)
#else
#define debug_out(m, ...) do {} while(0)
#endif

#endif
