// Implements the board composition root, ordered startup, cooperative service
// loop, and guarded transitions into and out of the latched safe state.
#include "App/BoardApplication.h"

#include "Config/BoardConfig.h"
#include "Config/PowerConfig.h"
#include "Config/SafetyConfig.h"
#include "Drivers/Drv8874.h"
#include "Platform/Stm32/System/InterruptEvents.h"
#include "Platform/Stm32/System/Timestamp.h"
#include "Platform/Stm32/Usb/UsbCdcBridge.h"
#include "Service/Power/CoilController.h"
#include "Service/Sensor/SensorController.h"
#include "Service/UI/UserInput.h"

extern "C" {
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim15;
}

namespace dda {

BoardApplication::BoardApplication() noexcept
    : _led1(STATUS_LED_1_GPIO_Port, STATUS_LED_1_Pin, GpioDirection::OUTPUT),
      _led2(STATUS_LED_2_GPIO_Port, STATUS_LED_2_Pin, GpioDirection::OUTPUT),
      _led3(STATUS_LED_3_GPIO_Port, STATUS_LED_3_Pin, GpioDirection::OUTPUT),
      _externalDacChipSelect(SPI1_CS_1_GPIO_Port, SPI1_CS_1_Pin,
                             GpioDirection::OUTPUT),
      _powerAlert(POWER_ALERT_GPIO_Port, POWER_ALERT_Pin, GpioDirection::INPUT),
      _userInput1(USER_IN1_GPIO_Port, USER_IN1_Pin, GpioDirection::INPUT),
      _userInput2(USER_IN2_GPIO_Port, USER_IN2_Pin, GpioDirection::INPUT),
      _userInput3(USER_IN3_GPIO_Port, USER_IN3_Pin, GpioDirection::INPUT),
      _userInput(_userInput1, _userInput2, _userInput3), _display(hi2c1),
      _statusLeds(&_led1, &_led2, &_led3),
      _externalDac(hspi1, _externalDacChipSelect), _calibrationStore{},
      _requestManager{},
      _sensorController(_externalDac, _calibrationStore, _requestManager),
      _safetyManager(_requestManager, _powerAlert),
      _coilController(_safetyManager),
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

  _statusLeds.setState(LedIndex::LED1, LedState::Off);
  _statusLeds.setState(LedIndex::LED2, LedState::Off);
  _statusLeds.setState(LedIndex::LED3, LedState::Off);

  HAL_StatusTypeDef firstFailure = timestamp.start();
  if (firstFailure != HAL_OK) {
    _initializationFailure = BoardInitializationFailure::Timestamp;
  }

  const HAL_StatusTypeDef coilStatus = _coilController.init();
  if ((firstFailure == HAL_OK) && (coilStatus != HAL_OK)) {
    firstFailure = coilStatus;
    _initializationFailure = BoardInitializationFailure::CoilController;
  }

  HAL_StatusTypeDef externalDacStatus =
      _externalDac.init(config::PeripheralOperationTimeoutMilliseconds);
  if (externalDacStatus == HAL_OK) {
    externalDacStatus = _sensorController.clearSensorDacOutputs();
  }
  if ((firstFailure == HAL_OK) && (externalDacStatus != HAL_OK)) {
    firstFailure = externalDacStatus;
    _initializationFailure = BoardInitializationFailure::ExternalDac;
  }

  // Storage state is diagnostic and may legitimately be empty or read-only.
  (void)_sensorController.initializeCalibrationStore();

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

  (void)_display.initAutoDetect();
  _initialized = firstFailure == HAL_OK;
  _safetyManager.setSystemReady(_initialized);

  _usbController.init();
  bindUsbTransport(_usbController.transport());

  // Publish process-lifetime targets before enabling any application IRQ.
  initializeInterruptEvents(_sensorController);
  {
    auto criticalSection = _safetyManager.enterCriticalSection();
    HAL_NVIC_ClearPendingIRQ(EXTI0_1_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI4_15_IRQn);

    // Input edges can occur while the shared NVIC vectors are intentionally
    // deferred. Acknowledge only the edge flags actually captured; a later
    // hardware edge must remain pending for delivery after NVIC enable.
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
  return firstFailure;
}

HAL_StatusTypeDef BoardApplication::enterSafeState(bool turnLedsOff) noexcept {
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

  if (turnLedsOff) {
    _statusLeds.setState(LedIndex::LED1, LedState::Off);
    _statusLeds.setState(LedIndex::LED2, LedState::Off);
    _statusLeds.setState(LedIndex::LED3, LedState::Off);
  }

  const bool outputsSafe = _coilController.allDriversDisabled() &&
                           (_coilController.appliedCurrentMilliamps() ==
                            config::SafeStartupCurrentMilliamps);
  const bool success = outputsSafe && (firstFailure == HAL_OK);
  _safetyManager.reportSafeStateResult(success);
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
          (_coilController.appliedCurrentMilliamps() ==
           config::SafeStartupCurrentMilliamps),
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
  if (_sensorController.takeHostCalibrationRequest()) {
    const bool succeeded =
        _sensorController.runAutomaticCalibrationBlocking(_coilController);
    _sensorController.completeHostCalibration(succeeded);
    return;
  }
  if (_userInput.takePress(dda::UserInputId::Input2)) {
    _sensorController.setAllDefault();
  }
  _userInput.process();
  _requestManager.process();
  _usbController.processLaunchDataTransfer();
  _statusLeds.update();
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
  return _safetyManager.completeFaultClear(faultEpoch, hardwareCleared, false);
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

UserInput &BoardApplication::userInput() noexcept { return _userInput; }

Display &BoardApplication::display() noexcept { return _display; }

bool BoardApplication::isDisplayInitialized() const noexcept {
  return _display.isInitialized();
}

StatusLeds &BoardApplication::statusLeds() noexcept { return _statusLeds; }

Dac088s085 &BoardApplication::externalDac() noexcept { return _externalDac; }

CalibrationStore &BoardApplication::calibrationStore() noexcept {
  return _calibrationStore;
}

SensorController &BoardApplication::sensorController() noexcept {
  return _sensorController;
}

CoilController &BoardApplication::coilController() noexcept {
  return _coilController;
}

void BoardApplication::enableSafetyInterrupts() noexcept {
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0U, 0U);
  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0U, 0U);
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0U, 0U);

  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

BoardApplication &boardApplication() noexcept {
  static BoardApplication application;
  return application;
}

} // namespace dda

extern "C" void DdaApplication_Initialize(void) {
  dda::BoardApplication &application = dda::boardApplication();
  (void)application.init();
}

extern "C" void DdaApplication_Process(void) {
  dda::boardApplication().process();
}
