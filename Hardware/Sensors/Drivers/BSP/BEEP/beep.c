/**
 ****************************************************************************************************
 * @file        beep.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-10
 * @brief       蜂鸣器 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20230610
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./BSP/BEEP/beep.h"
#include "./SYSTEM/delay/delay.h"

/* 定时器句柄 */
static TIM_HandleTypeDef g_tim_handle;
/* 定时器通道句柄 */
static TIM_OC_InitTypeDef g_tim_oc_handle;
/* 当前报警类型 */
uint8_t g_current_alarm = BEEP_ALARM_NONE;

/**
 * @brief       初始化蜂鸣器相关IO口, 并使能时钟
 * @param       无
 * @retval      无
 */
void beep_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    
    /* 使能时钟 */
    BEEP_GPIO_CLK_ENABLE();                                /* 蜂鸣器GPIO时钟使能 */
    __HAL_RCC_TIM4_CLK_ENABLE();                          /* 定时器4时钟使能 */
    
    /* 初始化蜂鸣器引脚 */
    gpio_init_struct.Pin = BEEP_GPIO_PIN;                  /* 蜂鸣器引脚 */
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;               /* 复用推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                   /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;         /* 高速 */
    HAL_GPIO_Init(BEEP_GPIO_PORT, &gpio_init_struct);      /* 初始化蜂鸣器引脚 */
    
    /* 初始化定时器 */
    g_tim_handle.Instance = TIM4;                          /* 使用定时器4 */
    g_tim_handle.Init.Prescaler = 71;                      /* 72MHz的时钟，PSC=71，计数频率=1MHz */
    g_tim_handle.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 向上计数模式 */
    g_tim_handle.Init.Period = 999;                        /* 自动重装载值，决定PWM频率，1MHz/(999+1)=1KHz */
    g_tim_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; /* 时钟不分频 */
    g_tim_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&g_tim_handle);
    
    /* 配置定时器时钟源 */
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&g_tim_handle, &sClockSourceConfig);
    
    /* 初始化PWM */
    HAL_TIM_PWM_Init(&g_tim_handle);
    
    /* 配置主输出 */
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&g_tim_handle, &sMasterConfig);
    
    /* 配置PWM通道 */
    g_tim_oc_handle.OCMode = TIM_OCMODE_PWM1;              /* PWM模式1 */
    g_tim_oc_handle.Pulse = 499;                           /* 占空比50% */
    g_tim_oc_handle.OCPolarity = TIM_OCPOLARITY_HIGH;      /* 输出极性高 */
    g_tim_oc_handle.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&g_tim_handle, &g_tim_oc_handle, TIM_CHANNEL_3); /* 通道3对应PB8 */
    
    /* 关闭蜂鸣器 */
    beep_off();
}

/**
 * @brief       设置蜂鸣器频率
 * @param       freq: 频率值(Hz)
 * @retval      无
 */
void beep_set_freq(uint16_t freq)
{
    uint16_t arr;
    
    if (freq < 100) freq = 100;       /* 限制最小频率 */
    if (freq > 10000) freq = 10000;    /* 限制最大频率 */
    
    arr = 1000000 / freq - 1;         /* 计算自动重装载值 */
    
    __HAL_TIM_SET_AUTORELOAD(&g_tim_handle, arr);  /* 设置ARR */
    __HAL_TIM_SET_COMPARE(&g_tim_handle, TIM_CHANNEL_3, (arr + 1) / 2); /* 设置CCR，保持50%占空比 */
}

/**
 * @brief       开启蜂鸣器
 * @param       无
 * @retval      无
 */
void beep_on(void)
{
    HAL_TIM_PWM_Start(&g_tim_handle, TIM_CHANNEL_3);  /* 开启PWM输出 */
}

/**
 * @brief       关闭蜂鸣器
 * @param       无
 * @retval      无
 */
void beep_off(void)
{
    HAL_TIM_PWM_Stop(&g_tim_handle, TIM_CHANNEL_3);   /* 停止PWM输出 */
}

/**
 * @brief       报警处理函数
 * @param       temp: 温度值(°C)
 * @param       humi: 湿度值(%)
 * @param       co_ppm: 一氧化碳浓度(ppm)
 * @param       dust_density: 粉尘浓度(mg/m³)
 * @retval      无
 */
void beep_alarm_handler(uint8_t temp, uint8_t humi, float co_ppm, float dust_density)
{
    uint8_t alarm_type = BEEP_ALARM_NONE;
    
    /* 检查各传感器数据是否超过阈值，按优先级顺序检查 */
    
    /* 1. 检查一氧化碳浓度 - 最高优先级 */
    if (co_ppm >= CO_DANGER_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_CO_DANGER;
    }
    else if (co_ppm >= CO_NORMAL_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_CO_NORMAL;
    }
    /* 2. 检查粉尘浓度 */
    else if (dust_density >= DUST_HIGH_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_DUST_HIGH;
    }
    else if (dust_density >= DUST_LOW_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_DUST_LOW;
    }
    /* 3. 检查温度 */
    else if (temp >= TEMP_HIGH_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_TEMP_HIGH;
    }
    else if (temp <= TEMP_LOW_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_TEMP_LOW;
    }
    /* 4. 检查湿度 */
    else if (humi >= HUMI_HIGH_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_HUMI_HIGH;
    }
    else if (humi <= HUMI_LOW_THRESHOLD)
    {
        alarm_type = BEEP_ALARM_HUMI_LOW;
    }
    
    /* 如果报警类型发生变化，更新蜂鸣器状态 */
    if (alarm_type != g_current_alarm)
    {
        g_current_alarm = alarm_type;
        
        /* 根据报警类型设置不同的蜂鸣器频率 */
        switch (alarm_type)
        {
            case BEEP_ALARM_NONE:
                beep_off();
                break;
                
            case BEEP_ALARM_TEMP_HIGH:
                beep_set_freq(2000);  /* 温度过高报警：2000Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_TEMP_LOW:
                beep_set_freq(1800);  /* 温度过低报警：1800Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_HUMI_HIGH:
                beep_set_freq(1600);  /* 湿度过高报警：1600Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_HUMI_LOW:
                beep_set_freq(1400);  /* 湿度过低报警：1400Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_CO_NORMAL:
                beep_set_freq(2500);  /* CO一般报警：2500Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_CO_DANGER:
                beep_set_freq(3000);  /* CO危险报警：3000Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_DUST_LOW:
                beep_set_freq(2200);  /* 粉尘低浓度报警：2200Hz */
                beep_on();
                break;
                
            case BEEP_ALARM_DUST_HIGH:
                beep_set_freq(2700);  /* 粉尘高浓度报警：2700Hz */
                beep_on();
                break;
                
            default:
                beep_off();
                break;
        }
    }
}