/**
 * @file CalibrationRecordCodec.h
 * @brief Defines the compact, HAL-independent calibration Flash record.
 */

#pragma once

#include "Service/Calibration/CalibrationStore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dda::calibration_record {

inline constexpr uint32_t Magic = 0x5343414CU;
inline constexpr uint16_t FormatVersion = 2U;
inline constexpr uint64_t CommitMarker = 0x43414C4942524154ULL;
inline constexpr uint8_t SensorCount = 4U;
inline constexpr uint8_t AllSensorsMask = (1U << SensorCount) - 1U;

struct StoredSensor {
  uint8_t currentLedCode;
  uint8_t voltageTripCode;
};

struct Snapshot {
  uint32_t magic;
  uint16_t formatVersion;
  uint16_t snapshotSize;
  uint32_t sequence;
  uint8_t validSensorMask;
  uint8_t reserved[3];
  StoredSensor sensors[SensorCount];
  uint32_t reservedTail;
  uint32_t crc32;
  uint64_t commitMarker;
};

static_assert(sizeof(StoredSensor) == 2U);
static_assert(offsetof(Snapshot, crc32) == 28U);
static_assert(offsetof(Snapshot, commitMarker) == 32U);
static_assert(sizeof(Snapshot) == 40U);
static_assert(std::is_standard_layout_v<Snapshot>);
static_assert(std::is_trivially_copyable_v<Snapshot>);

uint32_t calculateCrc32(const uint8_t *data, size_t length) noexcept;
bool hasSupportedFormat(const Snapshot &snapshot) noexcept;
bool isValid(const Snapshot &snapshot) noexcept;
bool sequenceIsNewer(uint32_t candidate, uint32_t reference) noexcept;

uint8_t decode(
    const Snapshot &snapshot,
    std::array<SensorCalibrationData, SensorCount> &data) noexcept;

Snapshot encode(
    const std::array<SensorCalibrationData, SensorCount> &data,
    uint8_t validSensorMask, uint32_t sequence) noexcept;

} // namespace dda::calibration_record
