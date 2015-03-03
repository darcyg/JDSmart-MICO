/**
  ******************************************************************************
  * @file    PlatformUart.h
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file provides UART operation functions.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
  ******************************************************************************
  */

#include <stdlib.h>
#include "stm32f4xx.h"
#include "RingBufferUtils.h"
#include "application.h"

/* Definition for USARTx resources **********************************************/
#define UART_RX_BUF_SIZE      2048
#define UART_DMA_MAX_BUF_SIZE 512

#define GPIO_CLK_INIT         RCC_AHB1PeriphClockCmd

#define USARTx_CLK            RCC_APB2Periph_USART1
#define USARTx_CLK_INIT       RCC_APB2PeriphClockCmd

#define USARTx_RX_PIN         GPIO_Pin_10
#define USARTx_IRQ_PIN        10
#define USARTx_RX_SOURCE      GPIO_PinSource10
#define USARTx_RX_GPIO_PORT   GPIOA
#define USARTx_RX_GPIO_CLK    RCC_AHB1Periph_GPIOA
#define USARTx_RX_AF          GPIO_AF_USART1

#define USARTx_TX_PIN         GPIO_Pin_9
#define USARTx_TX_SOURCE      GPIO_PinSource9
#define USARTx_TX_GPIO_PORT   GPIOA
#define USARTx_TX_GPIO_CLK    RCC_AHB1Periph_GPIOA
#define USARTx_TX_AF          GPIO_AF_USART1

#define USARTx_CTS_PIN        GPIO_Pin_11
#define USARTx_CTS_SOURCE     GPIO_PinSource11
#define USARTx_CTS_GPIO_PORT  GPIOA
#define USARTx_CTS_GPIO_CLK   RCC_AHB1Periph_GPIOA
#define USARTx_CTS_AF         GPIO_AF_USART1

#define USARTx_RTS_PIN        GPIO_Pin_12
#define USARTx_RTS_SOURCE     GPIO_PinSource12
#define USARTx_RTS_GPIO_PORT  GPIOA
#define USARTx_RTS_GPIO_CLK   RCC_AHB1Periph_GPIOA
#define USARTx_RTS_AF         GPIO_AF_USART1

#define USARTx_IRQn           USART1_IRQn
#define USARTx                USART1
#define USARTx_IRQHandler     USART1_IRQHandler

#define USARTx_DR_Base        ((uint32_t)USART1 + 0x04)

#define DMA_CLK_INIT             RCC_AHB1Periph_DMA2
#define UART_RX_DMA              DMA2
#define UART_RX_DMA_Stream       DMA2_Stream2
#define UART_RX_DMA_Stream_IRQn  DMA2_Stream2_IRQn
#define UART_RX_DMA_HTIF         DMA_FLAG_HTIF2
#define UART_RX_DMA_TCIF         DMA_FLAG_TCIF2

#define UART_TX_DMA              DMA2
#define UART_TX_DMA_Stream       DMA2_Stream7
#define UART_TX_DMA_Stream_IRQn  DMA2_Stream7_IRQn
#define UART_TX_DMA_TCIF         DMA_FLAG_TCIF7
#define UART_TX_DMA_IRQHandler   DMA2_Stream7_IRQHandler

#define UART_BANDRATE   115200

uint32_t rx_size = 0;
static  mico_semaphore_t tx_complete, rx_complete;

static int uart_power_save = UART_PWR_SAVE;

static  mico_semaphore_t wakeup = NULL;
static mico_thread_t uart_wakeup_thread_handler;
static void uart_wakeup_thread(void *arg);
static mico_mutex_t _uart_send_mutex = NULL;

uint8_t rx_data[UART_RX_BUF_SIZE];
ring_buffer_t rx_buffer;

static uint8_t platform_uart_receive_bytes(void* data, uint32_t size);
void _Rx_irq_handler(void *arg);

