/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           main.h
 * @brief          Board GPIO assignments and fatal-error interface.
 * @note           Pin aliases are generated from DDA_V2.ioc and use STM32 HAL
 *                 pin masks and GPIO port instances.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SENSOR_ENA_Pin GPIO_PIN_12
#define SENSOR_ENA_GPIO_Port GPIOC
#define SENSOR_1_Pin GPIO_PIN_13
#define SENSOR_1_GPIO_Port GPIOC
#define SENSOR_2_Pin GPIO_PIN_14
#define SENSOR_2_GPIO_Port GPIOC
#define SLEEP_H1_Pin GPIO_PIN_2
#define SLEEP_H1_GPIO_Port GPIOC
#define SLEEP_H2_Pin GPIO_PIN_3
#define SLEEP_H2_GPIO_Port GPIOC
#define CURRENT_H1_Pin GPIO_PIN_0
#define CURRENT_H1_GPIO_Port GPIOA
#define FAULT_H1_Pin GPIO_PIN_1
#define FAULT_H1_GPIO_Port GPIOA
#define IN2_H1_Pin GPIO_PIN_2
#define IN2_H1_GPIO_Port GPIOA
#define IN1_H1_Pin GPIO_PIN_3
#define IN1_H1_GPIO_Port GPIOA
#define CURRENT_H2_Pin GPIO_PIN_4
#define CURRENT_H2_GPIO_Port GPIOA
#define FAULT_H2_Pin GPIO_PIN_5
#define FAULT_H2_GPIO_Port GPIOA
#define IN2_H2_Pin GPIO_PIN_6
#define IN2_H2_GPIO_Port GPIOA
#define IN1_H2_Pin GPIO_PIN_7
#define IN1_H2_GPIO_Port GPIOA
#define PMODE_Pin GPIO_PIN_4
#define PMODE_GPIO_Port GPIOC
#define CURRENT_H3_Pin GPIO_PIN_5
#define CURRENT_H3_GPIO_Port GPIOC
#define FAULT_H3_Pin GPIO_PIN_0
#define FAULT_H3_GPIO_Port GPIOB
#define IN2_H3_Pin GPIO_PIN_1
#define IN2_H3_GPIO_Port GPIOB
#define IN1_H3_Pin GPIO_PIN_2
#define IN1_H3_GPIO_Port GPIOB
#define DAC_SCLK_Pin GPIO_PIN_3
#define DAC_SCLK_GPIO_Port GPIOB
#define CURRENT_H4_Pin GPIO_PIN_10
#define CURRENT_H4_GPIO_Port GPIOB
#define FAULT_H4_Pin GPIO_PIN_11
#define FAULT_H4_GPIO_Port GPIOB
#define IN2_H4_Pin GPIO_PIN_12
#define IN2_H4_GPIO_Port GPIOB
#define IN1_H4_Pin GPIO_PIN_13
#define IN1_H4_GPIO_Port GPIOB
#define SLEEP_H3_Pin GPIO_PIN_15
#define SLEEP_H3_GPIO_Port GPIOB
#define SLEEP_H4_Pin GPIO_PIN_14
#define SLEEP_H4_GPIO_Port GPIOB
#define SENSOR_3_Pin GPIO_PIN_8
#define SENSOR_3_GPIO_Port GPIOA
#define SENSOR_4_Pin GPIO_PIN_9
#define SENSOR_4_GPIO_Port GPIOA
#define VEL_SENSOR_2_Pin GPIO_PIN_6
#define VEL_SENSOR_2_GPIO_Port GPIOC
#define VEL_SENSOR_1_Pin GPIO_PIN_7
#define VEL_SENSOR_1_GPIO_Port GPIOC
#define USB_VSENSE_Pin GPIO_PIN_9
#define USB_VSENSE_GPIO_Port GPIOD
#define USER_IN1_Pin GPIO_PIN_3
#define USER_IN1_GPIO_Port GPIOD
#define DAC_DIN_Pin GPIO_PIN_4
#define DAC_DIN_GPIO_Port GPIOD
#define DAC_SYNC_Pin GPIO_PIN_6
#define DAC_SYNC_GPIO_Port GPIOD
#define STATUS_LED_1_Pin GPIO_PIN_5
#define STATUS_LED_1_GPIO_Port GPIOB
#define POWER_ALERT_Pin GPIO_PIN_10
#define POWER_ALERT_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
