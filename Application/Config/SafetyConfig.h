/**
 * @file SafetyConfig.h
 * @brief Power-stage startup and fault-response timing.
 *
 * @warning Safe startup values are invariants and must remain zero. Increasing
 * them would allow energy to be requested before application safety checks
 * complete.
 */

#pragma once

#include "Config/BoardConfig.h"

#include <cstdint>

namespace dda::config {

inline constexpr uint16_t SafeStartupCurrentMilliamps = 0U;
///< Applied-current state required during startup and fault shutdown.
inline constexpr uint16_t SafeStartupDacCode = 0U;
///< Physical DAC code written during startup and fault shutdown.
inline constexpr uint32_t FaultReleaseValidationMilliseconds = 20U;
///< Continuous all-high nFAULT/POWER_ALERT interval required by ClearFault.
inline constexpr uint32_t PowerAlertRepeatWindowMilliseconds = 1000U;
///< Window in which a second released power alert becomes a latched fault.
inline constexpr uint32_t DirectionDeadTimeMicroseconds = 10U;
///< Minimum all-off interval before an energized direction transition.
inline constexpr uint32_t DriverWakeQualificationMilliseconds = 10U;
///< Conservative nSLEEP wake and nFAULT-release qualification interval.

static_assert(SafeStartupCurrentMilliamps == 0U && SafeStartupDacCode == 0U,
              "Safe startup must command zero energy");
static_assert(DirectionDeadTimeMicroseconds > 0U,
              "Direction reversal requires an all-off interval");
static_assert(DriverWakeQualificationMilliseconds > 0U,
              "Driver wake qualification must be nonzero");

} // namespace dda::config
