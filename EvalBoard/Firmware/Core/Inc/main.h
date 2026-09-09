#pragma once

#include "stm32g0xx_hal.h"

#define SENSOR_ENA_Pin GPIO_PIN_12
#define SENSOR_ENA_GPIO_Port GPIOC
#define SLEEP_H1_Pin GPIO_PIN_2
#define SLEEP_H1_GPIO_Port GPIOC
#define SLEEP_H2_Pin GPIO_PIN_3
#define SLEEP_H2_GPIO_Port GPIOC
#define IN2_H1_Pin GPIO_PIN_2
#define IN2_H1_GPIO_Port GPIOA
#define IN1_H1_Pin GPIO_PIN_3
#define IN1_H1_GPIO_Port GPIOA
#define IN2_H2_Pin GPIO_PIN_6
#define IN2_H2_GPIO_Port GPIOA
#define IN1_H2_Pin GPIO_PIN_7
#define IN1_H2_GPIO_Port GPIOA
#define PMODE_Pin GPIO_PIN_4
#define PMODE_GPIO_Port GPIOC
#define IN2_H3_Pin GPIO_PIN_1
#define IN2_H3_GPIO_Port GPIOB
#define IN1_H3_Pin GPIO_PIN_2
#define IN1_H3_GPIO_Port GPIOB
#define IN2_H4_Pin GPIO_PIN_12
#define IN2_H4_GPIO_Port GPIOB
#define IN1_H4_Pin GPIO_PIN_13
#define IN1_H4_GPIO_Port GPIOB
#define SLEEP_H4_Pin GPIO_PIN_14
#define SLEEP_H4_GPIO_Port GPIOB
#define SLEEP_H3_Pin GPIO_PIN_15
#define SLEEP_H3_GPIO_Port GPIOB
#define VEL_SENSOR_2_Pin GPIO_PIN_6
#define VEL_SENSOR_2_GPIO_Port GPIOC
#define VEL_SENSOR_1_Pin GPIO_PIN_7
#define VEL_SENSOR_1_GPIO_Port GPIOC
#define DAC_DIN_Pin GPIO_PIN_4
#define DAC_DIN_GPIO_Port GPIOD
#define DAC_SYNC_Pin GPIO_PIN_6
#define DAC_SYNC_GPIO_Port GPIOD
#define DAC_SCLK_Pin GPIO_PIN_3
#define DAC_SCLK_GPIO_Port GPIOB
#define STATUS_LED_1_Pin GPIO_PIN_5
#define STATUS_LED_1_GPIO_Port GPIOB

void SystemClock_Config(void);
void Error_Handler(void);
void EvalBoard_DisablePowerOutputs(void);
