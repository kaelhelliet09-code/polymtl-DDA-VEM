// Implements bridge hardware control, immediate fault isolation, safety-event
// reporting, and wake qualification.
#include "Service/Power/CoilController.h"

#include "Config/PowerConfig.h"
#include "Config/SafetyConfig.h"
#include "Service/Power/CoilRequest.h"
#include "stm32g0xx_hal.h"

extern "C" {
#include "main.h"
extern DAC_HandleTypeDef hdac1;
}

namespace {

volatile uint32_t driverShutdownRequestedMask = 0U;
volatile uint32_t driverWakeQualificationMask = 0U;
dda::CoilController *activeController = nullptr;

constexpr uint32_t SafetyExtiMask =
    FAULT_H1_Pin | FAULT_H2_Pin | FAULT_H3_Pin | FAULT_H4_Pin | POWER_ALERT_Pin;

constexpr uint32_t faultBit(dda::Driver driver) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  return index < dda::config::DriverCount ? (1UL << index) : 0U;
}

constexpr uint32_t faultBit(dda::PowerStageFault fault) noexcept {
  return static_cast<uint32_t>(fault);
}

constexpr uint32_t DriverFaultMask = faultBit(dda::PowerStageFault::DriverH1) |
                                     faultBit(dda::PowerStageFault::DriverH2) |
                                     faultBit(dda::PowerStageFault::DriverH3) |
                                     faultBit(dda::PowerStageFault::DriverH4);

GPIO_TypeDef *const DriverEnablePorts[dda::config::DriverCount] = {
    SLEEP_H1_GPIO_Port,
    SLEEP_H2_GPIO_Port,
    SLEEP_H3_GPIO_Port,
    SLEEP_H4_GPIO_Port,
};

constexpr uint16_t DriverEnablePins[dda::config::DriverCount] = {
    SLEEP_H1_Pin,
    SLEEP_H2_Pin,
    SLEEP_H3_Pin,
    SLEEP_H4_Pin,
};

constexpr uint16_t DriverFaultPins[dda::config::DriverCount] = {
    FAULT_H1_Pin,
    FAULT_H2_Pin,
    FAULT_H3_Pin,
    FAULT_H4_Pin,
};

uint16_t faultPinForDriver(uint8_t index) noexcept {
  return index < dda::config::DriverCount ? DriverFaultPins[index] : 0U;
}

void forceDriverEnableLow(dda::Driver driver) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  if (index < dda::config::DriverCount) {
    DriverEnablePorts[index]->BSRR =
        static_cast<uint32_t>(DriverEnablePins[index]) << 16U;
  }
}

bool isEnergizedCommand(dda::Drv8874::State state) noexcept {
  return state == dda::Drv8874::State::Forward ||
         state == dda::Drv8874::State::Reverse;
}

bool selectedDriver(uint8_t options, dda::Driver &driver) noexcept {
  if (options > static_cast<uint8_t>(dda::BridgeOptions::H4)) {
    return false;
  }
  driver = static_cast<dda::Driver>(options);
  return true;
}

bool allDriversSelected(uint8_t options) noexcept {
  return options == static_cast<uint8_t>(dda::BridgeOptions::All);
}

} // namespace

namespace dda {

CoilController::CoilController(SafetyManager &safetyManager) noexcept
    : _drivers{
          {IN1_H1_GPIO_Port, IN1_H1_Pin, IN2_H1_GPIO_Port, IN2_H1_Pin,
           SLEEP_H1_GPIO_Port, SLEEP_H1_Pin, FAULT_H1_GPIO_Port, FAULT_H1_Pin},
          {IN1_H2_GPIO_Port, IN1_H2_Pin, IN2_H2_GPIO_Port, IN2_H2_Pin,
           SLEEP_H2_GPIO_Port, SLEEP_H2_Pin, FAULT_H2_GPIO_Port, FAULT_H2_Pin},
          {IN1_H3_GPIO_Port, IN1_H3_Pin, IN2_H3_GPIO_Port, IN2_H3_Pin,
           SLEEP_H3_GPIO_Port, SLEEP_H3_Pin, FAULT_H3_GPIO_Port, FAULT_H3_Pin},
          {IN1_H4_GPIO_Port, IN1_H4_Pin, IN2_H4_GPIO_Port, IN2_H4_Pin,
           SLEEP_H4_GPIO_Port, SLEEP_H4_Pin, FAULT_H4_GPIO_Port, FAULT_H4_Pin},
      },
      _currentControlH12(hdac1, DAC_CHANNEL_1),
      _currentControlH34(hdac1, DAC_CHANNEL_2),
      _requestedCurrentMilliamps(config::DefaultCoilCurrentMilliamps),
      _appliedCurrentMilliamps(0U), _safetyManager(safetyManager) {
  activeController = this;
}

HAL_StatusTypeDef CoilController::init() noexcept {
  forceBridgeEnablesLow();

  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    driverShutdownRequestedMask = 0U;
    driverWakeQualificationMask = 0U;
  }

