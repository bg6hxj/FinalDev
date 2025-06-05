#include "./BSP/MQ7/mq7.h"

static ADC_HandleTypeDef g_adc_handle;

uint8_t mq7_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    ADC_ChannelConfTypeDef adc_ch_conf;
    
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    gpio_init_struct.Pin = GPIO_PIN_1;
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;
    gpio_init_struct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);
    
    g_adc_handle.Instance = ADC1;
    g_adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_adc_handle.Init.ScanConvMode = ADC_SCAN_DISABLE;
    g_adc_handle.Init.ContinuousConvMode = DISABLE;
    g_adc_handle.Init.NbrOfConversion = 1;
    g_adc_handle.Init.DiscontinuousConvMode = DISABLE;
    g_adc_handle.Init.NbrOfDiscConversion = 0;
    g_adc_handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(&g_adc_handle);
    
    adc_ch_conf.Channel = MQ7_ADC_CHANNEL;
    adc_ch_conf.Rank = ADC_REGULAR_RANK_1;
    adc_ch_conf.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&g_adc_handle, &adc_ch_conf);
    
    HAL_ADCEx_Calibration_Start(&g_adc_handle);
    
    return 0;
}

uint16_t mq7_get_adc_value(void)
{
    HAL_ADC_Start(&g_adc_handle);
    HAL_ADC_PollForConversion(&g_adc_handle, 10);
    
    if (HAL_IS_BIT_SET(HAL_ADC_GetState(&g_adc_handle), HAL_ADC_STATE_REG_EOC))
    {
        return HAL_ADC_GetValue(&g_adc_handle);
    }
    
    return 0;
}

uint16_t mq7_get_adc_average(uint8_t times)
{
    uint32_t temp_val = 0;
    uint8_t t;
    
    for (t = 0; t < times; t++)
    {
        temp_val += mq7_get_adc_value();
        delay_ms(5);
    }
    
    return temp_val / times;
}

float mq7_get_co_ppm(void)
{
    float rs_r0, ppm;
    uint16_t adc_value;
    
    adc_value = mq7_get_adc_average(10);
    
    if (adc_value == 0) adc_value = 1;
    
    rs_r0 = (4095.0 - adc_value) / adc_value;
    
    // 修正的MQ-7一氧化碳浓度计算公式
    // 根据MQ-7数据手册，CO浓度与Rs/R0的关系更接近于ppm = 98.322 * pow(rs_r0, -1.458)
    ppm = 98.322 * pow(rs_r0, -1.458);
    
    // 限制输出范围在10-1000ppm之间，这是MQ-7的典型测量范围
    if (ppm < 10.0) ppm = 10.0;
    if (ppm > 1000.0) ppm = 1000.0;
    
    return ppm;
}