/**
 ****************************************************************************************************
 * @file        beep.h
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

#ifndef __BEEP_H
#define __BEEP_H
#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* 引脚 定义 */

#define BEEP_GPIO_PORT                  GPIOB
#define BEEP_GPIO_PIN                   GPIO_PIN_8
#define BEEP_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)             /* PB口时钟使能 */

/* 蜂鸣器报警类型定义 */
#define BEEP_ALARM_NONE                 0   /* 无报警 */
#define BEEP_ALARM_TEMP_HIGH            1   /* 温度过高报警 */
#define BEEP_ALARM_TEMP_LOW             2   /* 温度过低报警 */
#define BEEP_ALARM_HUMI_HIGH            3   /* 湿度过高报警 */
#define BEEP_ALARM_HUMI_LOW             4   /* 湿度过低报警 */
#define BEEP_ALARM_CO_NORMAL            5   /* CO浓度一般报警 */
#define BEEP_ALARM_CO_DANGER            6   /* CO浓度危险报警 */
#define BEEP_ALARM_DUST_LOW             7   /* 粉尘浓度低报警 */
#define BEEP_ALARM_DUST_HIGH            8   /* 粉尘浓度高报警 */

/* 传感器阈值定义 */
#define TEMP_HIGH_THRESHOLD             35  /* 温度高阈值 (°C) */
#define TEMP_LOW_THRESHOLD              20  /* 温度低阈值 (°C) */
#define HUMI_HIGH_THRESHOLD             80  /* 湿度高阈值 (%) */
#define HUMI_LOW_THRESHOLD              10  /* 湿度低阈值 (%) */
#define CO_NORMAL_THRESHOLD             50  /* CO一般报警阈值 (ppm) */
#define CO_DANGER_THRESHOLD             200 /* CO危险报警阈值 (ppm) */
#define DUST_LOW_THRESHOLD              0.2 /* 粉尘低浓度阈值 (mg/m³) */
#define DUST_HIGH_THRESHOLD             0.5 /* 粉尘高浓度阈值 (mg/m³) */

/******************************************************************************************/
/* 外部接口函数*/
void beep_init(void);                                /* 初始化 */
void beep_set_freq(uint16_t freq);                  /* 设置蜂鸣器频率 */
void beep_alarm_handler(uint8_t temp, uint8_t humi, float co_ppm, float dust_density); /* 报警处理函数 */
void beep_on(void);                                 /* 开启蜂鸣器 */
void beep_off(void);                                /* 关闭蜂鸣器 */

/* 外部变量声明 */
extern uint8_t g_current_alarm;                     /* 当前报警类型 */

#endif