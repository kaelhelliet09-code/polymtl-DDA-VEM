/**
 * @file Dac088s085.h
 * @brief Declares blocking GPIO serial control for the eight-channel DAC.
 * @details Validates channel selections, frames device commands, and controls
 * the SYNC frame without exposing pin details to higher-level services.
 */

#pragma once

#include "Drivers/DacChannel.h"
#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

/**
 * @brief Provides bounded, blocking writes to a Dac088s085.
 * @note Frames are sent MSB-first and clocked into the DAC on falling edges.
 * @note The device has no analog output readback through this interface.
 */
class Dac088s085 {
public:
  /** @brief Number of individually addressable DAC outputs. */
  static constexpr uint8_t ChannelCount = 8U;

  /**
   * @brief Binds the driver to the three DAC serial GPIOs.
   */
  Dac088s085(GpioPin &data, GpioPin &sync, GpioPin &clock) noexcept;

  /**
   * @brief Sets the serial pins idle and selects device write-through mode.
   * @param timeoutMs Must be nonzero.
   * @return HAL status.
   */
  HAL_StatusTypeDef init(uint32_t timeoutMs) noexcept;

  /**
   * @brief Writes an 8-bit code to one enumerated DAC output.
   * @param channel Output channel A through H.
   * @param value Unsigned DAC code in the range 0 to 255.
   * @param timeoutMs Must be nonzero.
   * @return HAL status.
   */
  HAL_StatusTypeDef write(DacChannel channel, uint8_t value,
                          uint32_t timeoutMs) noexcept;

  /**
   * @brief Writes an 8-bit code to a numeric DAC output.
   * @param channel Zero-based output index in the range 0 to 7.
   * @param value Unsigned DAC code in the range 0 to 255.
   * @param timeoutMs Must be nonzero.
   * @return HAL status.
   */
  HAL_StatusTypeDef write(uint8_t channel, uint8_t value,
                          uint32_t timeoutMs) noexcept;

  /**
   * @brief Broadcasts one 8-bit code to all DAC outputs.
   * @param value Unsigned DAC code in the range 0 to 255.
   * @param timeoutMs Must be nonzero.
   * @return HAL status.
   */
  HAL_StatusTypeDef writeAll(uint8_t value, uint32_t timeoutMs) noexcept;

  /**
   * @brief Restores the three serial pins to their idle levels.
   * @return HAL status.
   */
  HAL_StatusTypeDef abort() noexcept;

private:
  /**
   * @brief Encodes and sends one channel-write frame.
   * @param channel Three-bit DAC channel address.
   * @param value Unsigned 8-bit DAC code.
   * @param timeoutMs Must be nonzero.
   * @return Result from `transmit()`.
   */
  HAL_StatusTypeDef writeDac(uint8_t channel, uint8_t value,
                             uint32_t timeoutMs) noexcept;

  /**
   * @brief Sends one SYNC-framed 16-bit command.
   * @param frame Complete Dac088s085 command word.
   * @param timeoutMs Must be nonzero.
   * @return HAL status.
   */
  HAL_StatusTypeDef transmit(uint16_t frame, uint32_t timeoutMs) noexcept;

  GpioPin &_data;
  GpioPin &_sync;
  GpioPin &_clock;
};

} // namespace dda
