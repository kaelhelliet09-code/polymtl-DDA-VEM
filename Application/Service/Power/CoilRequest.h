#pragma once

#include <cstdint>

namespace dda {

/** @brief Stable command byte values for the coil-control service. */
enum class CoilCommand : uint8_t {
  Forward = 0U, ///< Drive the selected bridge(s) forward.
  Reverse,      ///< Drive the selected bridge(s) in reverse.
  Sleep,        ///< Put the selected bridge(s) in low-power sleep.
  Wake,         ///< Wake the selected bridge(s) without driving.
  SetCurrent,   ///< Set all four current thresholds.
  GetFaults,    ///< Read the legacy coil-service fault mask.
  CoilOff,      ///< Stop drive while leaving the bridge(s) awake.
  SetCurrentH1, ///< Set H1; options carry current units.
  SetCurrentH2, ///< Set H2; options carry current units.
  SetCurrentH3, ///< Set H3; options carry current units.
  SetCurrentH4, ///< Set H4; options carry current units.
  GetCurrentH1, ///< Read H1 configured current units.
  GetCurrentH2, ///< Read H2 configured current units.
  GetCurrentH3, ///< Read H3 configured current units.
  GetCurrentH4, ///< Read H4 configured current units.
  SetPmode,     ///< Set shared PMODE: zero PH/EN, one PWM.
  GetPmode,     ///< Read shared PMODE: zero PH/EN, one PWM.
};

/** @brief Bridge selector carried by direction, sleep, wake, and off options. */
enum class BridgeOptions : uint8_t {
  H1 = 0U, ///< Bridge H1.
  H2,      ///< Bridge H2.
  H3,      ///< Bridge H3.
  H4,      ///< Bridge H4.
  All,     ///< All four bridges.
};

/** @brief Setter response values. */
enum class CoilStatus : uint8_t {
  Succeeded = 0U, ///< Command accepted.
  Failed,         ///< Validation or hardware operation failed.
};

/** Current carried by one SetCurrent option unit. */
inline constexpr uint16_t CoilCurrentOptionStepMilliamps = 25U;

} // namespace dda
