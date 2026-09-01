/** @file UserInput.h @brief Debounced access to the unused single button. */
#pragma once

#include "Config/BoardConfig.h"
#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

namespace dda {

/**
 * @brief Polls the one populated active-high user button.
 * @note No application behavior is assigned to this input on DDA V2.
 */
class UserInput {
public:
  /** @brief Bind the sole button GPIO. @param button Input that outlives this. */
  explicit UserInput(GpioPin &button) noexcept;

  /** @brief Poll and non-blockingly debounce the input. */
  void process() noexcept;

  /** @brief Consume a debounced press. @return Whether a press was pending. */
  bool takePress() noexcept;

private:
  GpioPin &_button;
  bool _sampledPressed;
  bool _stablePressed;
  bool _pressPending;
  uint32_t _sampleChangedMilliseconds;
};

} // namespace dda
