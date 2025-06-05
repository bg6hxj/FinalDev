#ifndef __MQ7_H
#define __MQ7_H

#include "./SYSTEM/sys/sys.h"
#include "stm32f1xx_hal.h"
#include <math.h>

#define MQ7_ADC_CHANNEL    ADC_CHANNEL_1

uint8_t mq7_init(void);
uint16_t mq7_get_adc_value(void);
uint16_t mq7_get_adc_average(uint8_t times);
float mq7_get_co_ppm(void);

#endif