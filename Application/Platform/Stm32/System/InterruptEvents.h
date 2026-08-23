/**
 * @file InterruptEvents.h
 * @brief Declares the STM32 HAL bridge for driver and sensor interrupts.
 * @details Exposes only C-compatible callback entry points; interrupt context
 * never constructs or discovers the application singleton.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace dda {
class SensorController;

/**
 * @brief Publishes the already-constructed sensor target before EXTI is
 * enabled.
 * @param sensors Process-lifetime sensor controller.
 */
void initializeInterruptEvents(SensorController &sensors) noexcept;

/**
 * @brief Consume a coalesced nFAULT or POWER_ALERT interrupt indication.
 * @return `true` once after one or more safety-input falling edges.
 */
bool takeSafetyInterruptEvent() noexcept;
} // namespace dda

extern "C" {
#endif

/**
 * @brief Dispatches a falling-edge driver fault or calibration event.
 * @param gpioPin HAL GpioPin pin mask for a configured EXTI input.
 * @note Unrecognized pins are ignored.
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t gpioPin);

/**
 * @brief Dispatches a rising-edge sensor event.
 * @param gpioPin HAL GpioPin pin mask for a configured EXTI input.
 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t gpioPin);

/**
 * @brief Clocks and drives every bridge-enable pin low with bounded writes.
 * @note Safe before CubeMX GPIO setup or C++ application construction.
 */
void DdaEmergency_DisablePowerOutputs(void);

#ifdef __cplusplus
}
#endif