  for (Drv8874 &driver : _drivers) {
    driver.init();
  }

  HAL_StatusTypeDef status = _currentControlH12.write(0U);
  if (status == HAL_OK) {
    status = _currentControlH34.write(0U);
  }
  if (status == HAL_OK) {
    status = _currentControlH12.start();
  }
  if (status == HAL_OK) {
    status = _currentControlH34.start();
    if (status != HAL_OK) {
      _currentControlH12.stop();
    }
  }

  _requestedCurrentMilliamps = config::DefaultCoilCurrentMilliamps;
  _appliedCurrentMilliamps = 0U;
  if (status != HAL_OK) {
    latchSafeStateFailure();
  }
  return status;
}

bool CoilController::setDriverState(Drv8874::State state) noexcept {
  if (!Drv8874::isValidState(state)) {
    return false;
  }
  if (state == Drv8874::State::Sleep) {
    return disableDriverGpios();
  }
  for (const Drv8874 &driver : _drivers) {
    if (driver.state() == Drv8874::State::Sleep) {
      return false;
    }
  }

  const bool energizedTransition = isEnergizedCommand(state);
  if (energizedTransition) {
    if ((_requestedCurrentMilliamps == 0U) || hasAnyFault()) {
      return false;
    }
    if (applyCurrentMilliamps(_requestedCurrentMilliamps) != HAL_OK) {
      (void)disableAll();
      return false;
    }
  }

  for (uint8_t index = 0U; index < DriverCount; ++index) {
    const bool stateApplied = energizedTransition
                                  ? setEnergizedDriverStateSafely(index, state)
                                  : _drivers[index].setState(state);
    if (!stateApplied) {
      (void)disableAll();
      return false;
    }
  }

  if (energizedTransition) {
    sampleAndLatchFaultInputs();
    if (hasAnyFault()) {
      (void)disableAll();
      return false;
    }
  }
  if ((state == Drv8874::State::CoilOff) &&
      (applyCurrentMilliamps(0U) != HAL_OK)) {
    (void)disableAll();
    return false;
  }
  return true;
}

bool CoilController::setDriverState(Driver driver,
                                    Drv8874::State state) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  if ((index >= DriverCount) || !Drv8874::isValidState(state)) {
    return false;
  }

  if (state == Drv8874::State::Sleep) {
    if (!_drivers[index].setState(state)) {
      return false;
    }
    return !allDriversDisabled() || (applyCurrentMilliamps(0U) == HAL_OK);
  }
  if (_drivers[index].state() == Drv8874::State::Sleep) {
    return false;
  }

  const bool energizedTransition = isEnergizedCommand(state);
  if (energizedTransition) {
    if ((_requestedCurrentMilliamps == 0U) || hasGlobalFault() ||
        hasFault(driver)) {
      return false;
    }
    if (applyCurrentMilliamps(_requestedCurrentMilliamps) != HAL_OK) {
      (void)disableAll();
      return false;
    }
  }

  const bool stateApplied = energizedTransition
                                ? setEnergizedDriverStateSafely(index, state)
                                : _drivers[index].setState(state);
  if (!stateApplied) {
    if (energizedTransition) {
      isolateAfterFailedEnable(index);
    } else {
      (void)_drivers[index].setState(Drv8874::State::Sleep);
    }
    return false;
  }

  if (energizedTransition) {
    sampleAndLatchFaultInputs();
    if (hasGlobalFault()) {
      (void)disableAll();
      return false;
    }
    if (hasFault(driver)) {
      (void)_drivers[index].setState(Drv8874::State::Sleep);
      return false;
    }
  }
  if (state == Drv8874::State::CoilOff) {
    bool anyDriverEnergized = false;
    for (const Drv8874 &other : _drivers) {
      anyDriverEnergized =
          anyDriverEnergized || isEnergizedCommand(other.state());
    }
    if (!anyDriverEnergized && (applyCurrentMilliamps(0U) != HAL_OK)) {
      (void)disableAll();
      return false;
    }
  }
  return true;
}

