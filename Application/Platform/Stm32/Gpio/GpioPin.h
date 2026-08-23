/**
 * @file GpioPin.h
 * @brief Declares a direction-checked wrapper for an STM32 HAL GPIO pin.
 * @details Rejects reads from declared outputs and writes or toggles on
 * declared inputs while retaining the board-owned HAL port and pin mask.
 */

#pragma once

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

/** @brief Logical direction used to validate GpioPin operations. */
enum class GpioDirection : uint8_t {
  INPUT,  ///< Allows reads.
  OUTPUT, ///< Allows set, reset, write, and toggle.
};

/**
 * @brief Provides non-owning access to one CubeMX-configured GpioPin pin.
 * @note The GpioPin clock, electrical mode, pull resistors, and speed must
 * already be configured for the requested use.
 */
class GpioPin {
public:
  /**
   * @brief Binds a wrapper to a GpioPin port and pin mask.
   * @param port HAL GpioPin port; must remain valid for this object's lifetime.
   * @param pin HAL GpioPin pin mask.
   * @param direction Logical direction used to reject incompatible operations.
   */
  GpioPin(GPIO_TypeDef *port, uint16_t pin,
          GpioDirection direction) noexcept;

  /** @brief Drives an output pin high. @return `true` for an output pin. */
  bool set() const noexcept;

  /** @brief Drives an output pin low. @return `true` for an output pin. */
  bool reset() const noexcept;

  /**
   * @brief Drives an output pin to the requested logic level.
   * @param state `true` for high or `false` for low.
   * @return `true` for an output pin.
   */
  bool write(bool state) const noexcept;

  /**
   * @brief Samples an input pin.
   * @param[out] state Receives `true` for high or `false` for low on success.
   * @return `true` for an input pin.
   */
  bool read(bool &state) const noexcept;

  /** @brief Toggles the bound output pin. @return `true` for an output pin. */
  bool toggle() const noexcept;

  /** @brief Returns the bound HAL GpioPin port. @return Non-owning port. */
  GPIO_TypeDef *port() const noexcept;

  /** @brief Returns the bound HAL GpioPin pin mask. @return Pin bit mask. */
  uint16_t pin() const noexcept;

  /** @brief Returns the wrapper's logical direction. @return Direction. */
  GpioDirection direction() const noexcept;

  /**
   * @brief Changes only the wrapper's logical direction.
   * @param direction New logical direction.
   */
  void setDirection(GpioDirection direction) noexcept;

private:
  GPIO_TypeDef *const _port;
  const uint16_t _pin;
  GpioDirection _direction;
};

} // namespace dda
