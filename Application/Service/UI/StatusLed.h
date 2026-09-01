/** @file StatusLed.h @brief Single latched-fault indicator LED. */
#pragma once

#include "Platform/Stm32/Gpio/GpioPin.h"

namespace dda {

/** @brief Drives the board's only status LED directly from fault-latch state. */
class StatusLed {
public:
  /** @brief Bind the indicator GPIO. @param led Output that outlives this. */
  explicit StatusLed(GpioPin &led) noexcept;

  /**
   * @brief Turn the LED on exactly while any relevant fault is latched.
   * @param faultLatched Current aggregate software-latch state.
   */
  void setFaultLatched(bool faultLatched) noexcept;

  /**
   * @brief Return the last fault indication applied to the GPIO.
   * @return Whether the LED is on.
   */
  bool isOn() const noexcept;

private:
  GpioPin &_led;
  bool _isOn;
};

} // namespace dda
