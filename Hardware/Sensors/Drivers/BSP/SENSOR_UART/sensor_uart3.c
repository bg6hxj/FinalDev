/**
 ****************************************************************************************************
 * @file        sensor_uart3.c
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

#include "./BSP/SENSOR_UART/sensor_uart3.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/BEEP/beep.h"
#include <stdio.h>
#include <string.h>

/* UART3句柄 */
UART_HandleTypeDef g_uart3_handle;

/**
 * @brief       初始化UART3
 * @param       baudrate: 波特率
 * @retval      无
 */
void sensor_uart3_init(uint32_t baudrate)
{
    /* GPIO端口设置 */
    GPIO_InitTypeDef gpio_init_struct;
    
    /* 使能USART3和对应GPIO的时钟 */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    /* 配置USART3 TX引脚(PB10) */
    gpio_init_struct.Pin = GPIO_PIN_10;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);
    
    /* 配置USART3 RX引脚(PB11) */
    gpio_init_struct.Pin = GPIO_PIN_11;
    gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);
    
    /* USART3配置 */
    g_uart3_handle.Instance = USART3;
    g_uart3_handle.Init.BaudRate = baudrate;
    g_uart3_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart3_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart3_handle.Init.Parity = UART_PARITY_NONE;
    g_uart3_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart3_handle.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&g_uart3_handle);
}

/**
 * @brief       通过UART3发送传感器数据
 * @param       temperature: 温度值
 * @param       humidity: 湿度值
 * @param       co_ppm: 一氧化碳浓度(ppm)
 * @param       dust_density: 粉尘浓度(ug/m3)
 * @retval      无
 */
void sensor_uart3_send_data(uint8_t temperature, uint8_t humidity, float co_ppm, float dust_density)
{
    char buffer[150];
    int len;
    char alarm_str[20] = "None";
    
    /* 根据当前报警类型设置报警字符串 */
    switch(g_current_alarm)
    {
        case BEEP_ALARM_NONE:
            strcpy(alarm_str, "None");
            break;
        case BEEP_ALARM_TEMP_HIGH:
            strcpy(alarm_str, "Temp High");
            break;
        case BEEP_ALARM_TEMP_LOW:
            strcpy(alarm_str, "Temp Low");
            break;
        case BEEP_ALARM_HUMI_HIGH:
            strcpy(alarm_str, "Humi High");
            break;
        case BEEP_ALARM_HUMI_LOW:
            strcpy(alarm_str, "Humi Low");
            break;
        case BEEP_ALARM_CO_NORMAL:
            strcpy(alarm_str, "CO Normal");
            break;
        case BEEP_ALARM_CO_DANGER:
            strcpy(alarm_str, "CO Danger");
            break;
        case BEEP_ALARM_DUST_LOW:
            strcpy(alarm_str, "Dust Low");
            break;
        case BEEP_ALARM_DUST_HIGH:
            strcpy(alarm_str, "Dust High");
            break;
        default:
            strcpy(alarm_str, "Unknown");
            break;
    }
    
    /* 格式化传感器数据为字符串，添加报警信息 */
    len = sprintf(buffer, "T:%d,H:%d,CO:%.1f,DUST:%.1f,ALARM:%s\r\n", 
                 temperature, humidity, co_ppm, dust_density, alarm_str);
    
    /* 通过HAL库函数发送数据 */
    HAL_UART_Transmit(&g_uart3_handle, (uint8_t*)buffer, len, 100);
}