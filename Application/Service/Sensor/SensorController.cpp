// Implements logical sensor-to-DAC routing, interrupt trigger publication,
// and delegation to the transactional calibration store.
#include "Service/Sensor/SensorController.h"

#include "Config/SensorConfig.h"
#include "Platform/Stm32/System/InterruptGuard.h"
#include "Platform/Stm32/System/Timestamp.h"
#include "Service/Calibration/CalibrationStore.h"
#include "Service/Power/CoilController.h"
#include "Service/Sensor/SensorRequests.h"
#include "stm32g0xx_hal_def.h"
#include <cstdint>

extern "C" {
#include "main.h"
}

namespace {
constexpr uint8_t sensorIndex(dda::SensorId sensor) noexcept {
  return static_cast<uint8_t>(sensor);
}

} // namespace

namespace dda {

SensorController::SensorController(Dac088s085 &dac,
                                   CalibrationStore &calibrationStore,
                                   RequestManager &requestManager) noexcept
    : _sensors{
          {config::SensorHardwareConfigs[0].id,
           config::SensorHardwareConfigs[0].inputPort,
           config::SensorHardwareConfigs[0].inputPin,
           config::SensorHardwareConfigs[0].ledCurrentChannel,
           config::SensorHardwareConfigs[0].tripVoltageChannel},
          {config::SensorHardwareConfigs[1].id,
           config::SensorHardwareConfigs[1].inputPort,
           config::SensorHardwareConfigs[1].inputPin,
           config::SensorHardwareConfigs[1].ledCurrentChannel,
           config::SensorHardwareConfigs[1].tripVoltageChannel},
          {config::SensorHardwareConfigs[2].id,
           config::SensorHardwareConfigs[2].inputPort,
           config::SensorHardwareConfigs[2].inputPin,
           config::SensorHardwareConfigs[2].ledCurrentChannel,
           config::SensorHardwareConfigs[2].tripVoltageChannel},
          {config::SensorHardwareConfigs[3].id,
           config::SensorHardwareConfigs[3].inputPort,
           config::SensorHardwareConfigs[3].inputPin,
           config::SensorHardwareConfigs[3].ledCurrentChannel,
           config::SensorHardwareConfigs[3].tripVoltageChannel},
      },
      _dac(dac), _calibrationStore(calibrationStore),
      _requestManager(requestManager), _pendingTriggers(0U),
      _hostCalibrationRequested(false), _hostCalibrationActive(false),
      _hostCalibrationResultReady(false),
      _hostCalibrationSucceeded(false), _launchCaptureActive(false) {
  _requestManager.registerService(Service::SensorControl, *this);
}

CalibrationStoreResult SensorController::initializeCalibrationStore() noexcept {
  return _calibrationStore.initialize();
}

HAL_StatusTypeDef SensorController::setLedCurrentCode(Sensor &sensor,
                                                      uint8_t code) noexcept {
  return sensor.setCurrentLedCode(_dac, code);
}

HAL_StatusTypeDef SensorController::setTripVoltageCode(Sensor &sensor,
                                                       uint8_t code) noexcept {
  return sensor.setVoltageTripCode(_dac, code);
}

HAL_StatusTypeDef SensorController::clearSensorDacOutputs() noexcept {
  return _dac.writeAll(0, dda::config::SensorCalibrationDacTimeoutMilliseconds);
}

HAL_StatusTypeDef SensorController::setToCalibrated(Sensor &sensor) noexcept {
  SensorCalibrationData calibration{};
  if (_calibrationStore.readSensor(sensor._sensorId, calibration) !=
      CalibrationStoreResult::Ok) {
    return HAL_ERROR;
  }
  auto status = sensor.setCurrentLedCode(_dac, calibration.currentLedCode);
  if (status != HAL_OK) {
    return status;
  }
  return sensor.setVoltageTripCode(_dac, calibration.voltageTripCode);
}

HAL_StatusTypeDef SensorController::setToDefault(Sensor &sensor) noexcept {
  auto status =
      sensor.setCurrentLedCode(_dac, config::DefaultSensorLedCurrentCode);
  if (status != HAL_OK) {
    return status;
  }
  return sensor.setVoltageTripCode(_dac, config::DefaultSensorTripVoltageCode);
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

HAL_StatusTypeDef SensorController::setAllCalibrated() noexcept {
  for (Sensor &sensor : _sensors) {
    const HAL_StatusTypeDef status = setToCalibrated(sensor);
    if (status != HAL_OK) {
      return status;
    }
  }
  return HAL_OK;
}

bool SensorController::calibrateSensor(Sensor &sensor) noexcept {
  sensor.clearTriggered();
  uint8_t finalTripCode = config::SensorCalibrationMaximumDacCode;
  uint8_t finalCurrentCode = config::SensorCalibrationInitialLedCode;

  if (sensor.setCurrentLedCode(
          _dac, config::SensorCalibrationInitialLedCode,
          config::SensorCalibrationDacTimeoutMilliseconds) != HAL_OK) {
    return false;
  }

  for (uint8_t tripCode = config::SensorCalibrationInitialTripCode;
       tripCode <= config::SensorCalibrationMaximumDacCode; ++tripCode) {
    if (sensor.setVoltageTripCode(
            _dac, tripCode, config::SensorCalibrationDacTimeoutMilliseconds) !=
        HAL_OK) {
      return false;
    }
    HAL_Delay(config::SensorCalibrationSettlingMilliseconds);
    if (sensor.refreshTriggered()) {
      finalTripCode = tripCode;
      break;
    }
  }

  if (!sensor.wasTriggered()) {
    for (uint8_t ledCode = config::SensorCalibrationInitialLedCode;
         ledCode <= config::SensorCalibrationMaximumDacCode; ++ledCode) {
      if (sensor.setCurrentLedCode(
              _dac, ledCode, config::SensorCalibrationDacTimeoutMilliseconds) !=
          HAL_OK) {
        return false;
      }
      HAL_Delay(config::SensorCalibrationSettlingMilliseconds);
      if (sensor.refreshTriggered()) {
        finalCurrentCode = ledCode;
        break;
      }
    }
  }

  if (!sensor.wasTriggered()) {
    return false;
  }

  const SensorCalibrationData calibration{finalCurrentCode, finalTripCode};
  return _calibrationStore.saveSensor(sensor._sensorId, calibration) ==
         CalibrationStoreResult::Ok;
}

bool SensorController::runAutomaticCalibrationBlocking(
    CoilController &coils) noexcept {
  if (coils.disableAll() != HAL_OK) {
    return false;
  }

  bool success = true;
  for (uint8_t sensorIndex = 0U; sensorIndex < config::SensorCount;
       ++sensorIndex) {
    if ((_dac.writeAll(0U, config::SensorDacTimeoutMilliseconds) != HAL_OK) ||
        !calibrateSensor(_sensors[sensorIndex])) {
      success = false;
      break;
    }
  }
  const bool outputsCleared =
      _dac.writeAll(0U, config::SensorDacTimeoutMilliseconds) == HAL_OK;
  return success && outputsCleared;
}

bool SensorController::takeHostCalibrationRequest() noexcept {
  InterruptGuard interruptGuard;
  const bool requested = _hostCalibrationRequested;
  _hostCalibrationRequested = false;
  return requested;
}

void SensorController::completeHostCalibration(bool succeeded) noexcept {
  _hostCalibrationSucceeded = succeeded;
  _hostCalibrationResultReady = true;
}

Sensor &SensorController::sensor(uint8_t index) noexcept {
  return _sensors[index];
}

void SensorController::handleSensorInterrupt(Sensor &sensor,
                                             SensorEdge edge) noexcept {
  if (edge == SensorEdge::Rising) {
    sensor._triggered = true;
    _pendingTriggers = static_cast<uint8_t>(
        _pendingTriggers | (1U << sensorIndex(sensor._sensorId)));
  }

  if (_launchCaptureActive) {
    sensor.recordEdge(edge, timestamp.now());
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
  // Atomically consume only the requested bit; ISR code may publish another
  // sensor trigger while foreground processing handles the returned event.
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

CalibrationStoreResult
SensorController::readCalibration(const Sensor &sensor,
                                  SensorCalibrationData &data) const noexcept {
  return _calibrationStore.readSensor(sensor._sensorId, data);
}

CalibrationStoreResult
SensorController::saveCalibration(const Sensor &sensor,
                                  CalibrationDacCode currentLedCode,
                                  CalibrationDacCode voltageTripCode) noexcept {
  return _calibrationStore.saveSensor(sensor._sensorId,
                                      {currentLedCode, voltageTripCode});
}

CalibrationStoreResult
SensorController::calibrationStoreStatus() const noexcept {
  return _calibrationStore.initializationResult();
}

uint32_t SensorController::calibrationFlashErrorFlags() const noexcept {
  return _calibrationStore.flashErrorFlags();
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
  case SensorCommand::SensorCalibration:
    if (_hostCalibrationResultReady) {
      succeeded = _hostCalibrationSucceeded;
      _hostCalibrationResultReady = false;
      _hostCalibrationActive = false;
      break;
    }
    if (!_hostCalibrationActive) {
      _hostCalibrationActive = true;
      _hostCalibrationRequested = true;
    }
    return;
  case SensorCommand::SetDefaultLevels:
    succeeded = (allSelected      ? setAllDefault()
                 : sensorSelected ? setToDefault(_sensors[selection])
                                  : HAL_ERROR) == HAL_OK;
    break;
  case SensorCommand::SetCalibrationLevels:
    succeeded = (allSelected      ? setAllCalibrated()
                 : sensorSelected ? setToCalibrated(_sensors[selection])
                                  : HAL_ERROR) == HAL_OK;
    break;
  case SensorCommand::UnlockSensor:
    _requestManager.unlockSensor();
    succeeded = true;
    break;
  case SensorCommand::ReadCalibrationLedCode:
  case SensorCommand::ReadCalibrationTripCode: {
    SensorCalibrationData calibration{};
    request.options = CalibrationValueUnavailable;
    if (sensorSelected && (_calibrationStore.readSensor(
                               _sensors[selection]._sensorId, calibration) ==
                           CalibrationStoreResult::Ok)) {
      request.options = static_cast<SensorCommand>(request.command) ==
                                SensorCommand::ReadCalibrationLedCode
                            ? calibration.currentLedCode
                            : calibration.voltageTripCode;
    }
    request.complete(Service::SensorControl);
    return;
  }
  case SensorCommand::ReportSensor:
  default:
    break;
  }

  request.options = static_cast<uint8_t>(succeeded ? SensorStatus::Succeeded
                                                   : SensorStatus::Failed);
  request.complete(Service::SensorControl);
}

} // namespace dda
