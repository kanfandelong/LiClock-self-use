#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "Arduino.h"

//select your board
//#define CONFIG_ESP_LYRAT_V4_3_BOARD
#define CONFIG_ESP_AUDIO_KIT_V2_2_BOARD

#define I2C_FREQ    100000UL
#define I2C_SCL     GPIO_NUM_32
#define I2C_SDA     GPIO_NUM_33

#define I2S_BCK     GPIO_NUM_27
#define I2S_WS      GPIO_NUM_25
#define I2S_DOUT    GPIO_NUM_26

#define I2S_DIN     GPIO_NUM_35
#define I2S_MCLK    GPIO_NUM_0
#define I2S_SCLK    I2S_BCK

#define AUXIN_DETECT_GPIO  -1
#define HEADPHONE_DETECT   GPIO_NUM_39
#define PA_ENABLE_GPIO     GPIO_NUM_21

//WS2812点阵驱动引脚
#define MATRIX_LED_PIN     GPIO_NUM_18
//ADCKey Detect
#define ADC_KEY_DETECT_PIN GPIO_NUM_36
//Normal Key2
#define KEY2_DETECT_PIN    GPIO_NUM_13

enum{
    ES8311,
    ES8388
};

#endif

