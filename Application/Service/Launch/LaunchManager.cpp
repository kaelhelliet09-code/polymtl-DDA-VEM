#include "Service/Launch/LaunchManager.h"

#include "Service/Launch/CurrentSampleOrder.h"

#include "Platform/Stm32/System/Timestamp.h"
#include "Service/Power/CoilController.h"
#include "Service/Sensor/SensorController.h"
#include "Service/Usb/USBcontroller.h"

namespace {

dda::LaunchManager *activeLaunchManager = nullptr;

constexpr uint32_t TimerClockHz = 64'000'000U;
constexpr uint32_t SamplingTimerClockHz = 1'000'000U;
constexpr uint32_t MillisecondsPerSecond = 1'000U;

} // namespace

namespace dda {

LaunchManager::LaunchManager(RequestManager &requestManager,
                             SensorController &sensors, CoilController &coils,
                             ADC_HandleTypeDef &adc, Ina226 &powerMonitor,
                             USBcontroller &usb,
                             TIM_HandleTypeDef &adcSamplingTimer,
                             TIM_HandleTypeDef &powerSamplingTimer,
                             TIM_HandleTypeDef &attemptTimer) noexcept
    : _requestManager(requestManager), _sensors(sensors), _coils(coils),
      _adc(adc), _powerMonitor(powerMonitor), _usb(usb),
      _adcSamplingTimer(adcSamplingTimer),
      _powerSamplingTimer(powerSamplingTimer), _attemptTimer(attemptTimer) {
  activeLaunchManager = this;
  _requestManager.registerService(Service::LaunchManager, *this);
}

HAL_StatusTypeDef LaunchManager::init() noexcept {
  if ((_adc.validateConfiguration() != HAL_OK) ||
      (_adc.calibrate() != HAL_OK)) {
    return HAL_ERROR;
  }
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 1U, 0U);
  HAL_NVIC_SetPriority(TIM3_TIM4_IRQn, 1U, 0U);
  HAL_NVIC_SetPriority(TIM7_LPTIM2_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(TIM7_LPTIM2_IRQn);
  return configureTimers();
}

HAL_StatusTypeDef LaunchManager::configureTimers() noexcept {
  if ((_samplingFrequencyHz == 0U) ||
      (_samplingFrequencyHz > MaximumSamplingFrequencyHz)) {
    return HAL_ERROR;
  }

  __HAL_TIM_SET_PRESCALER(&_adcSamplingTimer,
                          (TimerClockHz / SamplingTimerClockHz) - 1U);
  __HAL_TIM_SET_AUTORELOAD(
      &_adcSamplingTimer,
      (SamplingTimerClockHz / _samplingFrequencyHz) - 1U);
  __HAL_TIM_SET_COUNTER(&_adcSamplingTimer, 0U);
  CLEAR_BIT(_adcSamplingTimer.Instance->SMCR, TIM_SMCR_SMS | TIM_SMCR_TS);
  _adcSamplingTimer.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_CLEAR_FLAG(&_adcSamplingTimer, TIM_FLAG_UPDATE);

  __HAL_TIM_SET_PRESCALER(&_powerSamplingTimer, 0U);
  __HAL_TIM_SET_AUTORELOAD(&_powerSamplingTimer,
                          PowerSamplingDivider - 1U);
  __HAL_TIM_SET_COUNTER(&_powerSamplingTimer, 0U);
  TIM_SlaveConfigTypeDef slave{};
  slave.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
  slave.InputTrigger = TIM_TS_ITR2;
  if (HAL_TIM_SlaveConfigSynchro(&_powerSamplingTimer, &slave) != HAL_OK) {
    return HAL_ERROR;
  }
  _powerSamplingTimer.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_CLEAR_FLAG(&_powerSamplingTimer, TIM_FLAG_UPDATE);

  __HAL_TIM_SET_PRESCALER(&_attemptTimer,
                          (TimerClockHz / MillisecondsPerSecond) - 1U);
  __HAL_TIM_SET_AUTORELOAD(&_attemptTimer,
                          RunDurationMilliseconds - 1U);
  __HAL_TIM_SET_COUNTER(&_attemptTimer, 0U);
  _attemptTimer.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_CLEAR_FLAG(&_attemptTimer, TIM_FLAG_UPDATE);
  return HAL_OK;
}

bool LaunchManager::startRun(uint8_t runId) noexcept {
  if (_runActive || _usb.isLaunchDataTransferActive() ||
      !_usb.isCommunicationReady()) {
    return false;
  }
  _requestManager.clearSnapshots();

  _data.currentSampleCount = 0U;
  _data.powerSampleCount = 0U;
  _data.missedPowerSampleCount = 0U;
  _missedPowerSamples = 0U;
  _timeoutRequested = false;
  _runId = runId;

  _sensors._velocitySensor.reset();
  _sensors.beginLaunchCapture();
  _data.launchStart = timestamp.now();
  _runActive = true;
  _requestManager.setLaunchActive(true);

  if (!_powerMonitor.isReady() || (configureTimers() != HAL_OK)) {
    finishRun(LaunchStatus::AcquisitionError, false, false);
    return false;
  }
  (void)_powerMonitor.takeAsyncFailure();

  const uint32_t adcValueCount =
      (_samplingFrequencyHz * RunDurationMilliseconds * BridgeCount) /
      MillisecondsPerSecond;
  if (_adc.startDma(_data.currentData, adcValueCount) != HAL_OK) {
    finishRun(LaunchStatus::AcquisitionError, false, false);
    return false;
  }
  _adcActive = true;

  if (startTimers() == HAL_OK) {
    return true;
  }

  finishRun(LaunchStatus::AcquisitionError, false, false);
  return false;
}

HAL_StatusTypeDef LaunchManager::startTimers() noexcept {
  if (HAL_TIM_Base_Start_IT(&_powerSamplingTimer) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_TIM_Base_Start_IT(&_attemptTimer) != HAL_OK) {
    (void)HAL_TIM_Base_Stop_IT(&_powerSamplingTimer);
    return HAL_ERROR;
  }
  if (HAL_TIM_Base_Start(&_adcSamplingTimer) != HAL_OK) {
    (void)HAL_TIM_Base_Stop_IT(&_attemptTimer);
    (void)HAL_TIM_Base_Stop_IT(&_powerSamplingTimer);
    return HAL_ERROR;
  }
  return HAL_OK;
}

void LaunchManager::stopTimers() noexcept {
  (void)HAL_TIM_Base_Stop(&_adcSamplingTimer);
  (void)HAL_TIM_Base_Stop_IT(&_powerSamplingTimer);
  (void)HAL_TIM_Base_Stop_IT(&_attemptTimer);
}

void LaunchManager::finishRun(LaunchStatus status, bool sendData,
                              bool notifyHost) noexcept {
  if (!_runActive) {
    return;
  }

  stopTimers();
  const HAL_StatusTypeDef coilStopStatus = _coils.disableAll();
  _sensors.endLaunchCapture();
  const HAL_StatusTypeDef sensorStopStatus =
      _sensors.clearSensorDacOutputs();
  (void)_sensors.takePendingTriggers();
  const uint32_t currentValueCount =
      _adcActive ? _adc.transferredValueCount() : 0U;
  const HAL_StatusTypeDef adcStopStatus =
      _adcActive ? _adc.stop() : HAL_OK;
  _adcActive = false;
  _data.launchEnd = timestamp.now();

  _powerMonitor.process();
  collectPowerSample();

  if (_powerMonitor.isBusy()) {
    (void)_powerMonitor.abortRead();
  } else {
    _powerMonitor.cancelRead();
  }

  _data.currentSampleCount = currentValueCount / BridgeCount;
  normalizeBridgeCurrentSamples(_data.currentData, _data.currentSampleCount);
  _data.missedPowerSampleCount = _missedPowerSamples;
  _sensors.copyLaunchEvents(_data.sensorEvents, config::SensorCount);
  _data.velocityTickDelta = _sensors._velocitySensor.getTickDelta();
  _data.requestSnapshots = _requestManager.snapshots();
  _data.snapshotCount = _requestManager.isCompetitionMode()
                            ? 0U
                            : _requestManager.snapshotCount();
  _runActive = false;
  _timeoutRequested = false;
  _requestManager.setLaunchActive(false);

  if ((coilStopStatus != HAL_OK) || (sensorStopStatus != HAL_OK)) {
    status = LaunchStatus::SafetyFault;
  }
  if (adcStopStatus != HAL_OK) {
    status = LaunchStatus::AcquisitionError;
    sendData = false;
  }
  if (!sendData) {
    _usb.cancelLaunchDataTransfer();
    _requestManager.clearSnapshots();
  }
  if (notifyHost) {
    queueRunStatus(status);
  }
  if (sendData && !_usb.startLaunchDataTransfer(_data, _runId)) {
    _usb.cancelLaunchDataTransfer();
  }
}

void LaunchManager::collectPowerSample() noexcept {
  Ina226::RawMeasurements measurement{};
  if (!_powerMonitor.takeRawMeasurements(measurement)) {
    return;
  }
  if (_data.powerSampleCount < PowerBufferSize) {
    _data.powerData[_data.powerSampleCount++] = measurement.power;
  }
}

void LaunchManager::process() noexcept {
  if (_usb.takeCommunicationFailure()) {
    _requestManager.discardRequestsInvolving(Service::UsbControl);
    if (_runActive) {
      finishRun(LaunchStatus::HostAborted, false, false);
    }
    _systemResetRequested = true;
    return;
  }

  _powerMonitor.process();
  if (!_runActive) {
    return;
  }

  collectPowerSample();
  if (_powerMonitor.takeAsyncFailure()) {
    finishRun(LaunchStatus::AcquisitionError, true);
    return;
  }
  if (_timeoutRequested) {
    finishRun(LaunchStatus::TimedOut, true);
    return;
  }

  switch (_adc.takeSequenceResult()) {
  case AdcSampler::SequenceResult::Complete:
    finishRun(LaunchStatus::TimedOut, true);
    break;
  case AdcSampler::SequenceResult::Error:
    finishRun(LaunchStatus::AcquisitionError, true);
    break;
  default:
    break;
  }
}

bool LaunchManager::takeSystemResetRequest() noexcept {
  const bool requested = _systemResetRequested;
  _systemResetRequested = false;
  return requested;
}

void LaunchManager::processRequest(Request &request) noexcept {
  if (request.destination != Service::LaunchManager) {
    return;
  }

  LaunchStatus status = LaunchStatus::Success;
  switch (static_cast<LaunchCommand>(request.command)) {
  case LaunchCommand::StartRun:
    if (_runActive || _usb.isLaunchDataTransferActive()) {
      status = LaunchStatus::Busy;
    } else {
      status = startRun(request.options) ? LaunchStatus::Success
                                         : LaunchStatus::AcquisitionError;
    }
    break;
  case LaunchCommand::StopRun:
    if (_runActive) {
      finishRun(LaunchStatus::Success, true);
    } else {
      status = LaunchStatus::Busy;
    }
    break;
  case LaunchCommand::AbortRun:
    if (_runActive) {
      finishRun(LaunchStatus::HostAborted, false);
    } else {
      status = LaunchStatus::Busy;
    }
    break;
  case LaunchCommand::SetSamplingRate: {
    const uint32_t frequencyHz = static_cast<uint32_t>(request.options) * 100U;
    if (_runActive || (frequencyHz == 0U) ||
        (frequencyHz > MaximumSamplingFrequencyHz)) {
      status = LaunchStatus::Busy;
    } else {
      _samplingFrequencyHz = frequencyHz;
    }
    break;
  }
  case LaunchCommand::SetDebugMode:
    _requestManager.setDebugMode(request.options != 0U);
    break;
  case LaunchCommand::RunStatus:
  default:
    status = LaunchStatus::InvalidCommand;
    break;
  }

  request.options = static_cast<uint8_t>(status);
  request.complete(Service::LaunchManager);
}

void LaunchManager::queueRunStatus(LaunchStatus status) noexcept {
  Request request{};
  request.destination = Service::UsbControl;
  request.source = Service::LaunchManager;
  request.command = static_cast<uint8_t>(LaunchCommand::RunStatus);
  request.options = static_cast<uint8_t>(status);
  request.state = RequestState::Outgoing;
  (void)_requestManager.queueRequest(request);
}

void LaunchManager::abortForSafety() noexcept {
  finishRun(LaunchStatus::SafetyFault, true);
}

bool LaunchManager::isRunActive() const noexcept { return _runActive; }

void LaunchManager::handleTimerFromIsr(TIM_HandleTypeDef &timer) noexcept {
  if (!_runActive) {
    return;
  }
  if (timer.Instance == _powerSamplingTimer.Instance) {
    if (_powerMonitor.readMeasurementsNonBlocking(Ina226::RawPower) != HAL_OK) {
      ++_missedPowerSamples;
    }
  } else if (timer.Instance == _attemptTimer.Instance) {
    _timeoutRequested = true;
  }
}

} // namespace dda

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer) {
  if ((activeLaunchManager != nullptr) && (timer != nullptr)) {
    activeLaunchManager->handleTimerFromIsr(*timer);
  }
}
