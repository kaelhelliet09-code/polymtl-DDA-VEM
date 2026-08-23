#pragma once

#include "Drivers/Sensors/Sensor.h"

#include <cstdint>

namespace dda {

enum class SensorCommand : uint8_t {
  ReportSensor = 0U,
  SensorCalibration,
  SetDefaultLevels,
  SetCalibrationLevels,
  UnlockSensor,
  ReadCalibrationLedCode,
  ReadCalibrationTripCode,
};

enum class SensorOptions : uint8_t {
  Sensor1 = 0U,
  Sensor2,
  Sensor3,
  Sensor4,
  All,
};

enum class SensorStatus : uint8_t {
  Succeeded = 0U,
  Failed,
};

inline constexpr uint8_t CalibrationValueUnavailable = 0xFFU;

constexpr uint8_t sensorNotificationOptions(SensorId sensor,
                                            SensorEdge edge) noexcept {
  constexpr uint8_t FallingEdgeMask = 0x04U;
  return static_cast<uint8_t>(
      static_cast<uint8_t>(sensor) |
      (edge == SensorEdge::Falling ? FallingEdgeMask : 0U));
}

} // namespace dda
