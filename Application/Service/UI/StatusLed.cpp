// Implements foreground-timed status LED colors and blinking without blocking
// delays or timer-interrupt ownership.
#include "Service/UI/StatusLed.h"

namespace dda {

StatusLeds::StatusLeds(GpioPin *led1, GpioPin *led2, GpioPin *led3) noexcept
    : _leds{led1, led2, led3},
      _states{LedState::Off, LedState::Off, LedState::Off},
      _outputStates{false, false, false}, _lastToggleMs{0U, 0U, 0U} {
  for (uint8_t index = 0U; index < LedCount; ++index) {
    write(index, false);
  }
}

void StatusLeds::setState(LedIndex ledIndex, LedState newState) noexcept {
  const uint8_t index = static_cast<uint8_t>(ledIndex);
  if (index >= LedCount) {
    return;
  }

  // Reasserting BLINKING must not restart its phase and keep the LED on.
  if (newState == LedState::Blinking && _states[index] == LedState::Blinking) {
    return;
  }

  bool outputState = false;
  switch (newState) {
  case LedState::Off:
    outputState = false;
    break;
  case LedState::On:
  case LedState::Blinking:
    outputState = true;
    break;
  default:
    return;
  }

  _states[index] = newState;
  _lastToggleMs[index] = HAL_GetTick();
  write(index, outputState);
}

LedState StatusLeds::state(LedIndex ledIndex) const noexcept {
  const uint8_t index = static_cast<uint8_t>(ledIndex);
  return index < LedCount ? _states[index] : LedState::Off;
}

void StatusLeds::update() noexcept { updateAt(HAL_GetTick()); }

void StatusLeds::updateAt(uint32_t currentTimeMs) noexcept {
  for (uint8_t index = 0U; index < LedCount; ++index) {
    if (_states[index] != LedState::Blinking) {
      continue;
    }

    // Unsigned subtraction handles HAL tick rollover; advancing by whole
    // intervals preserves phase if the main loop was delayed.
    const uint32_t elapsedMs = currentTimeMs - _lastToggleMs[index];
    if (elapsedMs < BlinkHalfPeriodMilliseconds) {
      continue;
    }
    const uint32_t elapsedIntervals = elapsedMs / BlinkHalfPeriodMilliseconds;
    _lastToggleMs[index] += elapsedIntervals * BlinkHalfPeriodMilliseconds;

    if ((elapsedIntervals & 1U) != 0U) {
      write(index, !_outputStates[index]);
    }
  }
}

void StatusLeds::write(uint8_t ledIndex, bool isOn) noexcept {
  _outputStates[ledIndex] = isOn;
  if (_leds[ledIndex] != nullptr) {
    _leds[ledIndex]->write(isOn);
  }
}

} // namespace dda
