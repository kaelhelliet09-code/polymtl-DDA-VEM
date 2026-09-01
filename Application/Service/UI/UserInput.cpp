#include "Service/UI/UserInput.h"

namespace dda {

UserInput::UserInput(GpioPin &button) noexcept
    : _button(button), _sampledPressed(false), _stablePressed(false),
      _pressPending(false), _sampleChangedMilliseconds(0U) {}

void UserInput::process() noexcept {
  bool pressed = false;
  (void)_button.read(pressed);
  const uint32_t now = HAL_GetTick();
  if (pressed != _sampledPressed) {
    _sampledPressed = pressed;
    _sampleChangedMilliseconds = now;
    return;
  }
  if (((now - _sampleChangedMilliseconds) <
       config::UserInputDebounceMilliseconds) ||
      (pressed == _stablePressed)) {
    return;
  }
  _stablePressed = pressed;
  if (pressed) {
    _pressPending = true;
  }
}

bool UserInput::takePress() noexcept {
  const bool pending = _pressPending;
  _pressPending = false;
  return pending;
}

} // namespace dda
