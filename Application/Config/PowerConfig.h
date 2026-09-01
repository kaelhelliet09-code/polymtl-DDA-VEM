/**
 * @file PowerConfig.h
 * @brief Board-power monitoring and DRV8874 current-reference configuration.
 *
 * The INA226 measures total board current through the shared shunt. Each
 * DRV8874 independently reports coil current through IPROPI and its installed
 * RPROPI resistor. The 6 A INA226 range is a measurement-calibration choice,
 * not an aggregate software current limit.
 *
 * Current-reference and ADC codes use the nominal relationship:
 * `VIPROPI = IOUT * AIPROPI * RPROPI`.
 * Component tolerance and DRV8874 mirror error must be characterized on the
 * assembled board if better absolute accuracy is required.
 */

#pragma once

#include "Config/BoardConfig.h"
#include "Config/ExternalDacConfig.h"

#include <cstdint>

namespace dda::config {

inline constexpr uint32_t ShuntResistanceMicroOhms = 5000U;
///< Physical INA226 shunt value, in micro-ohms.
inline constexpr uint32_t Ina226CalibrationCurrentRangeMilliamps = 6000U;
///< Whole-board INA226 calibration range; no production interlock uses it.
inline constexpr uint32_t PowerAlertThresholdMilliwatts = 120000U;

///< INA226 power-over-limit threshold that asserts active-low POWER_ALERT.

/** @brief INA226 bus/shunt averaging choices encoded for register 0x00. */
enum class Ina226Averaging : uint16_t {
  Samples1 = 0x0000U,    ///< No averaging.
  Samples4 = 0x0200U,    ///< Average four conversions.
  Samples16 = 0x0400U,   ///< Average sixteen conversions.
  Samples64 = 0x0600U,   ///< Average sixty-four conversions.
  Samples128 = 0x0800U,  ///< Average 128 conversions.
  Samples256 = 0x0A00U,  ///< Average 256 conversions.
  Samples512 = 0x0C00U,  ///< Average 512 conversions.
  Samples1024 = 0x0E00U, ///< Average 1024 conversions.
};

inline constexpr Ina226Averaging Ina226AveragingSetting =
    Ina226Averaging::Samples1024;
///< Number of samples averaged before INA226 measurements and alerts update.

inline constexpr uint32_t Drv8874RpropiOhms = 1800U;
///< Verified resistor from each DRV8874 IPROPI output to ground, in ohms.
inline constexpr uint32_t Drv8874IpropiMicroampsPerAmp = 455U;
///< Nominal AIPROPI scaling from the TI design equations, in microamps/amp.
inline constexpr uint16_t MaximumCoilCurrentMilliamps = 3000U;
///< Per-coil command limit enforced by every current-setting path.
inline constexpr uint16_t DefaultCoilCurrentMilliamps = 1000U;
///< Retained boot setpoint; hardware VREF remains zero until a drive succeeds.

/** @brief Foreground SPI timeout for a DRV8874 current-limit update. */
inline constexpr uint32_t DriverDacTimeoutMilliseconds =
    PeripheralOperationTimeoutMilliseconds;

/**
 * @brief Delay used before changing the shared PMODE level.
 *
 * DRV8874 latches PMODE when nSLEEP rises. The datasheet requires every
 * device to reach sleep (tSLEEP) before PMODE changes; one millisecond is a
 * deliberately conservative foreground-only interval.
 */
inline constexpr uint32_t PmodeRelatchDelayMilliseconds = 1U;

namespace detail {

/** @brief Common fixed-point denominator for current-to-code conversion. */
inline constexpr uint64_t CurrentCodeDenominator =
    static_cast<uint64_t>(AnalogReferenceMillivolts) * 1'000'000ULL;

/**
 * @brief Build the common fixed-point numerator for ADC and DAC conversions.
 * @param currentMilliamps Individual-coil current in milliamperes.
 * @param maximumCode Maximum code of the target converter.
 * @return Unscaled conversion numerator.
 */
constexpr uint64_t currentCodeNumerator(uint32_t currentMilliamps,
                                        uint16_t maximumCode) noexcept {
  return static_cast<uint64_t>(currentMilliamps) *
         Drv8874IpropiMicroampsPerAmp * Drv8874RpropiOhms * maximumCode;
}

} // namespace detail

/**
 * @brief Convert one-coil current to the first ADC code not below it.
 * @param currentMilliamps Individual-coil current in milliamperes.
 * @return Ceiling-rounded ADC code using RPROPI and nominal AIPROPI.
 * @note Ceiling rounding makes a configured lower acceptance bound
 * conservative.
 */
constexpr uint16_t
currentMilliampsToAdcCodeCeiling(uint32_t currentMilliamps) noexcept {
  return static_cast<uint16_t>(
      (detail::currentCodeNumerator(currentMilliamps, AdcMaximumCode) +
       detail::CurrentCodeDenominator - 1ULL) /
      detail::CurrentCodeDenominator);
}

/**
 * @brief Convert one-coil current to the last ADC code not above it.
 * @param currentMilliamps Individual-coil current in milliamperes.
 * @return Floor-rounded ADC code using RPROPI and nominal AIPROPI.
 * @note Floor rounding makes a configured upper acceptance bound
 * conservative.
 */
constexpr uint16_t
currentMilliampsToAdcCodeFloor(uint32_t currentMilliamps) noexcept {
  return static_cast<uint16_t>(
      detail::currentCodeNumerator(currentMilliamps, AdcMaximumCode) /
      detail::CurrentCodeDenominator);
}

/**
 * @brief Convert a requested coil current to the nearest VREF DAC code.
 * @param currentMilliamps Individual-coil current in milliamperes.
 * @return Nearest 8-bit DAC088S085 code using RPROPI and nominal AIPROPI.
 * @note This assumes DAC VREF drives the DRV8874 VREF input without additional
 * gain or attenuation.
 */
constexpr uint8_t
currentMilliampsToReferenceCode(uint32_t currentMilliamps) noexcept {
  return static_cast<uint8_t>(
      (detail::currentCodeNumerator(currentMilliamps, ExternalDacMaximumCode) +
       (detail::CurrentCodeDenominator / 2ULL)) /
      detail::CurrentCodeDenominator);
}

inline constexpr uint32_t MaximumRpropiMeasurableCurrentMilliamps =
    static_cast<uint32_t>(detail::CurrentCodeDenominator /
                          (Drv8874IpropiMicroampsPerAmp * Drv8874RpropiOhms));
///< Largest nominal current that keeps IPROPI within the analog reference.
///< This is an ADC range calculation, not a permissible coil-current rating.

static_assert(ShuntResistanceMicroOhms > 0U,
              "The INA226 shunt value must be nonzero");
static_assert(Ina226CalibrationCurrentRangeMilliamps > 0U,
              "The INA226 calibration range must be nonzero");
static_assert(Drv8874RpropiOhms > 0U && Drv8874IpropiMicroampsPerAmp > 0U,
              "The DRV8874 current-sense transfer must be nonzero");
static_assert(DefaultCoilCurrentMilliamps <= MaximumCoilCurrentMilliamps,
              "The default coil current must be representable");
static_assert(MaximumCoilCurrentMilliamps <=
                  MaximumRpropiMeasurableCurrentMilliamps,
              "The coil limit exceeds the IPROPI ADC range");
static_assert(currentMilliampsToReferenceCode(MaximumCoilCurrentMilliamps) <=
                  ExternalDacMaximumCode,
              "The coil limit exceeds the VREF DAC range");
static_assert(PmodeRelatchDelayMilliseconds > 0U,
              "PMODE changes must allow every DRV8874 to enter sleep");

} // namespace dda::config
