// Implements the direction-checked GPIO wrapper. Operations that conflict
// with the declared pin role fail before calling the STM32 HAL.
#include "Platform/Stm32/Gpio/GpioPin.h"

namespace dda {

// The wrapper records intended direction because HAL GPIO handles do not carry
// enough runtime metadata to reject accidental input/output misuse.
GpioPin::GpioPin(GPIO_TypeDef *port, uint16_t pin,
                 GpioDirection direction) noexcept
    : _port(port), _pin(pin), _direction(direction) {}

bool GpioPin::set() const noexcept {
  if (_direction != GpioDirection::OUTPUT) {
    return false;
  }

  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  return true;
}

bool GpioPin::reset() const noexcept {
  if (_direction != GpioDirection::OUTPUT) {
    return false;
  }

  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  return true;
}

bool GpioPin::write(bool state) const noexcept {
  if (_direction != GpioDirection::OUTPUT) {
    return false;
  }

  HAL_GPIO_WritePin(_port, _pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return true;
}

bool GpioPin::read(bool &state) const noexcept {
  if (_direction != GpioDirection::INPUT) {
    return false;
  }

  state = HAL_GPIO_ReadPin(_port, _pin) == GPIO_PIN_SET;
  return true;
}

bool GpioPin::toggle() const noexcept {
  if (_direction != GpioDirection::OUTPUT) {
    return false;
  }

  HAL_GPIO_TogglePin(_port, _pin);
  return true;
}

GPIO_TypeDef *GpioPin::port() const noexcept { return _port; }

uint16_t GpioPin::pin() const noexcept { return _pin; }

GpioDirection GpioPin::direction() const noexcept { return _direction; }

void GpioPin::setDirection(GpioDirection direction) noexcept {
  _direction = direction;
}

} // namespace dda
