// Implements the board's DRV8874 PH/EN truth table, nSLEEP sequencing,
// active-low fault checks, and the direction-change dead time.
#include "Drivers/Drv8874.h"

#include "Config/BoardConfig.h"
#include "Config/SafetyConfig.h"

namespace {

bool waitForDirectionDeadTime(uint32_t disabledAtTicks) noexcept {
  if ((TIM2->CR1 & TIM_CR1_CEN) == 0U) {
    return false;
  }

  constexpr uint32_t directionDeadTimeTicks =
      dda::config::DirectionDeadTimeMicroseconds *
      dda::config::Tim2TicksPerMicrosecond;
  // Bound the polling work in case TIM2 stops after the running check.
  constexpr uint32_t maximumPolls =
      (directionDeadTimeTicks + 1U) * 4U;
  uint32_t polls = 0U;
  while ((TIM2->CNT - disabledAtTicks) < directionDeadTimeTicks) {
    ++polls;
    if (polls >= maximumPolls) {
      return false;
    }
  }
  return true;
}

} // namespace

namespace dda {

Drv8874::Drv8874(GPIO_TypeDef *driveEnablePort, uint16_t driveEnablePin,
                 GPIO_TypeDef *phasePort, uint16_t phasePin,
                 GPIO_TypeDef *sleepControlPort, uint16_t sleepControlPin,
                 GPIO_TypeDef *faultPort, uint16_t faultPin) noexcept
    : _driveEnable(driveEnablePort, driveEnablePin, GpioDirection::OUTPUT),
      _phase(phasePort, phasePin, GpioDirection::OUTPUT),
      _sleepControl(sleepControlPort, sleepControlPin, GpioDirection::OUTPUT),
      _fault(faultPort, faultPin, GpioDirection::INPUT), _state(State::Sleep),
      _disabledAtTicks(0U), _disabledAtValid(false),
      _faultLatched(false) {}

void Drv8874::init() noexcept {
  (void)applyState(State::Sleep);
  _faultLatched = false;
}

bool Drv8874::setState(State state) noexcept {
  if (!isValidState(state)) {
    return false;
  }
  return applyState(state);
}

bool Drv8874::applyState(State state) noexcept {
  if (state == State::Sleep) {
    // nSLEEP low is the only high-impedance state in PH/EN mode.
    const bool sleeping = _sleepControl.reset();
    const bool driveDisabled = _driveEnable.reset();
    const bool phaseLow = _phase.reset();
    if (sleeping && driveDisabled && phaseLow) {
      _state = state;
      _disabledAtValid = (TIM2->CR1 & TIM_CR1_CEN) != 0U;
      if (_disabledAtValid) {
        _disabledAtTicks = TIM2->CNT;
      }
      return true;
    }
    return false;
  }

  if (hasFault()) {
    return false;
  }

  // Remove drive through EN before changing PH. Keep nSLEEP high when moving
  // between active states so an ordinary state change does not restart tWAKE.
  if (_state != State::Sleep) {
    if (!_driveEnable.reset()) {
      return false;
    }
    _state = State::CoilOff;
    _disabledAtValid = (TIM2->CR1 & TIM_CR1_CEN) != 0U;
    if (_disabledAtValid) {
      _disabledAtTicks = TIM2->CNT;
    }
  }

  if (!_disabledAtValid || !waitForDirectionDeadTime(_disabledAtTicks)) {
    (void)_sleepControl.reset();
    (void)_driveEnable.reset();
    (void)_phase.reset();
    _state = State::Sleep;
    return false;
  }

  bool inputsValid = false;
  switch (state) {
  case State::CoilOff:
  case State::SlowDecay:
    // EN low commands brake/low-side slow decay; PH is don't-care.
    inputsValid = _driveEnable.reset() && _phase.reset();
    break;
  case State::Forward:
    inputsValid = _phase.set() && _driveEnable.set();
    break;
  case State::Reverse:
    inputsValid = _phase.reset() && _driveEnable.set();
    break;
  case State::Sleep:
    break;
  }

  // Recheck immediately before leaving sleep. For an already-awake driver,
  // nSLEEP remains high and this write simply preserves that state.
  if (!inputsValid || !faultInputReleased() || !_sleepControl.set()) {
    (void)_sleepControl.reset();
    (void)_driveEnable.reset();
    (void)_phase.reset();
    _state = State::Sleep;
    return false;
  }

  _state = state;
  return true;
}

Drv8874::State Drv8874::state() const noexcept { return _state; }

bool Drv8874::hasFault() const noexcept {
  bool faultPinHigh = true;
  // Treat an unreadable active-low safety input as a fault.
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