Drv8874::State CoilController::driverState(Driver driver) const noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  return index < DriverCount ? _drivers[index].state() : Drv8874::State::Sleep;
}

bool CoilController::setRequestedCurrentMilliamps(
    uint16_t currentMilliamps) noexcept {
  if ((currentMilliamps > config::MaximumCoilCurrentMilliamps) ||
      hasAnyFault() || (_appliedCurrentMilliamps != 0U)) {
    return false;
  }
  for (const Drv8874 &driver : _drivers) {
    if ((driver.state() != Drv8874::State::Sleep) &&
        (driver.state() != Drv8874::State::CoilOff)) {
      return false;
    }
  }

  _requestedCurrentMilliamps = currentMilliamps;
  return true;
}

uint16_t CoilController::requestedCurrentMilliamps() const noexcept {
  return _requestedCurrentMilliamps;
}

uint16_t CoilController::appliedCurrentMilliamps() const noexcept {
  return _appliedCurrentMilliamps;
}

bool CoilController::beginTestDriverWake(Driver driver) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  const uint32_t driverFault = faultBit(driver);
  if ((index >= DriverCount) || (driverFault == 0U) ||
      (_drivers[index].state() != Drv8874::State::Sleep) ||
      (_appliedCurrentMilliamps != 0U) || hasGlobalFault() ||
      hasFault(driver)) {
    return false;
  }
  for (const Drv8874 &other : _drivers) {
    if ((other.state() != Drv8874::State::Sleep) &&
        (other.state() != Drv8874::State::CoilOff)) {
      return false;
    }
  }

  const uint16_t driverFaultPin = faultPinForDriver(index);
  bool stateApplied = false;
  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    const bool safeToWake =
        !_safetyManager.hasDriverFault(driver) &&
        !_safetyManager.hasGlobalFault() &&
        _drivers[index].faultInputReleased() &&
        (__HAL_GPIO_EXTI_GET_FALLING_IT(driverFaultPin) == 0U);
    if (safeToWake) {
      driverWakeQualificationMask |= driverFault;
    }
    stateApplied =
        safeToWake && _drivers[index].setState(Drv8874::State::CoilOff);
    if (!stateApplied) {
      driverWakeQualificationMask &= ~driverFault;
    }
  }

  // In PH/EN mode EN low is a non-driving low-side brake. It is safe while
  // nSLEEP rises and the charge pump completes tWAKE.
  if (!stateApplied) {
    return false;
  }

  // HAL_Delay is blocking and uses the HAL SysTick timebase. Keep this
  // driver's nFAULT edge unclassified until the DRV8874 wake time expires.
  HAL_Delay(config::DriverWakeQualificationMilliseconds);
  return true;
}

bool CoilController::completeDriverWakeQualification(Driver driver) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  const uint32_t driverFault = faultBit(driver);
  if ((index >= DriverCount) || (driverFault == 0U) ||
      ((driverWakeQualificationMask & driverFault) == 0U)) {
    return false;
  }

  const uint16_t faultPin = faultPinForDriver(index);
  bool driverReleased = false;
  bool powerReleased = false;
  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    const bool driverReleasedBeforeClear = _drivers[index].faultInputReleased();
    const bool powerReleasedBeforeClear = _safetyManager.powerAlertReleased();

    // Discard only the edge captured while this driver was deliberately waking.
    // A persistent low or a new edge at the qualification boundary still fails.
    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(faultPin);
    driverWakeQualificationMask &= ~driverFault;

    driverReleased = driverReleasedBeforeClear &&
                     _drivers[index].faultInputReleased() &&
                     (__HAL_GPIO_EXTI_GET_FALLING_IT(faultPin) == 0U);
    powerReleased =
        powerReleasedBeforeClear && !_safetyManager.powerAlertFaultObserved();
  }

  if (!powerReleased) {
    forceBridgeEnablesLow();
    _safetyManager.handlePowerFault();
  }
  if (!driverReleased) {
    _drivers[index].onFaultInterrupt();
    latchDriverFault(driver);
  }
  if (!driverReleased || !powerReleased ||
      (_drivers[index].state() != Drv8874::State::CoilOff)) {
    (void)_drivers[index].setState(Drv8874::State::Sleep);
    return false;
  }
  return true;
}

