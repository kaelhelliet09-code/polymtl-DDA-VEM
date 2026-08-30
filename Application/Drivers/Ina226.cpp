// Implements INA226 calibration, power-alert programming with readback, and
// asynchronous raw-measurement sequencing.
#include "Drivers/Ina226.h"
#include "Config/PowerConfig.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float CalibrationNumerator = 0.00512F;
constexpr float PowerLsbMultiplier = 25.0F;
constexpr float SignedRegisterScale = 32768.0F;
constexpr float MaximumCalibration = 32767.0F;
constexpr uint32_t I2cTimeoutMs = 10U;
constexpr uint16_t DefaultConfiguration = static_cast<uint16_t>(
    0x4127U | static_cast<uint16_t>(dda::config::Ina226AveragingSetting));
} // namespace

namespace dda {

Ina226::Ina226(I2C_HandleTypeDef &i2c, float shuntResistanceOhms,
               float maximumExpectedCurrentA, uint8_t address) noexcept
    : _i2c(i2c, address), _shuntResistanceOhms(shuntResistanceOhms),
      _maximumExpectedCurrentA(maximumExpectedCurrentA), _currentLsbA(0.0F),
      _calibration(0U), _rawMeasurements{0U, 0U, 0U},
      _remainingMeasurementMask(0U), _asyncState(AsyncState::Idle),
      _asyncFailureAvailable(false), _cancelReadRequested(false) {}

bool Ina226::configureCalibration() noexcept {
  if ((_shuntResistanceOhms <= 0.0F) || (_maximumExpectedCurrentA <= 0.0F)) {
    _currentLsbA = 0.0F;
    return false;
  }

  const float minimumCurrentLsb =
      CalibrationNumerator / (MaximumCalibration * _shuntResistanceOhms);
  _currentLsbA = std::max(_maximumExpectedCurrentA / SignedRegisterScale,
                          minimumCurrentLsb);

  const float calibrationValue =
      CalibrationNumerator / (_currentLsbA * _shuntResistanceOhms);
  if (!std::isfinite(calibrationValue) || (calibrationValue < 1.0F)) {
    _currentLsbA = 0.0F;
    return false;
  }

  // A single-precision boundary result can be fractionally above 32767.
  _calibration =
      static_cast<uint16_t>(std::min(calibrationValue, MaximumCalibration));
  _currentLsbA = CalibrationNumerator /
                 (static_cast<float>(_calibration) * _shuntResistanceOhms);
  return true;
}

HAL_StatusTypeDef Ina226::init() noexcept {
  if (isBusy() || _i2c.isBusy()) {
    return HAL_BUSY;
  }
  if (!configureCalibration()) {
    _asyncState = AsyncState::Failed;
    return HAL_ERROR;
  }

  HAL_StatusTypeDef status = _i2c.writeRegister(
      static_cast<uint8_t>(Register::Calibration), _calibration, I2cTimeoutMs);
  if (status == HAL_OK) {
    status = _i2c.writeRegister(static_cast<uint8_t>(Register::Configuration),
                                DefaultConfiguration, I2cTimeoutMs);
  }

  if (status != HAL_OK) {
    _currentLsbA = 0.0F;
    _asyncState = AsyncState::Failed;
    return status;
  }

  _remainingMeasurementMask = 0U;
  _asyncFailureAvailable = false;
  _cancelReadRequested = false;
  _asyncState = AsyncState::Ready;
  return HAL_OK;
}

HAL_StatusTypeDef Ina226::readRegister(uint8_t registerAddress, uint16_t &value,
                                       uint32_t timeoutMs) noexcept {
  return _i2c.readRegister(registerAddress, value, timeoutMs);
}

HAL_StatusTypeDef
Ina226::configurePowerOverLimitAlert(uint32_t thresholdMilliwatts,
                                     uint32_t timeoutMs) noexcept {
  const float scaleW = powerLsbW();
  if ((timeoutMs == 0U) || (thresholdMilliwatts == 0U) || !(scaleW > 0.0F)) {
    return HAL_ERROR;
  }

  const float rawLimit =
      (static_cast<float>(thresholdMilliwatts) / 1000.0F) / scaleW;
  if (!std::isfinite(rawLimit) || (rawLimit > 65535.0F)) {
    return HAL_ERROR;
  }

  // Truncate so the representable trip point never exceeds the requested one.
  uint16_t limit = static_cast<uint16_t>(rawLimit);
  if (limit == 0U) {
    limit = 1U;
  }

  HAL_StatusTypeDef status =
      _i2c.writeRegister(MaskEnableRegister, 0U, timeoutMs);
  if (status == HAL_OK) {
    status = _i2c.writeRegister(AlertLimitRegister, limit, timeoutMs);
  }

  uint16_t readback = 0U;
  if (status == HAL_OK) {
    status = _i2c.readRegister(AlertLimitRegister, readback, timeoutMs);
  }
  if ((status == HAL_OK) && (readback != limit)) {
    status = HAL_ERROR;
  }

  if (status == HAL_OK) {
    status = _i2c.writeRegister(MaskEnableRegister, PowerOverLimitAlertMask,
                                timeoutMs);
  }
  if (status == HAL_OK) {
    status = _i2c.readRegister(MaskEnableRegister, readback, timeoutMs);
  }
  // AFF, CVRF, and OVF are live read-only status flags. Compare only writable
  // configuration bits.
  if ((status == HAL_OK) && ((readback & AlertConfigurationWritableMask) !=
                             PowerOverLimitAlertMask)) {
    status = HAL_ERROR;
  }
  return status;
}

HAL_StatusTypeDef
Ina226::readMeasurementsNonBlocking(uint8_t measurementMask) noexcept {
  constexpr uint8_t supportedMask =
      static_cast<uint8_t>(RawPower | RawBusVoltage | RawCurrent);

  if (!isReady()) {
    return isBusy() ? HAL_BUSY : HAL_ERROR;
  }
  if ((measurementMask == 0U) ||
      ((measurementMask & static_cast<uint8_t>(~supportedMask)) != 0U)) {
    return HAL_ERROR;
  }

  _remainingMeasurementMask = measurementMask;
  _rawMeasurements = {0U, 0U, 0U};
  _asyncFailureAvailable = false;
  _cancelReadRequested = false;

  const HAL_StatusTypeDef status = startNextMeasurementRead();
  if (status != HAL_OK) {
    failAsyncOperation();
  }
  return status;
}

HAL_StatusTypeDef Ina226::startNextMeasurementRead() noexcept {
  // Preserve protocol word order: power, bus voltage, then current.
  if ((_remainingMeasurementMask & RawPower) != 0U) {
    _remainingMeasurementMask =
        static_cast<uint8_t>(_remainingMeasurementMask & ~RawPower);
    _asyncState = AsyncState::ReadingPower;
    return _i2c.readRegisterDma(static_cast<uint8_t>(Register::Power));
  }

  if ((_remainingMeasurementMask & RawBusVoltage) != 0U) {
    _remainingMeasurementMask =
        static_cast<uint8_t>(_remainingMeasurementMask & ~RawBusVoltage);
    _asyncState = AsyncState::ReadingBusVoltage;
    return _i2c.readRegisterDma(static_cast<uint8_t>(Register::BusVoltage));
  }

  if ((_remainingMeasurementMask & RawCurrent) != 0U) {
    _remainingMeasurementMask =
        static_cast<uint8_t>(_remainingMeasurementMask & ~RawCurrent);
    _asyncState = AsyncState::ReadingCurrent;
    return _i2c.readRegisterDma(static_cast<uint8_t>(Register::Current));
  }

  _asyncState = AsyncState::MeasurementComplete;
  return HAL_OK;
}

void Ina226::process() noexcept {
  if (_i2c.takeError()) {
    // Requested abort completion and bus faults share the transport state.
    if (_cancelReadRequested) {
      _cancelReadRequested = false;
      _remainingMeasurementMask = 0U;
      _asyncState = AsyncState::Ready;
    } else {
      failAsyncOperation();
    }
    return;
  }

  const AsyncState completedRead = _asyncState;
  if ((completedRead != AsyncState::ReadingPower) &&
      (completedRead != AsyncState::ReadingBusVoltage) &&
      (completedRead != AsyncState::ReadingCurrent)) {
    return;
  }

  uint16_t rawValue = 0U;
  if (!_i2c.takeReadComplete(rawValue)) {
    return;
  }

  if (_cancelReadRequested) {
    _cancelReadRequested = false;
    _remainingMeasurementMask = 0U;
    _asyncState = AsyncState::Ready;
    return;
  }

  switch (completedRead) {
  case AsyncState::ReadingPower:
    _rawMeasurements.power = rawValue;
    break;
  case AsyncState::ReadingBusVoltage:
    _rawMeasurements.busVoltage = rawValue;
    break;
  case AsyncState::ReadingCurrent:
    _rawMeasurements.current = rawValue;
    break;
  default:
    break;
  }

  if (startNextMeasurementRead() != HAL_OK) {
    failAsyncOperation();
  }
}

void Ina226::cancelRead() noexcept {
  switch (_asyncState) {
  case AsyncState::ReadingPower:
  case AsyncState::ReadingBusVoltage:
  case AsyncState::ReadingCurrent:
    _cancelReadRequested = true;
    _remainingMeasurementMask = 0U;
    break;
  case AsyncState::MeasurementComplete:
    _asyncState = AsyncState::Ready;
    break;
  default:
    break;
  }
}

HAL_StatusTypeDef Ina226::abortRead() noexcept {
  switch (_asyncState) {
  case AsyncState::ReadingPower:
  case AsyncState::ReadingBusVoltage:
  case AsyncState::ReadingCurrent:
    _cancelReadRequested = true;
    _remainingMeasurementMask = 0U;
    return _i2c.requestAbort();
  default:
    return HAL_ERROR;
  }
}

bool Ina226::takeRawMeasurements(RawMeasurements &values) noexcept {
  if (_asyncState != AsyncState::MeasurementComplete) {
    return false;
  }

  values = _rawMeasurements;
  _asyncState = AsyncState::Ready;
  return true;
}

bool Ina226::takeAsyncFailure() noexcept {
  if (!_asyncFailureAvailable) {
    return false;
  }

  _asyncFailureAvailable = false;
  return true;
}

bool Ina226::isReady() const noexcept {
  return _asyncState == AsyncState::Ready;
}

bool Ina226::isBusy() const noexcept {
  return (_asyncState == AsyncState::ReadingPower) ||
         (_asyncState == AsyncState::ReadingBusVoltage) ||
         (_asyncState == AsyncState::ReadingCurrent);
}

void Ina226::failAsyncOperation() noexcept {
  _remainingMeasurementMask = 0U;
  _cancelReadRequested = false;
  _asyncFailureAvailable = true;
  _asyncState = AsyncState::Ready;
}

float Ina226::powerLsbW() const noexcept {
  return PowerLsbMultiplier * _currentLsbA;
}

} // namespace dda
