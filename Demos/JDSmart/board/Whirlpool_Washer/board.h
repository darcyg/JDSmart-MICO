#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx.h"

#include "port.h"

enum
{
    REBOOT_OTA = 1,
    REBOOT_FLASH_ERROR,
    REBOOT_APPINFO_VER,
    REBOOT_HARDFAULT,
};

enum
{
    RFLED_MODE_INIT = 1,
    RFLED_MODE_ON,
    RFLED_MODE_OFF,
    RFLED_MODE_QUICK,
    RFLED_MODE_NORMAL,
    RFLED_MODE_SLOW,
    RFLED_MODE_BLINK_ONCE,
};

void RFLED_Init();
void RFLED_Set_Mode(int mode);

// basci frequence: 100ms, which is the frequence equals to network thread
void RFLED_Interrupt_Trigger(void);

#endif