HAL_StatusTypeDef CoilController::disableAll() noexcept {
  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    driverWakeQualificationMask = 0U;
  }
  forceBridgeEnablesLow();

  const bool disabled = disableDriverGpios();
  const HAL_StatusTypeDef currentStatus = applyCurrentMilliamps(0U);
  if (disabled && (currentStatus == HAL_OK)) {
    return HAL_OK;
  }

  latchSafeStateFailure();
  return currentStatus != HAL_OK ? currentStatus : HAL_ERROR;
}

bool CoilController::allDriversDisabled() const noexcept {
  for (const Drv8874 &driver : _drivers) {
    if (driver.state() != Drv8874::State::Sleep) {
      return false;
    }
  }
  return true;
}

bool CoilController::hasAnyFault() const noexcept {
  if (_safetyManager.hasAnyFault()) {
    return true;
  }
  for (uint8_t index = 0U; index < DriverCount; ++index) {
    if ((_drivers[index].state() != Drv8874::State::Sleep) &&
        ((driverWakeQualificationMask & (1UL << index)) == 0U) &&
        _drivers[index].hasFault()) {
      return true;
    }
  }
  return false;
}

bool CoilController::hasGlobalFault() const noexcept {
  return _safetyManager.hasGlobalFault();
}

bool CoilController::faultedDriversDisabled() const noexcept {
  const uint32_t driverFaults = _safetyManager.faultMask() & DriverFaultMask;
  for (uint8_t index = 0U; index < DriverCount; ++index) {
    if (((driverFaults & (1UL << index)) != 0U) &&
        (_drivers[index].state() != Drv8874::State::Sleep)) {
      return false;
    }
  }
  return true;
}

bool CoilController::hasFault(Driver driver) const noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  if (index >= DriverCount) {
    return false;
  }
  const uint32_t bit = faultBit(driver);
  return _safetyManager.hasDriverFault(driver) ||
         ((_drivers[index].state() != Drv8874::State::Sleep) &&
          ((driverWakeQualificationMask & bit) == 0U) &&
          _drivers[index].hasFault());
}

bool CoilController::clearFaultHardware(uint32_t expectedFaultEpoch) noexcept {
  if (!allDriversDisabled() || (_appliedCurrentMilliamps != 0U)) {
    return false;
  }

  auto criticalSection = _safetyManager.enterCriticalSection();
  bool released = (_safetyManager.faultEpoch() == expectedFaultEpoch) &&
                  (__HAL_GPIO_EXTI_GET_FALLING_IT(SafetyExtiMask) == 0U) &&
                  allFaultInputsReleased();
  if (released) {
    for (Drv8874 &driver : _drivers) {
      released = driver.clearFault() && released;
    }
  }
  released = released && (_safetyManager.faultEpoch() == expectedFaultEpoch) &&
             (__HAL_GPIO_EXTI_GET_FALLING_IT(SafetyExtiMask) == 0U) &&
             allFaultInputsReleased();
  if (released) {
    driverShutdownRequestedMask = 0U;
  }
  return released;
}

uint32_t CoilController::faultMask() const noexcept {
  return _safetyManager.faultMask();
}

uint32_t CoilController::faultEpoch() const noexcept {
  return _safetyManager.faultEpoch();
}

void CoilController::sampleAndLatchFaultInputs() noexcept {
  for (uint8_t index = 0U; index < DriverCount; ++index) {
    if ((_drivers[index].state() != Drv8874::State::Sleep) &&
        ((driverWakeQualificationMask & (1UL << index)) == 0U) &&
        !_drivers[index].faultInputReleased()) {
      _drivers[index].onFaultInterrupt();
      latchDriverFault(static_cast<Driver>(index));
    }
  }
  if (!_safetyManager.powerAlertReleased()) {
    forceBridgeEnablesLow();
    _safetyManager.handlePowerFault();
  }
}

bool CoilController::allFaultInputsReleased() const noexcept {
  for (const Drv8874 &driver : _drivers) {
    if (!driver.faultInputReleased()) {
      return false;
    }
  }
  return _safetyManager.powerAlertReleased();
}

