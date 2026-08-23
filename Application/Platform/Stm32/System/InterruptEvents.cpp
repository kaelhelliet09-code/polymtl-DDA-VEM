// Implements prepublished interrupt targets and the direct-register emergency
// output shutdown that remains usable before normal HAL GPIO initialization.
#include "Platform/Stm32/System/InterruptEvents.h"
#include "Platform/Stm32/System/InterruptGuard.h"
#include "Service/Power/CoilController.h"
#include "Service/Sensor/SensorController.h"
#include "stm32g0xx_hal_tim.h"
#include <cstdint>

extern "C" {
#include "main.h"
extern TIM_HandleTypeDef htim2;
}

namespace {
dda::SensorController *interruptSensorTarget = nullptr;
volatile bool safetyInterruptPending = false;
uint32_t velocitySensor1Timestamp = 0U;
bool velocitySensor1Captured = false;

constexpr uint32_t VelocitySensor1CaptureChannel = TIM_CHANNEL_3;
constexpr uint32_t VelocitySensor2CaptureChannel = TIM_CHANNEL_4;
constexpr HAL_TIM_ActiveChannel VelocitySensor1ActiveChannel =
    HAL_TIM_ACTIVE_CHANNEL_3;
constexpr HAL_TIM_ActiveChannel VelocitySensor2ActiveChannel =
    HAL_TIM_ACTIVE_CHANNEL_4;

constexpr uint32_t gpioTwoBitFieldMask(uint32_t pins) noexcept {
  uint32_t mask = 0U;
  for (uint32_t bit = 0U; bit < 16U; ++bit) {
    if ((pins & (1UL << bit)) != 0U) {
      mask |= 3UL << (2U * bit);
    }
  }
  return mask;
}

constexpr uint32_t gpioOutputModeBits(uint32_t pins) noexcept {
  uint32_t mode = 0U;
  for (uint32_t bit = 0U; bit < 16U; ++bit) {
    if ((pins & (1UL << bit)) != 0U) {
      mode |= 1UL << (2U * bit);
    }
  }
  return mode;
}

constexpr uint32_t gpioPullDownBits(uint32_t pins) noexcept {
  uint32_t pull = 0U;
  for (uint32_t bit = 0U; bit < 16U; ++bit) {
    if ((pins & (1UL << bit)) != 0U) {
      pull |= 2UL << (2U * bit);
    }
  }
  return pull;
}
} // namespace

namespace dda {

void initializeInterruptEvents(SensorController &sensors) noexcept {
  InterruptGuard interruptGuard;
  interruptSensorTarget = &sensors;
  velocitySensor1Captured = false;
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC3 | TIM_FLAG_CC4);
  HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  const HAL_StatusTypeDef sensor1Status =
      HAL_TIM_IC_Start_IT(&htim2, VelocitySensor1CaptureChannel);
  const HAL_StatusTypeDef sensor2Status =
      HAL_TIM_IC_Start_IT(&htim2, VelocitySensor2CaptureChannel);
  if ((sensor1Status == HAL_OK) && (sensor2Status == HAL_OK)) {
    HAL_NVIC_SetPriority(TIM2_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  } else {
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_CC3 | TIM_IT_CC4);
  }
}

bool takeSafetyInterruptEvent() noexcept {
  InterruptGuard interruptGuard;
  const bool pending = safetyInterruptPending;
  safetyInterruptPending = false;
  return pending;
}

} // namespace dda

extern "C" void HAL_GPIO_EXTI_Falling_Callback(uint16_t gpioPin) {
  switch (gpioPin) {
  case FAULT_H1_Pin:
    safetyInterruptPending = true;
    dda::CoilController::handleDriverFaultFromIsr(dda::Driver::H1);
    break;
  case FAULT_H2_Pin:
    safetyInterruptPending = true;
    dda::CoilController::handleDriverFaultFromIsr(dda::Driver::H2);
    break;
  case FAULT_H3_Pin:
    safetyInterruptPending = true;
    dda::CoilController::handleDriverFaultFromIsr(dda::Driver::H3);
    break;
  case FAULT_H4_Pin:
    safetyInterruptPending = true;
    dda::CoilController::handleDriverFaultFromIsr(dda::Driver::H4);
    break;
  case POWER_ALERT_Pin:
    safetyInterruptPending = true;
    dda::CoilController::handlePowerAlertFromIsr();
    break;
  case SENSOR_1_Pin:
    if (interruptSensorTarget != nullptr) {
      interruptSensorTarget->handleSensorInterrupt(
          interruptSensorTarget->sensor(0U), dda::SensorEdge::Falling);
    }
    break;
  case SENSOR_2_Pin:
    if (interruptSensorTarget != nullptr) {
      interruptSensorTarget->handleSensorInterrupt(
          interruptSensorTarget->sensor(1U), dda::SensorEdge::Falling);
    }
    break;
  case SENSOR_3_Pin:
    if (interruptSensorTarget != nullptr) {
      interruptSensorTarget->handleSensorInterrupt(
          interruptSensorTarget->sensor(2U), dda::SensorEdge::Falling);
    }
    break;
  case SENSOR_4_Pin:
    if (interruptSensorTarget != nullptr) {
      interruptSensorTarget->handleSensorInterrupt(
          interruptSensorTarget->sensor(3U), dda::SensorEdge::Falling);
    }
    break;
  }
};

