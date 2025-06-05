/**
 ****************************************************************************************************
 * @file        gp2y1014au.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-01
 * @brief       GP2Y1014AU粉尘传感器驱动代码
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
 * V1.0 20230601
 * 第一次发布
 *
 ****************************************************************************************************
 */

 #ifndef __GP2Y1014AU_H
 #define __GP2Y1014AU_H
 
 #include "./SYSTEM/sys/sys.h"
 #include "stm32f1xx_hal.h"
 #include <math.h>
 
 /* GP2Y1014AU引脚定义 */
 #define GP2Y1014AU_LED_GPIO_PORT        GPIOB
 #define GP2Y1014AU_LED_GPIO_PIN         GPIO_PIN_5
 #define GP2Y1014AU_LED_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  /* PB口时钟使能 */
 
 /* GP2Y1014AU ADC通道定义 */
 #define GP2Y1014AU_ADC_CHANNEL          ADC_CHANNEL_0  /* PA0对应ADC通道0 */
 
 /* 函数声明 */
 uint8_t gp2y1014au_init(void);                      /* 初始化GP2Y1014AU */
 uint16_t gp2y1014au_get_adc_value(void);            /* 获取ADC值 */
 uint16_t gp2y1014au_get_adc_average(uint8_t times); /* 获取多次ADC平均值 */
 float gp2y1014au_get_dust_density(void);            /* 获取粉尘浓度(ug/m3) */
 
 #endif