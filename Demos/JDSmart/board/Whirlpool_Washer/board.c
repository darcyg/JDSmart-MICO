#include "application.h"

#define USER_DEFINE_RELOAD 0x001E2D3C

static volatile ring_buffer_t  rx_buffer;
static volatile uint8_t        rx_data[1024];

void board_init(void)
{
    mico_uart_config_t uart_config;

    uart_config.baud_rate    = 115200;
    uart_config.data_width   = DATA_WIDTH_8BIT;
    uart_config.parity       = NO_PARITY;
    uart_config.stop_bits    = STOP_BITS_1;
    uart_config.flow_control = FLOW_CONTROL_DISABLED;
    uart_config.flags = UART_WAKEUP_DISABLE;

    network_init();

    network_led_init();

    easylink_button_init();

    ring_buffer_init((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, 1024);
    MicoUartInitialize(UART_FOR_APP, &uart_config, (ring_buffer_t *)&rx_buffer);
}

int board_reboot_check(void)
{
    int reason;

    if (SET == RCC_GetFlagStatus(RCC_FLAG_SFTRST))
    {
        app_debug("BOARD >> system reboot by software\r\n");
    }
    else if (SET == RCC_GetFlagStatus(RCC_FLAG_IWDGRST))
    {
        app_debug("BOARD >> system reboot by iwatchdog\r\n");
    }
    else
    {
        app_debug("BOARD >> system boot by other method\r\n");
    }

    if (RTC_ReadBackupRegister(RTC_BKP_DR1) == USER_DEFINE_RELOAD)
    {
        reason = RTC_ReadBackupRegister(RTC_BKP_DR2);
        RTC_WriteBackupRegister(RTC_BKP_DR1, 0);

        app_debug("BOARD >> reboot with reason: %d\r\n", reason);
    }

    return reason;
}

void board_reboot(int reason)
{
    RTC_WriteBackupRegister(RTC_BKP_DR1, USER_DEFINE_RELOAD);
    RTC_WriteBackupRegister(RTC_BKP_DR2, reason);

    NVIC_SystemReset();
}


