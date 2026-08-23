#pragma once

#include "Config/SensorConfig.h"
#include "Drivers/Sensors/Sensor.h"
#include "Service/Launch/LaunchDataSource.h"
#include "Service/RequestManager/RequestManager.h"

#include <cstdint>

namespace dda {

/** @brief Number of current channels stored per ADC sample. */
inline constexpr uint32_t BridgeCount = 4U;
/** @brief Highest supported launch current-sampling frequency. */
inline constexpr uint32_t MaximumLaunchSamplingFrequencyHz = 5'000U;
/** @brief Fixed maximum duration of one recorded launch. */
inline constexpr uint32_t LaunchRunDurationMilliseconds = 5'000U;
/** @brief Current-sample periods between board-power samples. */
inline constexpr uint32_t PowerSamplingDivider = 10U;
/** @brief Extra power entries reserved for boundary scheduling jitter. */
inline constexpr uint32_t PowerSampleCapacityMargin = 16U;

/** @brief Maximum number of four-channel current samples in one launch. */
inline constexpr uint32_t MaximumLaunchSampleCount =
    (MaximumLaunchSamplingFrequencyHz * LaunchRunDurationMilliseconds) / 1'000U;
/** @brief Byte capacity of the interleaved four-bridge current buffer. */
inline constexpr uint32_t CurrentBufferSize =
    MaximumLaunchSampleCount * BridgeCount;
/** @brief Nominal maximum number of power samples in one launch. */
inline constexpr uint32_t MaximumPowerSampleCount =
    (MaximumLaunchSamplingFrequencyHz * LaunchRunDurationMilliseconds +
     (1'000U * PowerSamplingDivider) - 1U) /
    (1'000U * PowerSamplingDivider);
/** @brief Complete power buffer capacity including scheduling margin. */
inline constexpr uint32_t PowerBufferSize =
    MaximumPowerSampleCount + PowerSampleCapacityMargin;

/**
 * @brief Owns one launch capture and serializes its populated data.
 * @details The wire order is sensor edges, current samples, power samples,
 * launch timestamps, velocity interval, and request snapshots. Unused buffer
 * capacity is omitted. Request snapshots are referenced rather than copied;
 * RequestManager must retain them until USB transfer completes.
 */
struct LaunchData final : LaunchDataSource {
  SensorEvents sensorEvents[config::SensorCount]{}; ///< Per-sensor edge ticks.

  uint32_t currentSampleCount{0U}; ///< Valid four-channel current scans.
  alignas(4) uint8_t currentData[CurrentBufferSize]{}; ///< Interleaved ADC codes.

  uint32_t powerSampleCount{0U};       ///< Valid INA226 power words.
  uint32_t missedPowerSampleCount{0U}; ///< Sampling deadlines not serviced.
  uint16_t powerData[PowerBufferSize]{}; ///< Raw INA226 power-register words.

  uint32_t launchStart{0U};       ///< TIM2 tick captured at launch start.
  uint32_t launchEnd{0U};         ///< TIM2 tick captured at launch finish.
  uint32_t velocityTickDelta{0U}; ///< TIM2 ticks between velocity sensors.

  const RequestSnapshot *requestSnapshots{nullptr}; ///< Non-owning diagnostics.
  uint16_t snapshotCount{0U}; ///< Serialized snapshots; zero in competition.

  /** @brief Return the populated wire-stream size. @return Serialized bytes. */
  uint32_t serializedSize() const noexcept override;
  /**
   * @brief Serialize a bounded segment of the logical wire stream.
   * @param offset Byte offset in the complete serialized representation.
   * @param output Destination buffer.
   * @param capacity Available bytes in `output`.
   * @return Number of bytes written.
   */
  uint8_t serialize(uint32_t offset, uint8_t *output,
                    uint8_t capacity) const noexcept override;
};

} // namespace dda
