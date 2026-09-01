// Implements ordered board startup, cooperative dispatch, and safe-state
// transitions for the revised DDA hardware.
#include "App/BoardApplication.h"

#include "Config/BoardConfig.h"
#include "Config/PowerConfig.h"
#include "Config/SafetyConfig.h"
#include "Platform/Stm32/System/InterruptEvents.h"
#include "Platform/Stm32/System/Timestamp.h"
#include "Platform/Stm32/Usb/UsbCdcBridge.h"

extern "C" {
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim15;
}

namespace dda {

BoardApplication::BoardApplication() noexcept
    : _statusLedPin(STATUS_LED_1_GPIO_Port, STATUS_LED_1_Pin,
                    GpioDirection::OUTPUT),
      _externalDacData(DAC_DIN_GPIO_Port, DAC_DIN_Pin, GpioDirection::OUTPUT),
      _externalDacSync(DAC_SYNC_GPIO_Port, DAC_SYNC_Pin, GpioDirection::OUTPUT),
      _externalDacClock(DAC_SCLK_GPIO_Port, DAC_SCLK_Pin,
                        GpioDirection::OUTPUT),
      _powerAlert(POWER_ALERT_GPIO_Port, POWER_ALERT_Pin, GpioDirection::INPUT),
      _userButton(USER_IN1_GPIO_Port, USER_IN1_Pin, GpioDirection::INPUT),
      _pmode(PMODE_GPIO_Port, PMODE_Pin, GpioDirection::OUTPUT),
      _userInput(_userButton), _statusLed(_statusLedPin),
      _externalDac(_externalDacData, _externalDacSync, _externalDacClock),
      _requestManager{},
      _sensorController(_externalDac, _requestManager),
      _safetyManager(_requestManager, _powerAlert),
      _coilController(_safetyManager, _externalDac, _pmode),
      _powerMonitor(
          hi2c1,
          static_cast<float>(config::ShuntResistanceMicroOhms) / 1'000'000.0F,
          static_cast<float>(config::Ina226CalibrationCurrentRangeMilliamps) /
              1'000.0F),
      _usbController(_requestManager),
      _launchManager(_requestManager, _sensorController, _coilController, hadc1,
                     _powerMonitor, _usbController, htim15, htim4, htim7),
      _initializationFailure(BoardInitializationFailure::None),
      _initialized(false) {
  _requestManager.registerService(Service::CoilControl, _coilController);
}

HAL_StatusTypeDef BoardApplication::init() noexcept {
  _initializationFailure = BoardInitializationFailure::None;
  _initialized = false;
  _safetyManager.resetForInitialization();
  _statusLed.setFaultLatched(false);

  HAL_StatusTypeDef firstFailure = timestamp.start();
  if (firstFailure != HAL_OK) {
    _initializationFailure = BoardInitializationFailure::Timestamp;
  }

  HAL_StatusTypeDef externalDacStatus =
      _externalDac.init(config::PeripheralOperationTimeoutMilliseconds);
  if (externalDacStatus == HAL_OK) {
    externalDacStatus =
        _externalDac.writeAll(config::SafeStartupDacCode,
                              config::PeripheralOperationTimeoutMilliseconds);
  }
  if ((firstFailure == HAL_OK) && (externalDacStatus != HAL_OK)) {
    firstFailure = externalDacStatus;
    _initializationFailure = BoardInitializationFailure::ExternalDac;
  }

  const HAL_StatusTypeDef coilStatus = _coilController.init();
  if ((firstFailure == HAL_OK) && (coilStatus != HAL_OK)) {
    firstFailure = coilStatus;
    _initializationFailure = BoardInitializationFailure::CoilController;
  }

  const HAL_StatusTypeDef sensorStatus =
      _sensorController.clearSensorDacOutputs();
  if ((firstFailure == HAL_OK) && (sensorStatus != HAL_OK)) {
    firstFailure = sensorStatus;
    _initializationFailure = BoardInitializationFailure::ExternalDac;
  }

  const HAL_StatusTypeDef monitorStatus = _powerMonitor.init();
  HAL_StatusTypeDef alertStatus = monitorStatus;
  if (monitorStatus == HAL_OK) {
    alertStatus = _powerMonitor.configurePowerOverLimitAlert(
        config::PowerAlertThresholdMilliwatts,
        config::PeripheralOperationTimeoutMilliseconds);
    _safetyManager.setPowerAlertConfigured(alertStatus == HAL_OK);
  }
  if (firstFailure == HAL_OK) {
    if (monitorStatus != HAL_OK) {
      firstFailure = monitorStatus;
      _initializationFailure = BoardInitializationFailure::PowerMonitor;
    } else if (alertStatus != HAL_OK) {
      firstFailure = alertStatus;
      _initializationFailure = BoardInitializationFailure::PowerAlert;
    }
  }

  const HAL_StatusTypeDef acquisitionStatus = _launchManager.init();
  if ((firstFailure == HAL_OK) && (acquisitionStatus != HAL_OK)) {
    firstFailure = acquisitionStatus;
    _initializationFailure = BoardInitializationFailure::Acquisition;
  }

  const HAL_StatusTypeDef safeStatus = enterSafeState();
  if ((firstFailure == HAL_OK) && (safeStatus != HAL_OK)) {
    firstFailure = safeStatus;
    _initializationFailure = BoardInitializationFailure::SafeState;
  }

  _initialized = firstFailure == HAL_OK;
  _safetyManager.setSystemReady(_initialized);
  _usbController.init();
  bindUsbTransport(_usbController.transport());

  initializeInterruptEvents(_sensorController);
  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    HAL_NVIC_ClearPendingIRQ(EXTI0_1_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI4_15_IRQn);

    const uint32_t capturedSafetyEdges =
        CoilController::latchPendingSafetyEdges();
    constexpr uint32_t sensorEdgeMask =
        SENSOR_1_Pin | SENSOR_2_Pin | SENSOR_3_Pin | SENSOR_4_Pin;
    const uint32_t capturedSensorEdges =
        __HAL_GPIO_EXTI_GET_RISING_IT(sensorEdgeMask) |
        __HAL_GPIO_EXTI_GET_FALLING_IT(sensorEdgeMask);
    __HAL_GPIO_EXTI_CLEAR_IT(capturedSafetyEdges | capturedSensorEdges);
    _coilController.sampleAndLatchFaultInputs();
    if (_coilController.servicePendingDriverShutdowns() != HAL_OK) {
      _safetyManager.handleSafeStateFailure();
    }
    enableSafetyInterrupts();
  }
  updateFaultIndicator();
  return firstFailure;
}

HAL_StatusTypeDef
BoardApplication::enterSafeState(bool updateFaultIndicator) noexcept {
  _launchManager.abortForSafety();
  HAL_StatusTypeDef firstFailure = _coilController.disableAll();

  const HAL_StatusTypeDef abortStatus = _externalDac.abort();
  if ((firstFailure == HAL_OK) && (abortStatus != HAL_OK)) {
    firstFailure = abortStatus;
  }

  const HAL_StatusTypeDef sensorStatus =
      _sensorController.clearSensorDacOutputs();
  if ((firstFailure == HAL_OK) && (sensorStatus != HAL_OK)) {
    firstFailure = sensorStatus;
  }

  const bool outputsSafe = _coilController.allDriversDisabled() &&
                           _coilController.allCurrentThresholdOutputsDisabled();
  const bool success = outputsSafe && (firstFailure == HAL_OK);
  _safetyManager.reportSafeStateResult(success);
  if (updateFaultIndicator) {
    this->updateFaultIndicator();
  }
  if (!success && (firstFailure == HAL_OK)) {
    return HAL_ERROR;
  }
  return firstFailure;
}

void BoardApplication::processSafety() noexcept {
  _coilController.sampleAndLatchFaultInputs();
  if (_coilController.servicePendingDriverShutdowns() != HAL_OK) {
    _safetyManager.handleSafeStateFailure();
  }

  const SafetySnapshot snapshot{
      _coilController.allDriversDisabled() &&
          _coilController.allCurrentThresholdOutputsDisabled(),
      _coilController.faultedDriversDisabled(),
      _coilController.allFaultInputsReleased(),
  };
  _safetyManager.process(snapshot);

  if (_safetyManager.safeStateRequired()) {
    (void)enterSafeState(false);
  }

  uint32_t faultEpoch = 0U;
  if (_safetyManager.takeAutomaticFaultClearRequest(faultEpoch)) {
    const bool hardwareCleared = _coilController.clearFaultHardware(faultEpoch);
    (void)_safetyManager.completeFaultClear(faultEpoch, hardwareCleared, true);
  }
  updateFaultIndicator();
}

void BoardApplication::process() noexcept {
  processSafety();
  _usbController.process();
  _launchManager.process();
  if (_launchManager.takeSystemResetRequest()) {
    (void)enterSafeState();
    NVIC_SystemReset();
    return;
  }

  // USER_IN1 intentionally has no DDA V2 application action.
  _requestManager.process();
  _usbController.processLaunchDataTransfer();
}

bool BoardApplication::setRequestedCoilCurrentMilliamps(
    uint16_t currentMilliamps) noexcept {
  return powerStageReadyForCommands() &&
         _coilController.setRequestedCurrentMilliamps(currentMilliamps);
}

bool BoardApplication::clearFault() noexcept {
  uint32_t faultEpoch = 0U;
  if (!_safetyManager.prepareManualFaultClear(faultEpoch)) {
    return false;
  }
  const bool hardwareCleared = _coilController.clearFaultHardware(faultEpoch);
  const bool cleared =
      _safetyManager.completeFaultClear(faultEpoch, hardwareCleared, false);
  updateFaultIndicator();
  return cleared;
}

bool BoardApplication::powerStageReadyForCommands() const noexcept {
  return _initialized && _safetyManager.isPowerStageReady();
}

bool BoardApplication::powerAlertConfigured() const noexcept {
  return _safetyManager.powerAlertConfigured();
}

SystemState BoardApplication::systemState() const noexcept {
  return _safetyManager.getState();
}

bool BoardApplication::isInitialized() const noexcept { return _initialized; }

BoardInitializationFailure
BoardApplication::initializationFailure() const noexcept {
  return _initializationFailure;
}

StatusLed &BoardApplication::statusLed() noexcept { return _statusLed; }
Dac088s085 &BoardApplication::externalDac() noexcept { return _externalDac; }
SensorController &BoardApplication::sensorController() noexcept {
  return _sensorController;
}
CoilController &BoardApplication::coilController() noexcept {
  return _coilController;
}

void BoardApplication::updateFaultIndicator() noexcept {
  _statusLed.setFaultLatched(_safetyManager.faultMask() != 0U);
}

void BoardApplication::enableSafetyInterrupts() noexcept {
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0U, 0U);
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

BoardApplication &boardApplication() noexcept {
  static BoardApplication application;
  return application;
}

} // namespace dda

extern "C" void DdaApplication_Initialize(void) {
  (void)dda::boardApplication().init();
}

extern "C" void DdaApplication_Process(void) {
  dda::boardApplication().process();
}
