/**
 * @file InternalDacChannel.h
 * @brief Declares a wrapper for one STM32 internal DAC channel.
 * @details Validates 12-bit writes and provides explicit start/stop control
 * for one board-owned VREF output channel.
 */

#pragma once

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

/**
 * @brief Controls one non-owned, CubeMX-configured 12-bit DAC channel.
 * @note The DAC peripheral, GpioPin, trigger, and output buffer must be
 * configured before this wrapper is used.
 */
class InternalDacChannel {
public:
  /**
   * @brief Binds the wrapper to a HAL DAC handle and channel.
   * @param handle HAL DAC handle; must outlive this object.
   * @param channel HAL channel selector such as `DAC_CHANNEL_1`.
   */
  InternalDacChannel(DAC_HandleTypeDef &handle, uint32_t channel) noexcept;

  /**
   * @brief Enables the bound DAC channel.
   * @return STM32 HAL status from `HAL_DAC_Start()`.
   */
  HAL_StatusTypeDef start() noexcept;

  /**
   * @brief Disables the bound DAC channel.
   * @return STM32 HAL status from `HAL_DAC_Stop()`.
   */
  HAL_StatusTypeDef stop() noexcept;

  /**
   * @brief Writes a right-aligned 12-bit DAC code.
   * @param value DAC code; values above 4095 are saturated to 4095.
   * @return STM32 HAL status from `HAL_DAC_SetValue()`.
   * @note The output voltage depends on the configured DAC reference and
   * buffer.
   */
  HAL_StatusTypeDef write(uint16_t value) noexcept;

private:
  /** @brief Largest code accepted by the 12-bit right-aligned DAC. */
  static constexpr uint16_t MaximumValue = 0x0FFFu;

  /** @brief Non-owning reference to the CubeMX HAL DAC handle. */
  DAC_HandleTypeDef &_handle;

  /** @brief HAL channel selector controlled by this instance. */
  const uint32_t _channel;
};

} // namespace dda
