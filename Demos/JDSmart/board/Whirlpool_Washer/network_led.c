#include "board.h"

static int g_rfled_mode = 0;
static int g_rfled_mode_set = 0;


void base_timer_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_DeInit(TIM2);
    TIM_TimeBaseStructure.TIM_Period = 1000;
    TIM_TimeBaseStructure.TIM_Prescaler = 2999;

    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ARRPreloadConfig(TIM2, ENABLE);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    TIM_Cmd(TIM2, ENABLE);
}


void RFLED_Set_Mode(int mode)
{
    g_rfled_mode_set = 1;
    g_rfled_mode = mode;
}

void RFLED_Init()
{
    GPIO_InitTypeDef   GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void _led_on(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_1);
}

static void _led_off(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_1);
}

static void _led_toggle(void)
{
    GPIO_ToggleBits(GPIOB, GPIO_Pin_1);
}

static void init_mode_triggered(void)
{
    static int step = 0, light_on_keep;

    if (1 == g_rfled_mode_set)
    {
        g_rfled_mode_set = 0;
        step = 1;
    }

    switch (step)
    {
    case 1:
        step = 2;
        light_on_keep = 10;
        _led_on();
        break;

    case 2:
        if (0 == light_on_keep--)
        {
            step = 0;
            _led_on();
        }

        break;

    default:
        break;
    }
}

static void on_mode_triggered(void)
{
    static int step = 0;

    if (1 == g_rfled_mode_set)
    {
        g_rfled_mode_set = 0;
        step = 1;
    }

    switch (step)
    {
    case 1:
        step = 0;
        _led_on();
        break;

    default:
        break;
    }
}

static void off_mode_triggered(void)
{
    static int step = 0;

    if (1 == g_rfled_mode_set)
    {
        g_rfled_mode_set = 0;
        step = 1;
    }

    switch (step)
    {
    case 1:
        step = 0;
        _led_off();
        break;

    default:
        break;
    }
}

static void easylink_mode_triggered(void)
{
    static int counter = 2;

    if (0 == counter--)
    {
        counter = 2;
        _led_toggle();
    }
}

static void connect2router_mode_triggered(void)
{
    static int counter = 6;

    if (0 == counter--)
    {
        counter = 6;
        _led_toggle();
    }
}

static void connect2cloud_mode_triggered(void)
{
    static int counter = 9;

    if (0 == counter--)
    {
        counter = 9;
        _led_toggle();
    }
}

static void netdata_mode_triggered(void)
{
    static int step = 0, light_on_keep;

    if (1 == g_rfled_mode_set)
    {
        g_rfled_mode_set = 0;
        step = 1;
    }

    switch (step)
    {
    case 1:
        step = 2;
        light_on_keep = 5;
        _led_on();
        break;

    case 2:
        if (0 == light_on_keep--)
        {
            step = 0;
            _led_off();
        }

        break;

    default:
        break;
    }
}

void RFLED_Interrupt_Trigger(void)
{
    switch (g_rfled_mode)
    {
    case RFLED_MODE_INIT:
        init_mode_triggered();
        break;

    case RFLED_MODE_ON:
        on_mode_triggered();
        break;

    case RFLED_MODE_OFF:
        off_mode_triggered();
        break;

    case RFLED_MODE_QUICK:
        easylink_mode_triggered();
        break;

    case RFLED_MODE_NORMAL:
        connect2router_mode_triggered();
        break;

    case RFLED_MODE_SLOW:
        connect2cloud_mode_triggered();
        break;

    case RFLED_MODE_BLINK_ONCE:
        netdata_mode_triggered();
        break;

    default:
        break;
    }
}



void network_led_init(void)
{
    base_timer_init();

    RFLED_Init();
}

void TIM2_IRQHandler(void) //Led ctrl
{
    TIM_ClearITPendingBit(TIM2 , TIM_IT_Update);
    RFLED_Interrupt_Trigger();
}