int PlatformUartInitialize(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure;

    mico_rtos_init_semaphore(&tx_complete, 1);
    mico_rtos_init_semaphore(&rx_complete, 1);
    mico_rtos_init_mutex(&_uart_send_mutex);

    mico_mcu_powersave_config(false);

    rx_buffer.buffer = rx_data;
    rx_buffer.size = UART_RX_BUF_SIZE;
    rx_buffer.head = 0;
    rx_buffer.tail = 0;

    // Jinfeng-Buglog: you should reset the GPIOA if don't use PA9,10 with USART1
    //RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_CLK_INIT(USARTx_RX_GPIO_CLK, ENABLE);
    USARTx_CLK_INIT(USARTx_CLK, ENABLE);
    RCC_AHB1PeriphClockCmd(DMA_CLK_INIT, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = UART_TX_DMA_Stream_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 8;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel                   = USARTx_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = (uint8_t) 0x6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0x7;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Configure USART pin*/
    GPIO_PinAFConfig(USARTx_TX_GPIO_PORT, USARTx_TX_SOURCE, USARTx_TX_AF);
    GPIO_PinAFConfig(USARTx_RX_GPIO_PORT, USARTx_RX_SOURCE, USARTx_RX_AF);

    /* Configure USART Tx as alternate function  */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;

    GPIO_InitStructure.GPIO_Pin = USARTx_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStructure);

    /* Configure USART Rx as alternate function  */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = USARTx_RX_PIN;
    GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStructure);

//  if(inContext->flashContentInRam.micoSystemConfig.mcuPowerSaveEnable){
//    gpio_irq_enable(USARTx_RX_GPIO_PORT, USARTx_IRQ_PIN, IRQ_TRIGGER_FALLING_EDGE, _Rx_irq_handler, 0);
//    mico_rtos_init_semaphore(&wakeup, 1);
//    context = inContext;
//    mico_rtos_create_thread(&uart_wakeup_thread_handler, MICO_APPLICATION_PRIORITY, "UART_WAKEUP", uart_wakeup_thread, 0x100, inContext );
//  }

    //port_change
    USART_DeInit(USARTx);
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1 ;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USARTx, &USART_InitStructure);

    USART_Cmd(USARTx, ENABLE);
    USART_DMACmd(USARTx, USART_DMAReq_Rx | USART_DMAReq_Tx, ENABLE);


    DMA_DeInit(UART_RX_DMA_Stream);

    DMA_InitStructure.DMA_PeripheralBaseAddr = USARTx_DR_Base;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_BufferSize = 0;

    DMA_Init(UART_RX_DMA_Stream, &DMA_InitStructure);

    platform_uart_receive_bytes(rx_buffer.buffer, rx_buffer.size);

    //mico_mcu_powersave_config(true);

    return 0;
}

void _Rx_irq_handler(void *arg)
{
    (void)arg;
    //init_clocks();
    GPIO_CLK_INIT(USARTx_RX_GPIO_CLK, ENABLE);
    USARTx_CLK_INIT(USARTx_CLK, ENABLE);
    RCC_AHB1PeriphClockCmd(DMA_CLK_INIT, ENABLE);

    gpio_irq_disable(USARTx_RX_GPIO_PORT, USARTx_IRQ_PIN);
    mico_mcu_powersave_config(false);
    mico_rtos_set_semaphore(&wakeup);
}

int PlatformUartSend(uint8_t *inSendBuf, uint32_t inBufLen)
{
    DMA_InitTypeDef  DMA_InitStructure;

    if (NULL == _uart_send_mutex)
        return 0;

    mico_rtos_lock_mutex(&_uart_send_mutex);
    mico_mcu_powersave_config(false);

    DMA_DeInit(UART_TX_DMA_Stream);
    DMA_InitStructure.DMA_PeripheralBaseAddr = USARTx_DR_Base;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;

    /****************** USART will Transmit Specific Command ******************/
    /* Prepare the DMA to transfer the transaction command (2bytes) from the
    memory to the USART */
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)inSendBuf;
    DMA_InitStructure.DMA_BufferSize = (uint16_t)inBufLen;
    DMA_Init(UART_TX_DMA_Stream, &DMA_InitStructure);

    DMA_ITConfig(UART_TX_DMA_Stream, DMA_IT_TC, ENABLE);
    /* Enable the USART DMA requests */


    /* Clear the TC bit in the SR register by writing 0 to it */
    USART_ClearFlag(USARTx, USART_FLAG_TC);

    /* Enable the DMA TX Stream, USART will start sending the command code (2bytes) */
    DMA_Cmd(UART_TX_DMA_Stream, ENABLE);

    mico_rtos_get_semaphore(&tx_complete, 500);

    while ((USARTx->SR & USART_SR_TC) == 0);

    mico_mcu_powersave_config(true);
    mico_rtos_unlock_mutex(&_uart_send_mutex);
}


