#include "main.h"

void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_SYSCFG_StrobeDBattpinsConfig(SYSCFG_CFGR1_UCPD1_STROBE |
                                   SYSCFG_CFGR1_UCPD2_STROBE);
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *timer)
{
  GPIO_InitTypeDef gpio = {0};

  if (timer->Instance != TIM2) {
    return;
  }

  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  gpio.Pin = VEL_SENSOR_2_Pin | VEL_SENSOR_1_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF2_TIM2;
  HAL_GPIO_Init(GPIOC, &gpio);
}
