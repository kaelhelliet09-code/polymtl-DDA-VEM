#include "eval_board.h"
#include "main.h"
#include "usb_device.h"

TIM_HandleTypeDef htim2;

static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM2_Init();

  if ((HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3) != HAL_OK) ||
      (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4) != HAL_OK)) {
    Error_Handler();
  }

  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC3 | TIM_FLAG_CC4);
  HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  HAL_NVIC_SetPriority(TIM2_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);

  MX_USB_Device_Init();

  while (1) {
    EvalBoard_Process();
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clocks = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
  oscillator.HSEState = RCC_HSE_ON;
  oscillator.HSI48State = RCC_HSI48_ON;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  oscillator.PLL.PLLM = RCC_PLLM_DIV1;
  oscillator.PLL.PLLN = 8;
  oscillator.PLL.PLLP = RCC_PLLP_DIV2;
  oscillator.PLL.PLLQ = RCC_PLLQ_DIV2;
  oscillator.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
    Error_Handler();
  }

  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                     RCC_CLOCKTYPE_PCLK1;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_TIM2_Init(void)
{
  TIM_MasterConfigTypeDef master = {0};
  TIM_IC_InitTypeDef capture = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0U;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 0xFFFFFFFFU;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }

  master.MasterOutputTrigger = TIM_TRGO_RESET;
  master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &master) != HAL_OK) {
    Error_Handler();
  }

  capture.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  capture.ICSelection = TIM_ICSELECTION_DIRECTTI;
  capture.ICPrescaler = TIM_ICPSC_DIV1;
  capture.ICFilter = 0U;
  if ((HAL_TIM_IC_ConfigChannel(&htim2, &capture, TIM_CHANNEL_3) != HAL_OK) ||
      (HAL_TIM_IC_ConfigChannel(&htim2, &capture, TIM_CHANNEL_4) != HAL_OK)) {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA,
                    IN2_H1_Pin | IN1_H1_Pin | IN2_H2_Pin | IN1_H2_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB,
                    IN2_H3_Pin | IN1_H3_Pin | IN2_H4_Pin | IN1_H4_Pin |
                        SLEEP_H3_Pin | SLEEP_H4_Pin | DAC_SCLK_Pin |
                        STATUS_LED_1_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, SLEEP_H1_Pin | SLEEP_H2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, SENSOR_ENA_Pin | PMODE_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, DAC_DIN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, DAC_SYNC_Pin, GPIO_PIN_SET);

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = IN2_H1_Pin | IN1_H1_Pin | IN2_H2_Pin | IN1_H2_Pin;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = IN2_H3_Pin | IN1_H3_Pin | IN2_H4_Pin | IN1_H4_Pin |
             DAC_SCLK_Pin | STATUS_LED_1_Pin;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = SLEEP_H3_Pin | SLEEP_H4_Pin;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = SLEEP_H1_Pin | SLEEP_H2_Pin;
  HAL_GPIO_Init(GPIOC, &gpio);

  gpio.Pin = SENSOR_ENA_Pin | PMODE_Pin;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &gpio);

  gpio.Pin = DAC_DIN_Pin | DAC_SYNC_Pin;
  HAL_GPIO_Init(GPIOD, &gpio);
}

void EvalBoard_DisablePowerOutputs(void)
{
  GPIOA->BRR = IN2_H1_Pin | IN1_H1_Pin | IN2_H2_Pin | IN1_H2_Pin;
  GPIOB->BRR = IN2_H3_Pin | IN1_H3_Pin | IN2_H4_Pin | IN1_H4_Pin |
               SLEEP_H3_Pin | SLEEP_H4_Pin;
  GPIOC->BRR = SLEEP_H1_Pin | SLEEP_H2_Pin;
}

void Error_Handler(void)
{
  EvalBoard_DisablePowerOutputs();
  __disable_irq();
  while (1) {
  }
}
