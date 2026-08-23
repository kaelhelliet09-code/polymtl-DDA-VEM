#include "Service/Safety/SafetyManager.h"

#include "Config/BoardConfig.h"
#include "Config/SafetyConfig.h"
#include "Platform/Stm32/Gpio/GpioPin.h"
#include "Platform/Stm32/System/InterruptEvents.h"

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace {

constexpr uint32_t faultBit(dda::Driver driver) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  return index < dda::config::DriverCount ? (1UL << index) : 0U;
}

constexpr uint32_t faultBit(dda::PowerStageFault fault) noexcept {
  return static_cast<uint32_t>(fault);
}

constexpr uint32_t GlobalFaultMask =
    faultBit(dda::PowerStageFault::PowerAlert) |
    faultBit(dda::PowerStageFault::SafeStateFailure);

} // namespace

namespace dda {

SafetyManager::SafetyManager(RequestManager &requestManager,
                             GpioPin &powerAlertPin) noexcept
    : _requestManager(requestManager), _powerAlertPin(powerAlertPin),
      _faultMask(0U), _faultEpoch(0U), _faultReportPending(false),
      _state(SystemState::Initializing), _systemReady(false),
      _powerAlertConfigured(false), _faultReleaseTiming(false),
      _safeStateRequired(false), _faultSafeStateComplete(false),
      _powerAlertCycleActive(false), _powerAlertRequiresAcknowledgement(false),
      _powerAlertRepeatWindowActive(false), _faultReleasedSinceMilliseconds(0U),
      _faultReleaseEpoch(0U), _powerAlertRepeatWindowStartedMilliseconds(0U),
      _automaticClearPending(false), _automaticClearEpoch(0U) {
  _requestManager.registerService(Service::Safety, *this);
}

SafetyManager::CriticalSection
SafetyManager::enterCriticalSection() const noexcept {
  return CriticalSection{};
}

void SafetyManager::resetForInitialization() noexcept {
  auto criticalSection = enterCriticalSection();
  _faultMask = 0U;
  _faultEpoch = 0U;
  _faultReportPending = false;
  _state = SystemState::Initializing;
  _systemReady = false;
  _safeStateRequired = false;
  _powerAlertConfigured = false;
  _powerAlertCycleActive = false;
  _powerAlertRequiresAcknowledgement = false;
  _powerAlertRepeatWindowActive = false;
  _powerAlertRepeatWindowStartedMilliseconds = 0U;
  _automaticClearPending = false;
  _automaticClearEpoch = 0U;
  resetFaultRecoveryTracking();
}

void SafetyManager::setPowerAlertConfigured(bool configured) noexcept {
  _powerAlertConfigured = configured;
}

void SafetyManager::setSystemReady(bool ready) noexcept {
  _systemReady = ready;
  if (hasAnyFault()) {
    _state = SystemState::Faulted;
  } else {
    _state = ready ? SystemState::Ready : SystemState::Safe;
  }
}

SystemState SafetyManager::getState() const noexcept {
  auto criticalSection = enterCriticalSection();
  return _state;
}

bool SafetyManager::isPowerStageReady() const noexcept {
  auto criticalSection = enterCriticalSection();
  return _state == SystemState::Ready && _powerAlertConfigured &&
         (_faultMask == 0U) && !powerAlertFaultObserved();
}

bool SafetyManager::canEnergize(Driver driver) const noexcept {
  const uint32_t driverFault = faultBit(driver);
  if (driverFault == 0U) {
    return false;
  }
  auto criticalSection = enterCriticalSection();
  const bool operationalState =
      (_state == SystemState::Ready) ||
      ((_state == SystemState::Faulted) && (_faultMask != 0U));
  return _systemReady && operationalState && _powerAlertConfigured &&
         ((_faultMask & (GlobalFaultMask | driverFault)) == 0U) &&
         !powerAlertFaultObserved();
}

bool SafetyManager::powerAlertConfigured() const noexcept {
  return _powerAlertConfigured;
}

bool SafetyManager::powerAlertReleased() const noexcept {
  bool released = false;
  return _powerAlertPin.read(released) && released;
}

bool SafetyManager::powerAlertFaultObserved() const noexcept {
  return !powerAlertReleased() ||
         (__HAL_GPIO_EXTI_GET_FALLING_IT(_powerAlertPin.pin()) != 0U);
}

bool SafetyManager::recordFault(uint32_t mask) noexcept {
  if (mask == 0U) {
    return false;
  }

  auto criticalSection = enterCriticalSection();
  if ((_faultMask & mask) == mask) {
    return false;
  }
  _faultMask |= mask;
  ++_faultEpoch;
  if (_faultEpoch == 0U) {
    ++_faultEpoch;
  }
  _faultReportPending = true;
  _state = SystemState::Faulted;
  return true;
}

void SafetyManager::handleDriverFault(Driver driver) noexcept {
  (void)recordFault(faultBit(driver));
}

void SafetyManager::handlePowerFault() noexcept {
  if (recordFault(faultBit(PowerStageFault::PowerAlert))) {
    _safeStateRequired = true;
    _faultSafeStateComplete = false;
  }
}

void SafetyManager::handleSafeStateFailure() noexcept {
  (void)recordFault(faultBit(PowerStageFault::SafeStateFailure));
  _safeStateRequired = true;
  _faultSafeStateComplete = false;
}

bool SafetyManager::safeStateRequired() const noexcept {
  auto criticalSection = enterCriticalSection();
  return _safeStateRequired;
}

void SafetyManager::reportSafeStateResult(bool success) noexcept {
  if (!success) {
    handleSafeStateFailure();
    return;
  }

  auto criticalSection = enterCriticalSection();
  _safeStateRequired = false;
  _faultSafeStateComplete = true;
  _state = _faultMask != 0U ? SystemState::Faulted : SystemState::Safe;
}

void SafetyManager::process(const SafetySnapshot &snapshot) noexcept {
  const bool safetyEventPending = takeSafetyInterruptEvent();
  if (safetyEventPending) {
    resetFaultRecoveryTracking();
  }

  const uint32_t nowMilliseconds = HAL_GetTick();
  const uint32_t faults = faultMask();
  const bool isolatedPowerAlert =
      faults == faultBit(PowerStageFault::PowerAlert);

  updatePowerAlertCycle(safetyEventPending, faults, nowMilliseconds);
  updateSafeStatePolicy(snapshot);
  updateFaultReleaseValidation(snapshot, nowMilliseconds);
  updateAutomaticClearRequest(isolatedPowerAlert);
  queueFaultReport();
}

void SafetyManager::updatePowerAlertCycle(bool safetyEventPending,
                                          uint32_t faults,
                                          uint32_t nowMilliseconds) noexcept {
  if (_powerAlertRepeatWindowActive &&
      ((nowMilliseconds - _powerAlertRepeatWindowStartedMilliseconds) >=
       config::PowerAlertRepeatWindowMilliseconds)) {
    _powerAlertRepeatWindowActive = false;
  }

  const bool isolatedPowerAlert =
      faults == faultBit(PowerStageFault::PowerAlert);
  if (isolatedPowerAlert) {
    if (!_powerAlertCycleActive) {
      _powerAlertCycleActive = true;
      _powerAlertRequiresAcknowledgement = _powerAlertRepeatWindowActive;
      if (_powerAlertRequiresAcknowledgement) {
        _powerAlertRepeatWindowActive = false;
      }
    } else if (safetyEventPending) {
      _powerAlertRequiresAcknowledgement = true;
      _powerAlertRepeatWindowActive = false;
    }
  } else if (faults != 0U) {
    _powerAlertRequiresAcknowledgement = true;
    _powerAlertRepeatWindowActive = false;
  }
}

void SafetyManager::updateSafeStatePolicy(
    const SafetySnapshot &snapshot) noexcept {
  if (hasGlobalFault()) {
    if (!_faultSafeStateComplete) {
      _safeStateRequired = true;
    }
  } else if (hasAnyFault()) {
    _safeStateRequired = false;
    _faultSafeStateComplete = snapshot.faultedDriversDisabled;
  }
}

void SafetyManager::updateFaultReleaseValidation(
    const SafetySnapshot &snapshot, uint32_t nowMilliseconds) noexcept {
  const bool released =
      snapshot.faultInputsReleased && snapshot.powerStageOutputsSafe;
  if (!released) {
    _faultReleaseTiming = false;
    _faultReleasedSinceMilliseconds = 0U;
    _faultReleaseEpoch = 0U;
  } else if (!_faultReleaseTiming) {
    _faultReleaseTiming = true;
    _faultReleasedSinceMilliseconds = nowMilliseconds;
    _faultReleaseEpoch = faultEpoch();
  }
}

void SafetyManager::updateAutomaticClearRequest(
    bool isolatedPowerAlert) noexcept {
  const bool releaseValidated =
      _faultReleaseTiming &&
      ((HAL_GetTick() - _faultReleasedSinceMilliseconds) >=
       config::FaultReleaseValidationMilliseconds);
  if (isolatedPowerAlert && !_powerAlertRequiresAcknowledgement &&
      _faultSafeStateComplete && releaseValidated && _powerAlertConfigured) {
    _automaticClearPending = true;
    _automaticClearEpoch = _faultReleaseEpoch;
  }
}

bool SafetyManager::prepareManualFaultClear(uint32_t &faultEpoch) noexcept {
  auto criticalSection = enterCriticalSection();
  const bool safetyEventPending = takeSafetyInterruptEvent();
  if (safetyEventPending) {
    resetFaultRecoveryTracking();
  }
  const bool guardsPassed =
      !safetyEventPending && _powerAlertConfigured && _faultSafeStateComplete &&
      _faultReleaseTiming &&
      ((HAL_GetTick() - _faultReleasedSinceMilliseconds) >=
       config::FaultReleaseValidationMilliseconds);
  if (guardsPassed) {
    faultEpoch = _faultReleaseEpoch;
    resetFaultRecoveryTracking();
  }
  return guardsPassed;
}

bool SafetyManager::takeAutomaticFaultClearRequest(
    uint32_t &faultEpoch) noexcept {
  auto criticalSection = enterCriticalSection();
  if (!_automaticClearPending) {
    return false;
  }
  faultEpoch = _automaticClearEpoch;
  _automaticClearPending = false;
  resetFaultRecoveryTracking();
  return true;
}

bool SafetyManager::completeFaultClear(uint32_t expectedFaultEpoch,
                                       bool hardwareCleared,
                                       bool automatic) noexcept {
  auto criticalSection = enterCriticalSection();
  if (!hardwareCleared || (_faultEpoch != expectedFaultEpoch)) {
    return false;
  }

  _faultMask = 0U;
  _faultReportPending = false;
  _safeStateRequired = false;
  _faultSafeStateComplete = false;
  _powerAlertCycleActive = false;
  _powerAlertRequiresAcknowledgement = false;
  _powerAlertRepeatWindowActive = automatic;
  _powerAlertRepeatWindowStartedMilliseconds = automatic ? HAL_GetTick() : 0U;
  _state = _systemReady ? SystemState::Ready : SystemState::Safe;
  return true;
}

uint32_t SafetyManager::faultMask() const noexcept {
  auto criticalSection = enterCriticalSection();
  return _faultMask;
}

uint32_t SafetyManager::faultEpoch() const noexcept {
  auto criticalSection = enterCriticalSection();
  return _faultEpoch;
}

bool SafetyManager::hasAnyFault() const noexcept {
  return (faultMask() != 0U) || powerAlertFaultObserved();
}

bool SafetyManager::hasGlobalFault() const noexcept {
  return ((faultMask() & GlobalFaultMask) != 0U) || powerAlertFaultObserved();
}

bool SafetyManager::hasDriverFault(Driver driver) const noexcept {
  const uint32_t bit = faultBit(driver);
  return (bit != 0U) && ((faultMask() & bit) != 0U);
}

void SafetyManager::processRequest(Request &request) noexcept {
  switch (static_cast<SafetyCommand>(request.command)) {
  case SafetyCommand::GetState:
    request.options = static_cast<uint8_t>(getState());
    break;
  case SafetyCommand::GetFaultMask:
    request.options = static_cast<uint8_t>(faultMask() & 0x7FU);
    break;
  case SafetyCommand::ReportFault:
  default:
    request.complete(Service::Safety);
    return;
  }
  request.complete(Service::Safety);
}

void SafetyManager::resetFaultRecoveryTracking() noexcept {
  _faultSafeStateComplete = false;
  _faultReleaseTiming = false;
  _faultReleasedSinceMilliseconds = 0U;
  _faultReleaseEpoch = 0U;
  _automaticClearPending = false;
  _automaticClearEpoch = 0U;
}

void SafetyManager::queueFaultReport() noexcept {
  uint32_t mask = 0U;
  uint32_t epoch = 0U;
  {
    auto criticalSection = enterCriticalSection();
    if (!_faultReportPending) {
      return;
    }
    mask = _faultMask;
    epoch = _faultEpoch;
  }

  Request report{};
  report.destination = Service::UsbControl;
  report.source = Service::Safety;
  report.command = static_cast<uint8_t>(SafetyCommand::ReportFault);
  report.options = static_cast<uint8_t>(mask & 0x7FU);
  report.state = RequestState::Outgoing;
  if (!_requestManager.queueRequest(report)) {
    return;
  }

  auto criticalSection = enterCriticalSection();
  if (_faultEpoch == epoch) {
    _faultReportPending = false;
  }
}

} // namespace dda
