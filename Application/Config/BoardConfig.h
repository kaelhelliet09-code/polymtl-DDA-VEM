/**
 * @file BoardConfig.h
 * @brief Fixed board topology and converter characteristics.
 *
 * @note These values describe assembled hardware and CubeMX configuration.
 * Change them only together with the board design and `DDA.ioc`.
 */

#pragma once

#include <cstdint>

namespace dda::config {

/** @brief Limit for bounded peripheral initialization and readback calls. */
inline constexpr uint32_t PeripheralOperationTimeoutMilliseconds = 100U;

/** @brief Stable interval required before accepting a user-input transition. */
inline constexpr uint32_t UserInputDebounceMilliseconds = 20U;
static_assert(UserInputDebounceMilliseconds > 0U,
              "User-input debounce must be enabled");

inline constexpr uint8_t DriverCount = 4U;
///< Number of populated DRV8874 bridges and nFAULT inputs.
inline constexpr uint8_t SensorCount = 4U;
///< Number of external sensor channels represented in calibration storage.

inline constexpr uint8_t AdcResolutionBits = 8U;
///< ADC1 resolution selected in CubeMX.
inline constexpr uint8_t InternalDacResolutionBits = 12U;
///< DAC1 resolution used for both DRV8874 VREF channels.
inline constexpr uint16_t AdcMaximumCode =
    static_cast<uint16_t>((1UL << AdcResolutionBits) - 1UL);
///< Largest unsigned ADC code derived from `AdcResolutionBits`.
inline constexpr uint16_t InternalDacMaximumCode =
    static_cast<uint16_t>((1UL << InternalDacResolutionBits) - 1UL);
///< Largest unsigned DAC code derived from `InternalDacResolutionBits`.
inline constexpr uint32_t AnalogReferenceMillivolts = 3300U;
///< Nominal common ADC/DAC reference voltage; verify on assembled hardware.

inline constexpr uint32_t Tim2FrequencyHz = 64'000'000U;
///< Free-running TIM2 frequency configured by CubeMX.
inline constexpr uint32_t Tim2TicksPerMicrosecond =
    Tim2FrequencyHz / 1'000'000U;
inline constexpr double VelocitySensorSpacingMillimeters = 50.0;
///< Physical spacing used for the two-sensor velocity calculation.

static_assert(DriverCount == 4U, "The board pin map defines four drivers");
static_assert(SensorCount == 4U, "The board pin map defines four sensors");
static_assert((Tim2FrequencyHz % 1'000'000U) == 0U,
              "TIM2 must have an integral number of ticks per microsecond");
} // namespace dda::config