int PlatformUartRecv(uint8_t *inRecvBuf, uint32_t inBufLen, uint32_t inTimeOut)
{
    while (inBufLen != 0)
    {
        uint32_t transfer_size = MIN(rx_buffer.size / 2, inBufLen);

        /* Check if ring buffer already contains the required amount of data. */
        if (transfer_size > ring_buffer_used_space(&rx_buffer))
        {
            /* Set rx_size and wait in rx_complete semaphore until data reaches rx_size or timeout occurs */
            rx_size = transfer_size;

            if (mico_rtos_get_semaphore(&rx_complete, inTimeOut) != 0)
            {
                rx_size = 0;
                return -1;
            }

            /* Reset rx_size to prevent semaphore being set while nothing waits for the data */
            rx_size = 0;
        }

        inBufLen -= transfer_size;

        // Grab data from the buffer
        do
        {
            uint8_t* available_data;
            uint32_t bytes_available;

            ring_buffer_get_data(&rx_buffer, &available_data, &bytes_available);
            bytes_available = MIN(bytes_available, transfer_size);
            memcpy(inRecvBuf, available_data, bytes_available);
            transfer_size -= bytes_available;
            inRecvBuf = ((uint8_t*) inRecvBuf + bytes_available);
            ring_buffer_consume(&rx_buffer, bytes_available);
        }
        while (transfer_size != 0);
    }

    if (inBufLen != 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}


static u8 platform_uart_receive_bytes(void* data, uint32_t size)
{
    uint32_t tmpvar; /* needed to ensure ordering of volatile accesses */

    UART_RX_DMA_Stream->CR |= DMA_SxCR_CIRC;
    // Enabled individual byte interrupts so progress can be updated
    USART_ITConfig(USARTx, USART_IT_RXNE, ENABLE);

    tmpvar = UART_RX_DMA->LISR;
    UART_RX_DMA->LIFCR      |= tmpvar;
    tmpvar = UART_RX_DMA->HISR;
    UART_RX_DMA->HIFCR      |= tmpvar;
    UART_RX_DMA_Stream->NDTR = size;
    UART_RX_DMA_Stream->M0AR = (uint32_t)data;
    UART_RX_DMA_Stream->CR  |= DMA_SxCR_EN;

    return 0;
}


void UART_TX_DMA_IRQHandler(void)
{
    if ((UART_TX_DMA->HISR & UART_TX_DMA_TCIF) != 0)
    {
        UART_TX_DMA->HIFCR |= UART_TX_DMA_TCIF;
        mico_rtos_set_semaphore(&tx_complete);
    }    /* TX DMA error */

    if ((UART_TX_DMA->HISR & UART_TX_DMA_TCIF) != 0)
    {
        /* Clear interrupt */
        UART_TX_DMA->HIFCR |= UART_TX_DMA_TCIF;
    }
}

void USARTx_IRQHandler(void)
{
    // Clear all interrupts. It's safe to do so because only RXNE interrupt is enabled
    USARTx->SR = (uint16_t)(USARTx->SR | 0xffff);

    // Update tail
    rx_buffer.tail = rx_buffer.size - UART_RX_DMA_Stream->NDTR;

    // Notify thread if sufficient data are available
    if ((rx_size > 0) && (ring_buffer_used_space(&rx_buffer) >= rx_size))
    {
        mico_rtos_set_semaphore(&rx_complete);
        rx_size = 0;
    }

    if (wakeup)
    {
        if (uart_power_save == true)
            mico_rtos_set_semaphore(&wakeup);
    }


}

static void uart_wakeup_thread(void *arg)
{
    while (1)
    {
        if (mico_rtos_get_semaphore(&wakeup, 1000) != 0)
        {
            gpio_irq_enable(USARTx_RX_GPIO_PORT, USARTx_IRQ_PIN, IRQ_TRIGGER_FALLING_EDGE, _Rx_irq_handler, 0);

            if (uart_power_save)
                mico_mcu_powersave_config(true);
        }
    }
}

size_t PlatformUartRecvedDataLen(void)
{
    return ring_buffer_used_space(&rx_buffer);
}



