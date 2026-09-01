// Implements the DRV8874 input truth tables, per-channel VREF control, and
// active-low fault checks.
#include "Drivers/Drv8874.h"

#include "Config/PowerConfig.h"
#include "Config/SafetyConfig.h"

namespace dda {

Drv8874::Drv8874(GPIO_TypeDef *in1Port, uint16_t in1Pin, GPIO_TypeDef *in2Port,
                 uint16_t in2Pin, GPIO_TypeDef *sleepPort, uint16_t sleepPin,
                 GPIO_TypeDef *faultPort, uint16_t faultPin, Dac088s085 &dac,
                 DacChannel dacChannel) noexcept
    : _in1(in1Port, in1Pin, GpioDirection::OUTPUT),
      _in2(in2Port, in2Pin, GpioDirection::OUTPUT),
      _sleep(sleepPort, sleepPin, GpioDirection::OUTPUT),
      _fault(faultPort, faultPin, GpioDirection::INPUT), _dac(dac),
      _dacChannel(dacChannel), _state(State::Sleep),
      _currentThresholdMilliamps(config::DefaultCoilCurrentMilliamps),
      _appliedCurrentThresholdMilliamps(0U), _pwmMode(true),
      _faultLatched(false) {}

HAL_StatusTypeDef Drv8874::init() noexcept {
  const bool sleeping = applyState(State::Sleep);
  _faultLatched = false;
  const HAL_StatusTypeDef dacStatus = disableCurrentThresholdOutput();
  return sleeping ? dacStatus : HAL_ERROR;
}

bool Drv8874::setState(State state) noexcept {
  return isValidState(state) && applyState(state);
}

bool Drv8874::applyState(State state) noexcept {
  if (state == State::Sleep) {
    const bool inputsLow = _in1.reset() && _in2.reset();
    const bool sleeping = _sleep.reset();
    if (inputsLow && sleeping) {
      _state = State::Sleep;
      return true;
    }
    return false;
  }

  if (hasFault()) {
    return false;
  }

  // IN1=IN2=0 is the non-driving coast state in PWM mode. Apply it before
  // changing the awake-state truth-table inputs.
  if (_state != State::Sleep) {
    if (!_in1.reset() || !_in2.reset()) {
      return false;
    }
    _state = State::CoilOff;
  }

  bool inputsValid = false;
  switch (state) {
  case State::CoilOff:
    inputsValid = _in1.reset() && _in2.reset();
    break;
  case State::Forward:
    inputsValid =
        _pwmMode ? (_in2.reset() && _in1.set()) : (_in2.set() && _in1.set());
    break;
  case State::Reverse:
    inputsValid =
        _pwmMode ? (_in1.reset() && _in2.set()) : (_in2.reset() && _in1.set());
    break;
  case State::SlowDecay:
    inputsValid =
        _pwmMode ? (_in1.set() && _in2.set()) : (_in1.reset() && _in2.reset());
    break;
  case State::Sleep:
    break;
  }

  if (!inputsValid || !faultInputReleased() || !_sleep.set()) {
    (void)_in1.reset();
    (void)_in2.reset();
    (void)_sleep.reset();
    _state = State::Sleep;
    return false;
  }

  _state = state;
  return true;
}

Drv8874::State Drv8874::state() const noexcept { return _state; }

bool Drv8874::setPwmMode(bool pwmMode) noexcept {
  if (_state != State::Sleep) {
    return false;
  }
  _pwmMode = pwmMode;
  return true;
}

bool Drv8874::pwmMode() const noexcept { return _pwmMode; }

HAL_StatusTypeDef
Drv8874::setCurrentThresholdMilliamps(uint16_t currentMilliamps) noexcept {
  if (currentMilliamps > config::MaximumCoilCurrentMilliamps) {
    return HAL_ERROR;
  }
  if (_state == State::Sleep) {
    _currentThresholdMilliamps = currentMilliamps;
    return HAL_OK;
  }
  const HAL_StatusTypeDef status = writeCurrentThreshold(currentMilliamps);
  if (status == HAL_OK) {
    _currentThresholdMilliamps = currentMilliamps;
  }
  return status;
}

HAL_StatusTypeDef Drv8874::applyCurrentThreshold() noexcept {
  return writeCurrentThreshold(_currentThresholdMilliamps);
}

HAL_StatusTypeDef Drv8874::disableCurrentThresholdOutput() noexcept {
  return writeCurrentThreshold(config::SafeStartupCurrentMilliamps);
}

HAL_StatusTypeDef
Drv8874::writeCurrentThreshold(uint16_t currentMilliamps) noexcept {
  const HAL_StatusTypeDef status = _dac.write(
      _dacChannel, config::currentMilliampsToReferenceCode(currentMilliamps),
      config::DriverDacTimeoutMilliseconds);
  if (status == HAL_OK) {
    _appliedCurrentThresholdMilliamps = currentMilliamps;
  }
  return status;
}

uint16_t Drv8874::currentThresholdMilliamps() const noexcept {
  return _currentThresholdMilliamps;
}

uint16_t Drv8874::appliedCurrentThresholdMilliamps() const noexcept {
  return _appliedCurrentThresholdMilliamps;
}

DacChannel Drv8874::dacChannel() const noexcept { return _dacChannel; }

bool Drv8874::hasFault() const noexcept {
  bool faultPinHigh = true;
  return _faultLatched || !_fault.read(faultPinHigh) || !faultPinHigh;
}

bool Drv8874::clearFault() noexcept {
  if (!faultInputReleased()) {
    return false;
  }
  _faultLatched = false;
  return true;
}

void Drv8874::onFaultInterrupt() noexcept { _faultLatched = true; }

bool Drv8874::faultInputReleased() const noexcept {
  bool faultPinHigh = false;
  return _fault.read(faultPinHigh) && faultPinHigh;
}

bool Drv8874::isValidState(State state) noexcept {
  switch (state) {
  case State::Sleep:
  case State::CoilOff:
  case State::Forward:
  case State::Reverse:
  case State::SlowDecay:
    return true;
  }
  return false;
}

} // namespace dda
