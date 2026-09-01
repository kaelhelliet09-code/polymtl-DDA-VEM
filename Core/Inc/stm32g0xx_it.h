/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32g0xx_it.h
 * @brief   Cortex exception and project peripheral interrupt declarations.
 * @note    Every handler executes in interrupt context and must remain
 *          bounded and non-blocking unless it intentionally latches a fault.
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
#ifndef __STM32G0xx_IT_H
#define __STM32G0xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

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
void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void USB_UCPD1_2_IRQHandler(void);
void DMA1_Channel2_3_IRQHandler(void);
void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler(void);
void TIM3_TIM4_IRQHandler(void);
void TIM7_LPTIM2_IRQHandler(void);
void I2C1_IRQHandler(void);
/* USER CODE BEGIN EFP */

/** @brief Dispatch EXTI lines 0 and 1 for H3 and H1 faults. */
void EXTI0_1_IRQHandler(void);
/** @brief Dispatch the currently unused EXTI lines 2 and 3 vector. */
void EXTI2_3_IRQHandler(void);
/**
 * @brief Dispatch shared EXTI lines 4 through 15 for bridge faults, sensors,
 *        and the power alert.
 */
void EXTI4_15_IRQHandler(void);
/** @brief Dispatch the two velocity input-capture channels on TIM2. */
void TIM2_IRQHandler(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32G0xx_IT_H */
