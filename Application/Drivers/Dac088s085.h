/**
 * @file Dac088s085.h
 * @brief Declares blocking SPI control for the eight-channel Dac088s085.
 * @details Validates channel selections, frames device commands, and controls
 * chip select without exposing HAL SPI details to higher-level services.
 */

#pragma once

#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

/** @brief Dac088s085 output-channel address encoded in a command frame. */
enum DacChannel : uint8_t {
  DAC_CHANNEL_A = 0x00U, ///< Output A.
  DAC_CHANNEL_B = 0x01U, ///< Output B.
  DAC_CHANNEL_C = 0x02U, ///< Output C.
  DAC_CHANNEL_D = 0x03U, ///< Output D.
  DAC_CHANNEL_E = 0x04U, ///< Output E.
  DAC_CHANNEL_F = 0x05U, ///< Output F.
  DAC_CHANNEL_G = 0x06U, ///< Output G.
  DAC_CHANNEL_H = 0x07U  ///< Output H.
};

/**
 * @brief Provides bounded, blocking writes to a Dac088s085.
 * @note The SPI peripheral must be configured for 16-bit, MSB-first, mode-1
 * transmit operation. Chip select is externally owned and active low.
 * @note The device has no analog output readback through this interface.
 */
class Dac088s085 {
public:
  /** @brief Number of individually addressable DAC outputs. */
  static constexpr uint8_t ChannelCount = 8U;

  /**
   * @brief Binds the driver to an SPI peripheral and chip-select GpioPin.
   * @param spi HAL SPI handle; must outlive this object.
   * @param chipSelect Output GpioPin controlling the target's active-low
   * select; must outlive this object.
   */
  Dac088s085(SPI_HandleTypeDef &spi, GpioPin &chipSelect) noexcept;

  /**
   * @brief Deasserts chip select and selects device write-through mode.
   * @param timeoutMs SPI timeout, in milliseconds; must be nonzero.
   * @return HAL SPI status, or `HAL_ERROR` for zero timeout or chip-select
   * failure.
   */
  HAL_StatusTypeDef init(uint32_t timeoutMs) noexcept;

  /**
   * @brief Writes an 8-bit code to one enumerated DAC output.
   * @param channel Output channel A through H.
   * @param value Unsigned DAC code in the range 0 to 255.
   * @param timeoutMs SPI timeout, in milliseconds; must be nonzero.
   * @return HAL SPI status, or `HAL_ERROR` for an invalid enum value, zero
   * timeout, or chip-select failure.
   */
  HAL_StatusTypeDef write(DacChannel channel, uint8_t value,
                          uint32_t timeoutMs) noexcept;

  /**
   * @brief Writes an 8-bit code to a numeric DAC output.
   * @param channel Zero-based output index in the range 0 to 7.
   * @param value Unsigned DAC code in the range 0 to 255.
   * @param timeoutMs SPI timeout, in milliseconds; must be nonzero.
   * @return HAL SPI status, or `HAL_ERROR` for an invalid channel, zero
   * timeout, or chip-select failure.
   */
  HAL_StatusTypeDef write(uint8_t channel, uint8_t value,
                          uint32_t timeoutMs) noexcept;

  /**
   * @brief Broadcasts one 8-bit code to all DAC outputs.
   * @param value Unsigned DAC code in the range 0 to 255.
   * @param timeoutMs SPI timeout, in milliseconds; must be nonzero.
   * @return HAL SPI status, or `HAL_ERROR` for zero timeout or chip-select
   * failure.
   */
  HAL_StatusTypeDef writeAll(uint8_t value, uint32_t timeoutMs) noexcept;

  /**
   * @brief Aborts an in-progress SPI transfer and deasserts chip select.
   * @return `HAL_OK` when SPI was idle or abort succeeded, the HAL abort error,
   * or `HAL_ERROR` if chip select could not be driven high.
   * @note Chip-select deassertion is attempted even when HAL abort fails.
   */
  HAL_StatusTypeDef abort() noexcept;

private:
  /**
   * @brief Encodes and sends one channel-write frame.
   * @param channel Three-bit DAC channel address.
   * @param value Unsigned 8-bit DAC code.
   * @param timeoutMs SPI timeout, in milliseconds.
   * @return Result from `transmit()`.
   */
  HAL_StatusTypeDef writeDac(uint8_t channel, uint8_t value,
                             uint32_t timeoutMs) noexcept;

  /**
   * @brief Sends one chip-select-framed 16-bit command.
   * @param frame Complete Dac088s085 command word.
   * @param timeoutMs SPI timeout, in milliseconds; must be nonzero.
   * @return HAL SPI status or `HAL_ERROR` on chip-select failure.
   */
  HAL_StatusTypeDef transmit(uint16_t frame, uint32_t timeoutMs) noexcept;

  /** @brief Non-owning reference to the configured HAL SPI handle. */
  SPI_HandleTypeDef &_spi;

  /** @brief Non-owning reference to the active-low chip-select output. */
  GpioPin &_chipSelect;
};

} // namespace dda
