// Implements stable-level and new-press detection for the three active user
// inputs using independent wrap-safe debounce intervals.
#include "Service/UI/UserInput.h"

namespace dda {

UserInput::UserInput(GpioPin &input1, GpioPin &input2, GpioPin &input3) noexcept
    : _inputs{&input1, &input2, &input3},
      _states{{false, false, false, false, 0U},
              {false, false, false, false, 0U},
              {false, false, false, false, 0U}} {}

void UserInput::process() noexcept {
  const uint32_t nowMs = HAL_GetTick();
  // read all three inputs and update the stable and pending states
  for (uint8_t index = 0U; index < InputCount; ++index) {
    const bool pressed = readPressed(index);

    ButtonState &state = _states[index];
    if (pressed != state.sampledPressed) {
      state.sampledPressed = pressed;
      state.sampleChangedMs = nowMs;
      continue;
    }

    if ((nowMs - state.sampleChangedMs) < DebounceTimeMs) {
      continue;
    }

    if (pressed != state.stablePressed) {
      state.stablePressed = pressed;
      if (pressed && !state.releaseRequired) {
        state.pressPending = true;
      }
    }

    // A stable release arms a new stage even if the stable state was already
    // released when requireRelease() was called.
    if (!pressed) {
      state.releaseRequired = false;
    }
  }
}

bool UserInput::takePress(UserInputId input) noexcept {
  const uint8_t index = indexOf(input);
  if ((index >= InputCount) || !_states[index].pressPending) {
    return false;
  }

  _states[index].pressPending = false;
  return true;
}

void UserInput::waitForFreshPressBlocking(UserInputId input) noexcept {
  while (!takePress(input)) {
    process();
  }
}

void UserInput::requireRelease(UserInputId input) noexcept {
  const uint8_t index = indexOf(input);
  if (index >= InputCount) {
    return;
  }

  _states[index].pressPending = false;
  _states[index].releaseRequired = true;
}

uint8_t UserInput::indexOf(UserInputId input) noexcept {
  const uint8_t index = static_cast<uint8_t>(input);
  return index < InputCount ? index : InputCount;
}

bool UserInput::readPressed(uint8_t index) const noexcept {
  bool pressed = false;
  (void)_inputs[index]->read(pressed);
  return pressed;
}

} // namespace dda
