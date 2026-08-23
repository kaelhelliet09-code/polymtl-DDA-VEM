#include "Service/Calibration/CalibrationRecordCodec.h"

namespace dda::calibration_record {

uint32_t calculateCrc32(const uint8_t *data, size_t length) noexcept {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t index = 0U; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      const uint32_t mask =
          static_cast<uint32_t>(-static_cast<int32_t>(crc & 1U));
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

bool hasSupportedFormat(const Snapshot &snapshot) noexcept {
  return snapshot.magic == Magic && snapshot.formatVersion == FormatVersion &&
         snapshot.snapshotSize == sizeof(Snapshot);
}

bool isValid(const Snapshot &snapshot) noexcept {
  if (!hasSupportedFormat(snapshot) ||
      snapshot.commitMarker != CommitMarker ||
      (snapshot.validSensorMask & ~AllSensorsMask) != 0U) {
    return false;
  }
  return snapshot.crc32 ==
         calculateCrc32(reinterpret_cast<const uint8_t *>(&snapshot),
                        offsetof(Snapshot, crc32));
}

bool sequenceIsNewer(uint32_t candidate, uint32_t reference) noexcept {
  const uint32_t distance = candidate - reference;
  return candidate != reference && distance < 0x80000000U;
}

uint8_t decode(const Snapshot &snapshot,
               std::array<SensorCalibrationData, SensorCount> &data) noexcept {
  data = {};
  for (uint8_t sensor = 0U; sensor < SensorCount; ++sensor) {
    data[sensor] = {snapshot.sensors[sensor].currentLedCode,
                    snapshot.sensors[sensor].voltageTripCode};
  }
  return snapshot.validSensorMask;
}

Snapshot encode(const std::array<SensorCalibrationData, SensorCount> &data,
                uint8_t validSensorMask, uint32_t sequence) noexcept {
  Snapshot snapshot{};
  snapshot.magic = Magic;
  snapshot.formatVersion = FormatVersion;
  snapshot.snapshotSize = sizeof(Snapshot);
  snapshot.sequence = sequence;
  snapshot.validSensorMask = validSensorMask & AllSensorsMask;
  for (uint8_t sensor = 0U; sensor < SensorCount; ++sensor) {
    snapshot.sensors[sensor] = {data[sensor].currentLedCode,
                                data[sensor].voltageTripCode};
  }
  snapshot.crc32 = calculateCrc32(reinterpret_cast<const uint8_t *>(&snapshot),
                                  offsetof(Snapshot, crc32));
  snapshot.commitMarker = CommitMarker;
  return snapshot;
}

} // namespace dda::calibration_record
