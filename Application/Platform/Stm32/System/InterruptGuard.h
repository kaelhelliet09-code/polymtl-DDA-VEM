#pragma once

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

/** Restores the interrupt mask that was active when the guard was created. */
class InterruptGuard {
public:
  InterruptGuard() noexcept : _interruptState(__get_PRIMASK()), _active(true) {
    __disable_irq();
  }

  InterruptGuard(const InterruptGuard &) = delete;
  InterruptGuard &operator=(const InterruptGuard &) = delete;

  InterruptGuard(InterruptGuard &&other) noexcept
      : _interruptState(other._interruptState), _active(other._active) {
    other._active = false;
  }

  ~InterruptGuard() noexcept {
    if (_active && (_interruptState == 0U)) {
      __enable_irq();
    }
  }

private:
  uint32_t _interruptState;
  bool _active;
};

} // namespace dda
