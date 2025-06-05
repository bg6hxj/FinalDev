/**
 ****************************************************************************************************
 * @file        sensor_uart.c
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

#include "./BSP/SENSOR_UART/sensor_uart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/BEEP/beep.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief       格式化并发送传感器数据到串口
 * @param       temperature: 温度值
 * @param       humidity: 湿度值
 * @param       co_ppm: 一氧化碳浓度(ppm)
 * @param       dust_density: 粉尘浓度(ug/m3)
 * @retval      无
 */
void sensor_uart_send_data(uint8_t temperature, uint8_t humidity, float co_ppm, float dust_density)
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
    HAL_UART_Transmit(&g_uart1_handle, (uint8_t*)buffer, len, 100);
}

/**
 * @brief       定期发送传感器数据
 * @param       temperature: 温度值
 * @param       humidity: 湿度值
 * @param       co_ppm: 一氧化碳浓度(ppm)
 * @param       dust_density: 粉尘浓度(ug/m3)
 * @retval      无
 */
void sensor_uart_periodic_send(uint8_t temperature, uint8_t humidity, float co_ppm, float dust_density)
{
    static uint32_t last_send_time = 0;
    uint32_t current_time;
    
    current_time = HAL_GetTick();
    
    /* 每1000ms发送一次数据 */
    if (current_time - last_send_time >= 1000)
    {
        sensor_uart_send_data(temperature, humidity, co_ppm, dust_density);
        last_send_time = current_time;
    }
}