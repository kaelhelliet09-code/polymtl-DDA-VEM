/**
 * @file CalibrationStore.h
 * @brief Stores one two-code calibration record per sensor.
 */

#pragma once

#include "Drivers/Sensors/SensorId.h"

#include <array>
#include <cstdint>

namespace dda {

using CalibrationDacCode = uint8_t;

/** @brief The complete calibration retained for one sensor. */
struct SensorCalibrationData {
  CalibrationDacCode currentLedCode;
  CalibrationDacCode voltageTripCode;
};

enum class CalibrationStoreResult : uint8_t {
  Ok,
  StorageEmpty,
  RecoveredFromCorruption,
  CalibrationNotFound,
  NotInitialized,
  InvalidSensor,
  FlashUnlockFailed,
  FlashEraseFailed,
  FlashProgramFailed,
  FlashLockFailed,
  FlashVerificationFailed,
  CorruptStorage,
  IncompatibleFormat,
};

/** @brief Maintains one transactional four-sensor calibration snapshot. */
class CalibrationStore {
public:
  CalibrationStore() noexcept;

  CalibrationStoreResult initialize() noexcept;

  CalibrationStoreResult readSensor(SensorId sensor,
                                    SensorCalibrationData &data) const noexcept;

  CalibrationStoreResult saveSensor(
      SensorId sensor, const SensorCalibrationData &data) noexcept;

  CalibrationStoreResult initializationResult() const noexcept;
  uint32_t flashErrorFlags() const noexcept;

private:
  static constexpr uint8_t SensorCount = 4U;

  static bool isValidSensor(SensorId sensor) noexcept;
  void resetData() noexcept;

  CalibrationStoreResult writeSnapshot(
      const std::array<SensorCalibrationData, SensorCount> &candidate,
      uint8_t validSensorMask) noexcept;

  std::array<SensorCalibrationData, SensorCount> _sensorData;
  uintptr_t _activePageAddress;
  uint32_t _activeSequence;
  uint32_t _flashErrorFlags;
  CalibrationStoreResult _initializationResult;
  uint8_t _validSensorMask;
  bool _readable;
};

} // namespace dda