HAL_StatusTypeDef CoilController::servicePendingDriverShutdowns() noexcept {
  uint32_t driverShutdowns = 0U;
  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    driverShutdowns = driverShutdownRequestedMask;
  }

  if (driverShutdowns == 0U) {
    return HAL_OK;
  }

  bool disabled = true;
  for (uint8_t index = 0U; index < DriverCount; ++index) {
    if ((driverShutdowns & (1UL << index)) != 0U) {
      disabled = _drivers[index].setState(Drv8874::State::Sleep) && disabled;
    }
  }
  if (!disabled) {
    latchSafeStateFailure();
    return HAL_ERROR;
  }

  if (allDriversDisabled() && (applyCurrentMilliamps(0U) != HAL_OK)) {
    latchSafeStateFailure();
    return HAL_ERROR;
  }

  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    driverShutdownRequestedMask &= ~driverShutdowns;
  }
  return HAL_OK;
}

void CoilController::latchDriverFault(Driver driver) noexcept {
  const uint32_t bit = faultBit(driver);
  if (bit == 0U) {
    return;
  }

  forceDriverEnableLow(driver);
  auto criticalSection = _safetyManager.enterCriticalSection();
  _safetyManager.handleDriverFault(driver);
  driverShutdownRequestedMask |= bit;
}

void CoilController::handleDriverFaultFromIsr(Driver driver) noexcept {
  const uint8_t index = static_cast<uint8_t>(driver);
  const uint32_t bit = faultBit(driver);
  if ((index >= DriverCount) || (bit == 0U) ||
      ((driverWakeQualificationMask & bit) != 0U) ||
      ((DriverEnablePorts[index]->ODR & DriverEnablePins[index]) == 0U)) {
    return;
  }
  if (activeController != nullptr) {
    activeController->latchDriverFault(driver);
  }
}

uint32_t CoilController::latchPendingSafetyEdges() noexcept {
  const uint32_t pending = __HAL_GPIO_EXTI_GET_FALLING_IT(SafetyExtiMask);
  if ((pending & FAULT_H1_Pin) != 0U) {
    handleDriverFaultFromIsr(Driver::H1);
  }
  if ((pending & FAULT_H2_Pin) != 0U) {
    handleDriverFaultFromIsr(Driver::H2);
  }
  if ((pending & FAULT_H3_Pin) != 0U) {
    handleDriverFaultFromIsr(Driver::H3);
  }
  if ((pending & FAULT_H4_Pin) != 0U) {
    handleDriverFaultFromIsr(Driver::H4);
  }
  if ((pending & POWER_ALERT_Pin) != 0U) {
    handlePowerAlertFromIsr();
  }
  return pending;
}

void CoilController::handlePowerAlertFromIsr() noexcept {
  forceBridgeEnablesLow();
  if (activeController != nullptr) {
    activeController->_safetyManager.handlePowerFault();
  }
}

void CoilController::forceBridgeEnablesLow() noexcept {
  constexpr uint32_t allEnablePins =
      SLEEP_H1_Pin | SLEEP_H2_Pin | SLEEP_H3_Pin | SLEEP_H4_Pin;
  SLEEP_H1_GPIO_Port->BSRR = allEnablePins << 16U;
}

void CoilController::latchSafeStateFailure() noexcept {
  forceBridgeEnablesLow();
  if (activeController != nullptr) {
    activeController->_safetyManager.handleSafeStateFailure();
  }
}

HAL_StatusTypeDef
CoilController::applyCurrentMilliamps(uint16_t currentMilliamps) noexcept {
  const uint16_t dacValue =
      config::currentMilliampsToReferenceCode(currentMilliamps);

  const HAL_StatusTypeDef h12Status = _currentControlH12.write(dacValue);
  const HAL_StatusTypeDef h34Status = _currentControlH34.write(dacValue);
  if ((h12Status == HAL_OK) && (h34Status == HAL_OK)) {
    _appliedCurrentMilliamps = currentMilliamps;
    return HAL_OK;
  }

  const HAL_StatusTypeDef zeroH12 = _currentControlH12.write(0U);
  const HAL_StatusTypeDef zeroH34 = _currentControlH34.write(0U);
  if ((zeroH12 == HAL_OK) && (zeroH34 == HAL_OK)) {
    _appliedCurrentMilliamps = 0U;
  }
  return h12Status != HAL_OK ? h12Status : h34Status;
}

