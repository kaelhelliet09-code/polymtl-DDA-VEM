/**
 * @file StatusLed.h
 * @brief Declares non-blocking control for the three board status LEDs.
 * @details Converts logical colors and blink requests into foreground-timed
 * GPIO updates without delays or interrupt ownership.
 */

#pragma once

#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

namespace dda {

/** @brief Logical operating mode for a status LED. */
enum class LedState : uint8_t {
  Off = 0 /**< Hold output inactive. */,
  On = 1 /**< Hold output active. */,
  Blinking = 2 /**< Toggle cooperatively. */
};

/** @brief Zero-based selector for the three physical status LEDs. */
enum class LedIndex : uint8_t {
  LED1 = 0 /**< First status LED. */,
  LED2 = 1 /**< Second status LED. */,
  LED3 = 2 /**< Third status LED. */
};

/**
 * @brief Drives three GpioPin LEDs with steady or cooperative blinking states.
 *
 * GpioPin objects are referenced but not owned. Null pointers are accepted and
 * retain logical state without accessing hardware.
 */
class StatusLeds {
public:
  /** @brief Blink half-period in milliseconds. */
  static constexpr uint32_t BlinkHalfPeriodMilliseconds = 500U;

  /**
   * @brief Constructs the controller and drives all supplied LEDs off.
   * @param led1 Non-owning pointer to LED 1 GpioPin, or `nullptr`.
   * @param led2 Non-owning pointer to LED 2 GpioPin, or `nullptr`.
   * @param led3 Non-owning pointer to LED 3 GpioPin, or `nullptr`.
   * @note Each non-null GpioPin must remain valid for this object's lifetime
   * and be configured as an output.
   */
  StatusLeds(GpioPin *led1, GpioPin *led2, GpioPin *led3) noexcept;

  /** @brief Prevents copying a controller that stores non-owning GpioPin
   * pointers. */
  StatusLeds(const StatusLeds &) = delete;

  /** @brief Prevents copy assignment of the fixed GpioPin binding. */
  StatusLeds &operator=(const StatusLeds &) = delete;

  /** @brief Prevents moving a controller with fixed GpioPin bindings. */
  StatusLeds(StatusLeds &&) = delete;

  /** @brief Prevents move assignment of the fixed GpioPin binding. */
  StatusLeds &operator=(StatusLeds &&) = delete;

  /**
   * @brief Sets one LED's logical state and immediately updates its output.
   * @param ledIndex LED selector; out-of-range values are ignored.
   * @param state OFF, ON, or BLINKING; invalid enum values are ignored.
   * @note Entering BLINKING starts in the on phase. Reapplying BLINKING keeps
   * the existing phase.
   */
  void setState(LedIndex ledIndex, LedState state) noexcept;

  /**
   * @brief Gets one LED's configured logical state.
   * @param ledIndex LED selector.
   * @return Configured state, or LedState::Off for an invalid selector.
   */
  LedState state(LedIndex ledIndex) const noexcept;

  /**
   * @brief Advances all blinking LEDs using the HAL millisecond tick.
   * @note Call regularly from the main loop; no blocking delay is used.
   */
  void update() noexcept;

private:
  /** @brief Number of GPIOs managed by the controller. */
  static constexpr uint8_t LedCount = 3U;

  /**
   * @brief Advances blinking states at a supplied HAL tick.
   * @param currentTimeMs Current monotonic tick in milliseconds.
   */
  void updateAt(uint32_t currentTimeMs) noexcept;

  /**
   * @brief Stores and applies one physical output level.
   * @param ledIndex Valid zero-based index in `[0, ledCount)`.
   * @param isOn Logical output level passed to GpioPin::write().
   */
  void write(uint8_t ledIndex, bool isOn) noexcept;

  /** @brief Non-owning GpioPin pointers by LED index. */
  GpioPin *const _leds[LedCount];

  /** @brief Requested logical modes by LED index. */
  LedState _states[LedCount];

  /** @brief Last requested physical levels. */
  bool _outputStates[LedCount];

  /** @brief Blink phase anchors in milliseconds. */
  uint32_t _lastToggleMs[LedCount];
};

} // namespace dda
