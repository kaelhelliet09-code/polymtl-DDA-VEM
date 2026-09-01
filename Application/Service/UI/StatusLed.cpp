#include "Service/UI/StatusLed.h"

namespace dda {

StatusLed::StatusLed(GpioPin &led) noexcept : _led(led), _isOn(false) {
  (void)_led.reset();
}

void StatusLed::setFaultLatched(bool faultLatched) noexcept {
  if (_isOn == faultLatched) {
    return;
  }
  if (_led.write(faultLatched)) {
    _isOn = faultLatched;
  }
}

bool StatusLed::isOn() const noexcept { return _isOn; }

} // namespace dda
