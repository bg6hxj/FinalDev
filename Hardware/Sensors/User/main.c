#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./USMART/usmart.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/DHT11/dht11.h"
#include "./BSP/MQ7/mq7.h"
#include "./BSP/GP2Y1014AU/gp2y1014au.h"
#include "./BSP/SENSOR_UART/sensor_uart.h"
#include "./BSP/SENSOR_UART/sensor_uart3.h"
#include "./BSP/BEEP/beep.h"


int main(void)
{
    uint8_t t = 0;
    uint8_t temperature;
    uint8_t humidity;
    float co_ppm;
		float dust_density;  // 用于存储粉尘浓度
    uint16_t co_int, co_dec;  // 用于存储CO浓度的整数和小数部分
    uint8_t dht11_retry = 0;  // DHT11初始化重试次数
		uint16_t dust_int, dust_dec;  // 用于存储粉尘浓度的整数和小数部分

    HAL_Init();
    sys_stm32_clock_init(RCC_PLL_MUL9);
    delay_init(72);
    usart_init(115200);
    led_init();
    lcd_init();
    mq7_init();
		gp2y1014au_init();
    beep_init();  /* 初始化蜂鸣器 */
    
    /* 初始化UART3，用于与ESP32通信 */
    sensor_uart3_init(115200);

    lcd_show_string(30, 50, 200, 16, 16, "STM32", RED);
    lcd_show_string(30, 70, 200, 16, 16, "Environment Monitor", RED);
    
    // 增加DHT11上电稳定延时
    delay_ms(1500);  // DHT11上电需要至少1秒稳定时间
    
    while (dht11_init())
    {
        lcd_show_string(30, 90, 200, 16, 16, "Sensor Error", RED);
        delay_ms(200);
        lcd_fill(30, 90, 239, 130 + 16, WHITE);
        delay_ms(200);
        
        // 增加DHT11复位操作
        dht11_retry++;
        if(dht11_retry >= 3)  // 最多重试3次
        {
            // 执行DHT11硬件复位
            GPIO_InitTypeDef gpio_init_struct;
            
            __HAL_RCC_GPIOG_CLK_ENABLE();  // 使能DHT11所在端口时钟
            
            // 先将DHT11数据线设为输出，输出低电平
            gpio_init_struct.Pin = GPIO_PIN_11;
            gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
            gpio_init_struct.Pull = GPIO_PULLUP;
            gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(GPIOG, &gpio_init_struct);
            
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_RESET);  // 拉低数据线
            delay_ms(20);  // 保持至少18ms
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_SET);    // 拉高数据线
            delay_us(30);  // 延时30us
            
            dht11_retry = 0;  // 重置重试计数
            delay_ms(1000);   // 等待DHT11稳定
        }
    }

    lcd_show_string(30, 90, 200, 16, 16, "Sensor OK", RED);
    lcd_show_string(30, 110, 200, 16, 16, "Temp:  C", BLUE);
    lcd_show_string(30, 130, 200, 16, 16, "Humi:  %", BLUE);
    lcd_show_string(30, 150, 200, 16, 16, "CO:          ppm", BLUE);
		lcd_show_string(30, 170, 200, 16, 16, "Dust:        ug/m3", BLUE);
    lcd_show_string(30, 190, 200, 16, 16, "Alarm: None", BLUE);

    /* 用于计时，每5秒通过USART3发送一次数据到ESP32 */
    uint32_t last_uart3_send_time = 0;
    
    while (1)
    {
        if (t % 10 == 0)
        {
            dht11_read_data(&temperature, &humidity);
            co_ppm = mq7_get_co_ppm();                            /* 读取CO浓度 */
            dust_density = gp2y1014au_get_dust_density();          /* 读取粉尘浓度 */
            
            // 将浮点数分解为整数和小数部分
            co_int = (uint16_t)co_ppm;                            // 整数部分
            co_dec = (uint16_t)((co_ppm - co_int) * 10);          // 小数部分(取一位小数)
					
            dust_int = (uint16_t)dust_density;                    // 整数部分
            dust_dec = (uint16_t)((dust_density - dust_int) * 10); // 小数部分(取一位小数)
            
            lcd_show_num(30 + 40, 110, temperature, 2, 16, BLUE);
            lcd_show_num(30 + 40, 130, humidity, 2, 16, BLUE);
            
            // 分别显示整数和小数部分，并添加小数点
            lcd_show_num(30 + 40, 150, co_int, 3, 16, BLUE);      // 显示整数部分，3位数字
            lcd_show_char(30 + 40 + 24, 150, '.', 16, 0, BLUE);   // 显示小数点
            lcd_show_num(30 + 40 + 32, 150, co_dec, 1, 16, BLUE); // 显示小数部分，1位数字
					
            lcd_show_num(30 + 40, 170, dust_int, 3, 16, BLUE);    // 显示整数部分，3位数字
            lcd_show_char(30 + 40 + 24, 170, '.', 16, 0, BLUE);   // 显示小数点
            lcd_show_num(30 + 40 + 32, 170, dust_dec, 1, 16, BLUE); // 显示小数部分，1位数字
            
            // 处理报警逻辑
            beep_alarm_handler(temperature, humidity, co_ppm, dust_density);
            
            // 显示当前报警状态
            switch(g_current_alarm) // 需要在beep.c中将g_current_alarm声明为extern
            {
                case BEEP_ALARM_NONE:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "None      ", BLUE);
                    break;
                case BEEP_ALARM_TEMP_HIGH:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Temp High ", RED);
                    break;
                case BEEP_ALARM_TEMP_LOW:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Temp Low  ", RED);
                    break;
                case BEEP_ALARM_HUMI_HIGH:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Humi High ", RED);
                    break;
                case BEEP_ALARM_HUMI_LOW:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Humi Low  ", RED);
                    break;
                case BEEP_ALARM_CO_NORMAL:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "CO Normal ", RED);
                    break;
                case BEEP_ALARM_CO_DANGER:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "CO Danger!", RED);
                    break;
                case BEEP_ALARM_DUST_LOW:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Dust Low  ", RED);
                    break;
                case BEEP_ALARM_DUST_HIGH:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Dust High!", RED);
                    break;
                default:
                    lcd_show_string(30 + 56, 190, 144, 16, 16, "Unknown   ", BLUE);
                    break;
            }
            
            // 通过串口发送传感器数据到电脑
            sensor_uart_send_data(temperature, humidity, co_ppm, dust_density);
            
            // 检查是否需要通过USART3发送数据到ESP32（每5秒发送一次）
            uint32_t current_time = HAL_GetTick();
            if (current_time - last_uart3_send_time >= 5000) // 5000ms = 5秒
            {
                sensor_uart3_send_data(temperature, humidity, co_ppm, dust_density);
                last_uart3_send_time = current_time;
            }
        }

        delay_ms(10);
        t++;

        if (t == 20)
        {
            t = 0;
            LED0_TOGGLE(); /* LED0??? */
        }
    }
}
