#include "main.h"
#include "stm32g0xx_it.h"

extern PCD_HandleTypeDef hpcd_USB_DRD_FS;
extern TIM_HandleTypeDef htim2;

void NMI_Handler(void)
{
  EvalBoard_DisablePowerOutputs();
  __disable_irq();
  while (1) {
  }
}

void HardFault_Handler(void)
{
  EvalBoard_DisablePowerOutputs();
  __disable_irq();
  while (1) {
  }
}

void SVC_Handler(void) {}
void PendSV_Handler(void) {}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void USB_UCPD1_2_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_DRD_FS);
}

void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim2);
}
