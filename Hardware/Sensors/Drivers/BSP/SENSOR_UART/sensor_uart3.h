/**
 ****************************************************************************************************
 * @file        sensor_uart3.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-05
 * @brief       传感器数据UART3发送代码
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
 * V1.0 20230605
 * 第一次发布
 *
 ****************************************************************************************************
 */

#ifndef __SENSOR_UART3_H
#define __SENSOR_UART3_H

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"

/* 外部变量声明 */
extern UART_HandleTypeDef g_uart3_handle; /* UART3句柄 */

/* 函数声明 */
void sensor_uart3_init(uint32_t baudrate);
void sensor_uart3_send_data(uint8_t temperature, uint8_t humidity, float co_ppm, float dust_density);

#endif /* __SENSOR_UART3_H */