extern "C" void HAL_GPIO_EXTI_Rising_Callback(uint16_t gpioPin) {
  if (interruptSensorTarget == nullptr) {
    return;
  }
  switch (gpioPin) {
  case SENSOR_1_Pin:
    interruptSensorTarget->handleSensorInterrupt(
        interruptSensorTarget->sensor(0U), dda::SensorEdge::Rising);
    break;
  case SENSOR_2_Pin:
    interruptSensorTarget->handleSensorInterrupt(
        interruptSensorTarget->sensor(1U), dda::SensorEdge::Rising);
    break;
  case SENSOR_3_Pin:
    interruptSensorTarget->handleSensorInterrupt(
        interruptSensorTarget->sensor(2U), dda::SensorEdge::Rising);
    break;
  case SENSOR_4_Pin:
    interruptSensorTarget->handleSensorInterrupt(
        interruptSensorTarget->sensor(3U), dda::SensorEdge::Rising);
    break;
  default:
    break;
  }
}
extern "C" void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  if ((htim == nullptr) || (htim->Instance != TIM2)) {
    return;
  }

  // htim->Channel contains HAL_TIM_ACTIVE_CHANNEL_x while the capture/read
  // APIs take TIM_CHANNEL_x. These constants are different enum domains (and
  // ACTIVE_CHANNEL_4 has the same numeric value as TIM_CHANNEL_3), so they
  // must never be compared interchangeably.
  if (htim->Channel == VelocitySensor1ActiveChannel) {
    velocitySensor1Timestamp =
        HAL_TIM_ReadCapturedValue(htim, VelocitySensor1CaptureChannel);
    velocitySensor1Captured = true;
    return;
  }
  if ((htim->Channel == VelocitySensor2ActiveChannel) &&
      velocitySensor1Captured) {
    const uint32_t velocitySensor2Timestamp =
        HAL_TIM_ReadCapturedValue(htim, VelocitySensor2CaptureChannel);
    const uint32_t tickDelta =
        velocitySensor2Timestamp - velocitySensor1Timestamp;
    velocitySensor1Captured = false;
    if ((interruptSensorTarget != nullptr) && (tickDelta != 0U)) {
      interruptSensorTarget->_velocitySensor.markSpeedCaptureEvent(tickDelta);
    }
  }
}

extern "C" void DdaEmergency_DisablePowerOutputs(void) {
  constexpr uint32_t allEnablePins =
      SLEEP_H1_Pin | SLEEP_H2_Pin | SLEEP_H3_Pin | SLEEP_H4_Pin;
  constexpr uint32_t twoBitMask = gpioTwoBitFieldMask(allEnablePins);
  constexpr uint32_t outputModeBits = gpioOutputModeBits(allEnablePins);
  constexpr uint32_t pullDownBits = gpioPullDownBits(allEnablePins);

  // Error_Handler can run before CubeMX has clocked or configured GPIOC.
  // The board pin map places every bridge EN on GPIOC; establish their
  // output-low level directly, without relying on HAL initialization state.
  RCC->IOPENR |= RCC_IOPENR_GPIOCEN;
  (void)RCC->IOPENR;
  GPIOC->BSRR = allEnablePins << 16U;
  GPIOC->OTYPER &= ~allEnablePins;
  GPIOC->OSPEEDR &= ~twoBitMask;
  GPIOC->PUPDR = (GPIOC->PUPDR & ~twoBitMask) | pullDownBits;
  GPIOC->MODER = (GPIOC->MODER & ~twoBitMask) | outputModeBits;
  GPIOC->BSRR = allEnablePins << 16U;
}