bool CoilController::disableDriverGpios() noexcept {
  bool disabled = true;
  for (Drv8874 &driver : _drivers) {
    disabled = driver.setState(Drv8874::State::Sleep) && disabled;
  }
  return disabled;
}

bool CoilController::driverEnableInputsAreSafe(uint8_t index) const noexcept {
  return _drivers[index].faultInputReleased() &&
         (__HAL_GPIO_EXTI_GET_IT(faultPinForDriver(index)) == 0U);
}

bool CoilController::setEnergizedDriverStateSafely(
    uint8_t index, Drv8874::State state) noexcept {
  const Driver driver = static_cast<Driver>(index);
  auto criticalSection = _safetyManager.enterCriticalSection();
  if (!_safetyManager.canEnergize(driver) ||
      !driverEnableInputsAreSafe(index)) {
    return false;
  }
  if (!_drivers[index].setState(state)) {
    return false;
  }
  return verifyEnabledDriverOrIsolate(index);
}

bool CoilController::verifyEnabledDriverOrIsolate(uint8_t index) noexcept {
  const uint16_t driverFaultPin = faultPinForDriver(index);
  const bool powerFaultObserved = _safetyManager.hasGlobalFault();
  const bool driverFaultObserved =
      !_drivers[index].faultInputReleased() ||
      (__HAL_GPIO_EXTI_GET_IT(driverFaultPin) != 0U);

  if (!powerFaultObserved && !driverFaultObserved) {
    return true;
  }

  if (powerFaultObserved) {
    forceBridgeEnablesLow();
  } else {
    forceDriverEnableLow(static_cast<Driver>(index));
  }
  return false;
}

void CoilController::isolateAfterFailedEnable(uint8_t index) noexcept {
  if (_safetyManager.hasGlobalFault()) {
    (void)disableAll();
    return;
  }
  (void)_drivers[index].setState(Drv8874::State::Sleep);
}

void CoilController::processRequest(Request &request) noexcept {
  if (request.destination != Service::CoilControl) {
    return;
  }

  bool succeeded = false;
  Driver driver = Driver::H1;
  switch (static_cast<CoilCommand>(request.command)) {
  case CoilCommand::Forward:
    succeeded = allDriversSelected(request.options)
                    ? setDriverState(Drv8874::State::Forward)
                    : selectedDriver(request.options, driver) &&
                          setDriverState(driver, Drv8874::State::Forward);
    break;
  case CoilCommand::Reverse:
    succeeded = allDriversSelected(request.options)
                    ? setDriverState(Drv8874::State::Reverse)
                    : selectedDriver(request.options, driver) &&
                          setDriverState(driver, Drv8874::State::Reverse);
    break;
  case CoilCommand::Sleep:
    succeeded = allDriversSelected(request.options)
                    ? disableAll() == HAL_OK
                    : selectedDriver(request.options, driver) &&
                          setDriverState(driver, Drv8874::State::Sleep);
    break;
  case CoilCommand::Wake: {
    const uint8_t first =
        allDriversSelected(request.options) ? 0U : request.options;
    const uint8_t end = allDriversSelected(request.options)
                            ? DriverCount
                            : static_cast<uint8_t>(first + 1U);
    succeeded = end <= DriverCount;
    for (uint8_t index = first; succeeded && (index < end); ++index) {
      driver = static_cast<Driver>(index);
      succeeded = (driverState(driver) != Drv8874::State::Sleep) ||
                  (beginTestDriverWake(driver) &&
                   completeDriverWakeQualification(driver));
    }
    break;
  }
  case CoilCommand::SetCurrent:
    succeeded =
        setRequestedCurrentMilliamps(static_cast<uint16_t>(request.options) *
                                     CoilCurrentOptionStepMilliamps);
    break;
  case CoilCommand::GetFaults:
    request.options = static_cast<uint8_t>(faultMask() & 0x7FU);
    request.complete(Service::CoilControl);
    return;
  case CoilCommand::CoilOff:
    succeeded = allDriversSelected(request.options)
                    ? setDriverState(Drv8874::State::CoilOff)
                    : selectedDriver(request.options, driver) &&
                          setDriverState(driver, Drv8874::State::CoilOff);
    break;
  default:
    break;
  }

  request.options = static_cast<uint8_t>(succeeded ? CoilStatus::Succeeded
                                                   : CoilStatus::Failed);
  request.complete(Service::CoilControl);
}

} // namespace dda
