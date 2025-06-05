/**
 ****************************************************************************************************
 * @file        gp2y1014au.c
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

 #include "./BSP/GP2Y1014AU/gp2y1014au.h"
 #include "./SYSTEM/delay/delay.h"
 
 static ADC_HandleTypeDef g_adc_handle;
 
 /**
  * @brief       初始化GP2Y1014AU
  * @param       无
  * @retval      0: 成功
  */
 uint8_t gp2y1014au_init(void)
 {
     GPIO_InitTypeDef gpio_init_struct;
     ADC_ChannelConfTypeDef adc_ch_conf;
     
     /* 使能时钟 */
     __HAL_RCC_ADC1_CLK_ENABLE();
     __HAL_RCC_GPIOA_CLK_ENABLE();
     GP2Y1014AU_LED_GPIO_CLK_ENABLE();
     
     /* 配置ADC引脚 */
     gpio_init_struct.Pin = GPIO_PIN_0;  /* PA0 */
     gpio_init_struct.Mode = GPIO_MODE_ANALOG;
     gpio_init_struct.Pull = GPIO_NOPULL;
     HAL_GPIO_Init(GPIOA, &gpio_init_struct);
     
     /* 配置LED引脚 */
     gpio_init_struct.Pin = GP2Y1014AU_LED_GPIO_PIN;
     gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
     gpio_init_struct.Pull = GPIO_PULLUP;
     gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
     HAL_GPIO_Init(GP2Y1014AU_LED_GPIO_PORT, &gpio_init_struct);
     
     /* 默认关闭LED */
     HAL_GPIO_WritePin(GP2Y1014AU_LED_GPIO_PORT, GP2Y1014AU_LED_GPIO_PIN, GPIO_PIN_SET);
     
     /* 配置ADC */
     g_adc_handle.Instance = ADC1;
     g_adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
     g_adc_handle.Init.ScanConvMode = ADC_SCAN_DISABLE;
     g_adc_handle.Init.ContinuousConvMode = DISABLE;
     g_adc_handle.Init.NbrOfConversion = 1;
     g_adc_handle.Init.DiscontinuousConvMode = DISABLE;
     g_adc_handle.Init.NbrOfDiscConversion = 0;
     g_adc_handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
     HAL_ADC_Init(&g_adc_handle);
     
     adc_ch_conf.Channel = GP2Y1014AU_ADC_CHANNEL;
     adc_ch_conf.Rank = ADC_REGULAR_RANK_1;
     adc_ch_conf.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
     HAL_ADC_ConfigChannel(&g_adc_handle, &adc_ch_conf);
     
     HAL_ADCEx_Calibration_Start(&g_adc_handle);
     
     return 0;
 }
 
 /**
  * @brief       获取GP2Y1014AU的ADC值
  * @param       无
  * @retval      ADC值
  */
 uint16_t gp2y1014au_get_adc_value(void)
 {
     uint16_t adc_value;
     
     /* GP2Y1014AU测量时序：
      * 1. LED脉冲：先拉低LED引脚0.32ms
      * 2. 延时0.28ms后开始ADC采样
      * 3. 采样完成后，恢复LED引脚为高电平
      */
     
     /* 拉低LED引脚 */
     HAL_GPIO_WritePin(GP2Y1014AU_LED_GPIO_PORT, GP2Y1014AU_LED_GPIO_PIN, GPIO_PIN_RESET);
     delay_us(320);  /* 延时0.32ms */
     
     /* 延时0.28ms后开始ADC采样 */
     delay_us(280);
     
     /* 启动ADC转换 */
     HAL_ADC_Start(&g_adc_handle);
     HAL_ADC_PollForConversion(&g_adc_handle, 10);
     
     /* 获取ADC值 */
     if (HAL_IS_BIT_SET(HAL_ADC_GetState(&g_adc_handle), HAL_ADC_STATE_REG_EOC))
     {
         adc_value = HAL_ADC_GetValue(&g_adc_handle);
     }
     else
     {
         adc_value = 0;
     }
     
     /* 恢复LED引脚为高电平 */
     HAL_GPIO_WritePin(GP2Y1014AU_LED_GPIO_PORT, GP2Y1014AU_LED_GPIO_PIN, GPIO_PIN_SET);
     
     /* 等待一段时间再进行下一次测量 */
     delay_ms(10);
     
     return adc_value;
 }
 
 /**
  * @brief       获取多次ADC采样的平均值
  * @param       times: 采样次数
  * @retval      ADC平均值
  */
 uint16_t gp2y1014au_get_adc_average(uint8_t times)
 {
     uint32_t temp_val = 0;
     uint8_t t;
     
     for (t = 0; t < times; t++)
     {
         temp_val += gp2y1014au_get_adc_value();
     }
     
     return temp_val / times;
 }
 
 /**
  * @brief       获取粉尘浓度(ug/m3)
  * @param       无
  * @retval      粉尘浓度值(ug/m3)
  */
 float gp2y1014au_get_dust_density(void)
 {
     float voltage, dust_density;
     uint16_t adc_value;
     
     /* 获取多次采样的平均值 */
     adc_value = gp2y1014au_get_adc_average(5);
     
     /* 将ADC值转换为电压值(mV) */
     voltage = (float)adc_value * (3300.0 / 4096.0);  /* 3.3V参考电压，12位ADC */
     
     /* 根据GP2Y1014AU数据手册，将电压值转换为粉尘浓度(ug/m3) */
     /* 公式: ug/m3 = (voltage - 600) / 10.0 */
     /* 注意: 600mV是无尘时的电压输出(偏置电压) */
     if (voltage > 600.0)
     {
         dust_density = (voltage - 600.0) / 10.0;
     }
     else
     {
         dust_density = 0.0;
     }
     
     /* 限制输出范围 */
     if (dust_density < 0.0) dust_density = 0.0;
     if (dust_density > 500.0) dust_density = 500.0;  /* GP2Y1014AU的测量范围通常为0-500ug/m3 */
     
     return dust_density;
 }