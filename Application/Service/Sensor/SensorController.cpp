// Implements per-sensor LED GPIO control, VTRIP DAC routing, and non-blocking
// interrupt debounce/capture.
#include "Service/Sensor/SensorController.h"

#include "Platform/Stm32/System/InterruptGuard.h"
#include "Platform/Stm32/System/Timestamp.h"
#include "Service/Sensor/SensorRequests.h"

namespace {

constexpr uint8_t sensorIndex(dda::SensorId sensor) noexcept {
  return static_cast<uint8_t>(sensor);
}

} // namespace

namespace dda {

SensorController::SensorController(Dac088s085 &dac,
                                   RequestManager &requestManager) noexcept
    : _sensors{
          {config::SensorHardwareConfigs[0].id,
           config::SensorHardwareConfigs[0].inputPort,
           config::SensorHardwareConfigs[0].inputPin,
           config::SensorHardwareConfigs[0].irLedEnablePort,
           config::SensorHardwareConfigs[0].irLedEnablePin,
           config::SensorHardwareConfigs[0].tripVoltageChannel},
          {config::SensorHardwareConfigs[1].id,
           config::SensorHardwareConfigs[1].inputPort,
           config::SensorHardwareConfigs[1].inputPin,
           config::SensorHardwareConfigs[1].irLedEnablePort,
           config::SensorHardwareConfigs[1].irLedEnablePin,
           config::SensorHardwareConfigs[1].tripVoltageChannel},
          {config::SensorHardwareConfigs[2].id,
           config::SensorHardwareConfigs[2].inputPort,
           config::SensorHardwareConfigs[2].inputPin,
           config::SensorHardwareConfigs[2].irLedEnablePort,
           config::SensorHardwareConfigs[2].irLedEnablePin,
           config::SensorHardwareConfigs[2].tripVoltageChannel},
          {config::SensorHardwareConfigs[3].id,
           config::SensorHardwareConfigs[3].inputPort,
           config::SensorHardwareConfigs[3].inputPin,
           config::SensorHardwareConfigs[3].irLedEnablePort,
           config::SensorHardwareConfigs[3].irLedEnablePin,
           config::SensorHardwareConfigs[3].tripVoltageChannel},
      },
      _dac(dac), _requestManager(requestManager), _pendingTriggers(0U),
      _launchCaptureActive(false) {
  _requestManager.registerService(Service::SensorControl, *this);
}

HAL_StatusTypeDef SensorController::setTripVoltageCode(Sensor &sensor,
                                                       uint8_t code) noexcept {
  return sensor.setVoltageTripCode(_dac, code);
}

bool SensorController::setIrLedEnabled(Sensor &sensor, bool enabled) noexcept {
  return sensor.setIrLedEnabled(enabled);
}

HAL_StatusTypeDef SensorController::clearSensorDacOutputs() noexcept {
  HAL_StatusTypeDef firstFailure = HAL_OK;
  for (Sensor &sensor : _sensors) {
    if (!sensor.setIrLedEnabled(false) && (firstFailure == HAL_OK)) {
      firstFailure = HAL_ERROR;
    }
    const HAL_StatusTypeDef status = sensor.setVoltageTripCode(_dac, 0U);
    if ((firstFailure == HAL_OK) && (status != HAL_OK)) {
      firstFailure = status;
    }
  }
  return firstFailure;
}

HAL_StatusTypeDef SensorController::setToDefault(Sensor &sensor) noexcept {
  const HAL_StatusTypeDef status =
      sensor.setVoltageTripCode(_dac, config::DefaultSensorTripVoltageCode);
  if (status != HAL_OK) {
    return status;
  }
  return sensor.setIrLedEnabled(true) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef SensorController::setAllDefault() noexcept {
  for (Sensor &sensor : _sensors) {
    const HAL_StatusTypeDef status = setToDefault(sensor);
    if (status != HAL_OK) {
      return status;
    }
  }
  return HAL_OK;
}

Sensor &SensorController::sensor(uint8_t index) noexcept {
  return _sensors[index];
}

void SensorController::handleSensorInterrupt(Sensor &sensor,
                                             SensorEdge edge) noexcept {
  const uint32_t edgeTimestamp = timestamp.now();
  if (!sensor.acceptEdge(edge, edgeTimestamp)) {
    return;
  }

  if (edge == SensorEdge::Rising) {
    sensor._triggered = true;
    _pendingTriggers = static_cast<uint8_t>(
        _pendingTriggers | (1U << sensorIndex(sensor._sensorId)));
  }

  if (_launchCaptureActive) {
    sensor.recordEdge(edge, edgeTimestamp);
  }

  Request notification{};
  notification.destination = Service::UsbControl;
  notification.source = Service::SensorControl;
  notification.command = static_cast<uint8_t>(SensorCommand::ReportSensor);
  notification.options = sensorNotificationOptions(sensor._sensorId, edge);
  notification.state = RequestState::Outgoing;
  (void)_requestManager.queueRequest(notification);
}

void SensorController::beginLaunchCapture() noexcept {
  InterruptGuard interruptGuard;
  for (Sensor &sensor : _sensors) {
    sensor.clearEvents();
    sensor.resetDebounce();
  }
  _launchCaptureActive = true;
}

void SensorController::endLaunchCapture() noexcept {
  _launchCaptureActive = false;
}

void SensorController::copyLaunchEvents(SensorEvents *events,
                                        uint8_t capacity) const noexcept {
  if ((events == nullptr) || (capacity < config::SensorCount)) {
    return;
  }
  InterruptGuard interruptGuard;
  for (uint8_t index = 0U; index < config::SensorCount; ++index) {
    events[index] = _sensors[index].events();
  }
}

bool SensorController::takeTrigger(Sensor &sensor) noexcept {
  const uint8_t mask =
      static_cast<uint8_t>(1U << sensorIndex(sensor._sensorId));
  InterruptGuard interruptGuard;
  const bool triggered = (_pendingTriggers & mask) != 0U;
  _pendingTriggers = static_cast<uint8_t>(_pendingTriggers & ~mask);
  return triggered;
}

uint8_t SensorController::takePendingTriggers() noexcept {
  InterruptGuard interruptGuard;
  const uint8_t triggers = _pendingTriggers;
  _pendingTriggers = 0U;
  return triggers;
}

void SensorController::processRequest(Request &request) noexcept {
  if (request.destination != Service::SensorControl) {
    return;
  }

  bool succeeded = false;
  const uint8_t selection = request.options;
  const bool allSelected =
      selection == static_cast<uint8_t>(SensorOptions::All);
  const bool sensorSelected = selection < config::SensorCount;

  switch (static_cast<SensorCommand>(request.command)) {
  case SensorCommand::SetDefaultLevels:
    succeeded = (allSelected      ? setAllDefault()
                 : sensorSelected ? setToDefault(_sensors[selection])
                                  : HAL_ERROR) == HAL_OK;
    break;
  case SensorCommand::UnlockSensor:
    _requestManager.unlockSensor();
    succeeded = true;
    break;
  case SensorCommand::ReadCalibrationLedCode:
  case SensorCommand::ReadCalibrationTripCode:
    request.options = CalibrationValueUnavailable;
    request.complete(Service::SensorControl);
    return;
  case SensorCommand::ReportSensor:
  case SensorCommand::SensorCalibration:
  case SensorCommand::SetCalibrationLevels:
  default:
    break;
  }

  request.options = static_cast<uint8_t>(succeeded ? SensorStatus::Succeeded
                                                   : SensorStatus::Failed);
  request.complete(Service::SensorControl);
}

} // namespace dda
