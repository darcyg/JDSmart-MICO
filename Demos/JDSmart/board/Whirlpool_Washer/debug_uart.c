#include "application.h"

#define DEBUG_UART_NUM  1

mico_mutex_t printf_mutex = NULL;

static volatile ring_buffer_t  rx_buffer;
static volatile uint8_t        rx_data[128];

#if (1 == DEBUG_UART_NUM)
int fputc(int ch, FILE *f)
{
    MicoUartSend(UART_FOR_APP, &ch, 1);

    return ch;
}

void debug_init(void)
{
    mico_uart_config_t uart_config;

    mico_rtos_init_mutex(&printf_mutex);

    uart_config.baud_rate    = 115200;
    uart_config.data_width   = DATA_WIDTH_8BIT;
    uart_config.parity       = NO_PARITY;
    uart_config.stop_bits    = STOP_BITS_1;
    uart_config.flow_control = FLOW_CONTROL_DISABLED;
    uart_config.flags = UART_WAKEUP_DISABLE;

    ring_buffer_init((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, 128);
    MicoUartInitialize(UART_FOR_APP, &uart_config, (ring_buffer_t *)&rx_buffer);
}
#else
int fputc(int ch, FILE *f)
{
    while (RESET == USART_GetFlagStatus(USART6, USART_FLAG_TXE));

    USART_SendData(USART6, ch);
    return ch;
}

void debug_init(void)
{
    mico_rtos_init_mutex(&printf_mutex);

    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);

    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_USART6);

    USART_DeInit(USART6);
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1 ;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART6, &USART_InitStructure);

    USART_Cmd(USART6, ENABLE);
}
#endif

