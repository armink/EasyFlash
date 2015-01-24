/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/
#define  BSP_MODULE

#include <bsp.h>
/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

/** This function will initial STM32 board**/
void rt_hw_board_init()
{
    BSP_Init();
    //add for finsh
    rt_hw_usart_init();
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
}

/**
 * RCC configuration
 */
static void RCC_Configuration(void)
{
    ErrorStatus HSEStartUpStatus;

    //使能外部晶振
    RCC_HSEConfig(RCC_HSE_ON);
    //等待外部晶振稳定
    HSEStartUpStatus = RCC_WaitForHSEStartUp();
    //如果外部晶振启动成功，则进行下一步操作
    if(HSEStartUpStatus==SUCCESS)
    {
        //设置HCLK（AHB时钟）=SYSCLK = 72MHz
        RCC_HCLKConfig(RCC_SYSCLK_Div1);

        //PCLK1(APB1) = HCLK/2,RCC_HCLK_Div2——>PCLK1=36MHz,最大36MHz
        RCC_PCLK1Config(RCC_HCLK_Div2);

        //PCLK2(APB2) = HCLK = 72MHz
        RCC_PCLK2Config(RCC_HCLK_Div1);

        //FLASH时序控制
        //推荐值：SYSCLK = 0~24MHz   Latency=0
        //        SYSCLK = 24~48MHz  Latency=1
        //        SYSCLK = 48~72MHz  Latency=2
        FLASH_SetLatency(FLASH_Latency_2);
        //开启FLASH预取指功能
        FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

        //PLL设置 SYSCLK/1 * 9 = 8*1*9 = 72MHz
        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
        //启动PLL
        RCC_PLLCmd(ENABLE);
        //等待PLL稳定
        while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

        //系统时钟SYSCLK来自PLL输出
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        //切换时钟后等待系统时钟稳定
        while(RCC_GetSYSCLKSource()!=0x08);
        /*
        //设置系统SYSCLK时钟为HSE输入
        RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
        //等待时钟切换成功
        while(RCC_GetSYSCLKSource() != 0x04);
        */
    }

    //下面是给各模块开启时钟
    //启动GPIO
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | \
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | \
                           RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOG,
                           ENABLE);

    //启动AFIO
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    //启动USART1时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    //启动USART2时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    //启动DMA时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    /* Enable ADC1 and GPIOC clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    /* Enable WWDG clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);
}

/**
 * NVIC Configuration
 */
static void NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

#if  defined(VECT_TAB_RAM)
    // Set the Vector Table base location at 0x20000000
    NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x00);
#elif  defined(VECT_TAB_FLASH)
    // Set the Vector Table base location at 0x08000000
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x00);
#elif defined(VECT_TAB_USER)
    // Set the Vector Table base location by user
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, USER_VECTOR_TABLE);
#endif

    //设置NVIC优先级分组为Group2：0-3抢占式优先级，0-3的响应式优先级
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    //窗口看门狗中断配置
    NVIC_InitStructure.NVIC_IRQChannel = WWDG_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * GPIO Configuration
 */
static void GPIO_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    GPIO_Init(GPIOF, &GPIO_InitStructure);
    GPIO_Init(GPIOG, &GPIO_InitStructure);



    /******************系统运行LED指示灯配置*******************/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
 * WWDG Configuration
 */
static void WWDG_Configuration(void)
{
    //在PCLK1驱动看门狗计时之前,首先要经过既定的4096分频(详情查看STM32技术参考手册),再经过Prescaler = 8分频
    /* WWDG clock counter = (PCLK1/4096)/8 = 1098.6 Hz (~0.910 ms)  */
    WWDG_SetPrescaler(WWDG_Prescaler_8);

    /* Set Window value to 127 */
    //范围：1ms-58ms
    WWDG_SetWindowValue(0x7F);

    /* Enable WWDG and set counter value to 127, WWDG timeout = ~4 ms * (0x7F - 0x3F) = 58.24 ms */
    WWDG_Enable(127);

    /* Clear EWI flag */
    WWDG_ClearFlag();

    /* Enable EW interrupt */
    WWDG_EnableIT();
}

/**
 * IWDG_Configuration
 */
static void IWDG_Configuration(void) 
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);//使能对IWDG->PR和IWDG->RLR的写
    IWDG_SetPrescaler(IWDG_Prescaler_64);//64分频
    IWDG_SetReload(1875);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/**
 * feed IWDG dog
 */
void IWDG_Feed(void)
{
    IWDG_ReloadCounter();//reload
}


/**
 * SysTick Configuration
 */
void  SysTick_Configuration(void)
{
    RCC_ClocksTypeDef  rcc_clocks;
    rt_uint32_t         cnts;

    RCC_GetClocksFreq(&rcc_clocks);

    cnts = (rt_uint32_t)rcc_clocks.HCLK_Frequency / RT_TICK_PER_SECOND;

    SysTick_Config(cnts);
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
}
/**
 * This is the timer interrupt service routine.
 *
 */
void rt_hw_timer_handler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
}

void assert_failed(u8* file, u32 line)
{
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* Infinite loop */
    rt_kprintf("assert failed at %s:%d \n", file, line);
    while (1) {
    }
}
/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               BSP_Init()
*
* Description : Initialize the Board Support Package (BSP).
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) This function SHOULD be called before any other BSP function is called.
*********************************************************************************************************
*/

void  BSP_Init (void)
{
    RCC_Configuration();
    NVIC_Configuration();
    SysTick_Configuration();
    GPIO_Configuration();
//    TODO  Now temporary comment this code, the official version will open his
//     IWDG_Configuration();
}

