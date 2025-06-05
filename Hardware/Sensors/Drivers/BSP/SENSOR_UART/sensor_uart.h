/**
 ****************************************************************************************************
 * @file        sensor_uart.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-05
 * @brief       传感器数据UART发送代码
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

#ifndef __SENSOR_UART_H
#define __SENSOR_UART_H

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"

/* 函数声明 */
void sensor_uart_send_data(uint8_t temperature, uint8_t humidity, float co_ppm, float dust_density);
void sensor_uart_periodic_send(uint8_t temperature, uint8_t humidity, float co_ppm, float dust_density);

#endif /* __SENSOR_UART_H */