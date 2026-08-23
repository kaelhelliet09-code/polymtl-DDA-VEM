/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32g0xx_it.c
 * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32g0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
void DdaEmergency_DisablePowerOutputs(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_DRD_FS;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_i2c1_rx;
extern DMA_HandleTypeDef hdma_i2c1_tx;
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim7;
/* USER CODE BEGIN EV */
extern TIM_HandleTypeDef htim2;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  DdaEmergency_DisablePowerOutputs();
  __disable_irq();
#if defined(DDA_RELEASE_BUILD)
  NVIC_SystemReset();
#endif
  while (1) {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  DdaEmergency_DisablePowerOutputs();
  __disable_irq();
#if defined(DDA_RELEASE_BUILD)
  NVIC_SystemReset();
#endif
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    // Preserve the fault context for a debugger; continuing would be unsafe.
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles USB, UCPD1 and UCPD2 global interrupts.
  */
void USB_UCPD1_2_IRQHandler(void)
{
  /* USER CODE BEGIN USB_UCPD1_2_IRQn 0 */
  // HAL advances control and CDC endpoint state directly in interrupt context.
  /* USER CODE END USB_UCPD1_2_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_DRD_FS);
  /* USER CODE BEGIN USB_UCPD1_2_IRQn 1 */

  /* USER CODE END USB_UCPD1_2_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel 2 and channel 3 interrupts.
  */
void DMA1_Channel2_3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel2_3_IRQn 0 */
  // Dispatch ADC1 transfer-complete and DMA error flags to the linked handle.
  /* USER CODE END DMA1_Channel2_3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc1);
  /* USER CODE BEGIN DMA1_Channel2_3_IRQn 1 */

  /* USER CODE END DMA1_Channel2_3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 Ch4 to Ch7, DMA2 Ch1 to Ch5 and DMAMUX1 Overrun Interrupts.
  */
void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn 0 */
  // This shared vector services the configured I2C1 RX DMA2 channel.
  /* USER CODE END DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_i2c1_rx);
  HAL_DMA_IRQHandler(&hdma_i2c1_tx);
  /* USER CODE BEGIN DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn 1 */

  /* USER CODE END DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn 1 */
}

/**
  * @brief This function handles TIM3, TIM4 global Interrupt.
  */
void TIM3_TIM4_IRQHandler(void)
{
  /* USER CODE BEGIN TIM3_TIM4_IRQn 0 */

  /* USER CODE END TIM3_TIM4_IRQn 0 */
  HAL_TIM_IRQHandler(&htim3);
  HAL_TIM_IRQHandler(&htim4);
  /* USER CODE BEGIN TIM3_TIM4_IRQn 1 */

  /* USER CODE END TIM3_TIM4_IRQn 1 */
}

/**
  * @brief This function handles TIM7 and LPTIM2 global Interrupt.
  */
void TIM7_LPTIM2_IRQHandler(void)
{
  /* USER CODE BEGIN TIM7_LPTIM2_IRQn 0 */

  /* USER CODE END TIM7_LPTIM2_IRQn 0 */
  HAL_TIM_IRQHandler(&htim7);
  /* USER CODE BEGIN TIM7_LPTIM2_IRQn 1 */

  /* USER CODE END TIM7_LPTIM2_IRQn 1 */
}

/**
  * @brief This function handles I2C1 event global interrupt / I2C1 wake-up interrupt through EXTI line 23.
  */
void I2C1_IRQHandler(void)
{
  /* USER CODE BEGIN I2C1_IRQn 0 */
  // Bus error, arbitration loss, and overrun require HAL's error state
  // transition; all remaining flags belong to the normal event state machine.
  /* USER CODE END I2C1_IRQn 0 */
  if (hi2c1.Instance->ISR & (I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR))
  {
    HAL_I2C_ER_IRQHandler(&hi2c1);
  }
  else
  {
    HAL_I2C_EV_IRQHandler(&hi2c1);
  }
  /* USER CODE BEGIN I2C1_IRQn 1 */

  /* USER CODE END I2C1_IRQn 1 */
}

/* USER CODE BEGIN 1 */

// Each shared vector offers every configured line to HAL; HAL checks the
// pending bit before invoking the application EXTI callback.
void EXTI0_1_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(FAULT_H3_Pin); }

void EXTI2_3_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(FAULT_H1_Pin);
}

void EXTI4_15_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(FAULT_H2_Pin);
  HAL_GPIO_EXTI_IRQHandler(SENSOR_3_Pin);
  HAL_GPIO_EXTI_IRQHandler(SENSOR_4_Pin);
  HAL_GPIO_EXTI_IRQHandler(POWER_ALERT_Pin);
  HAL_GPIO_EXTI_IRQHandler(FAULT_H4_Pin);
  HAL_GPIO_EXTI_IRQHandler(SENSOR_1_Pin);
  HAL_GPIO_EXTI_IRQHandler(SENSOR_2_Pin);
}

void TIM2_IRQHandler(void) { HAL_TIM_IRQHandler(&htim2); }

/* USER CODE END 1 */
