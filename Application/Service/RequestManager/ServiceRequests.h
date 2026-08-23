#pragma once

#include <cstdint>

namespace dda {

// Coil Controller request and options :
enum class CoilCommand : uint8_t {
  Forward = 0U,
  Reverse,
  Sleep,
  Wake,
  SetCurrent,
  GetFaults,
  CoilOff,
};

enum class BridgeOptions : uint8_t {
  H1 = 0U,
  H2,
  H3,
  H4,
  All,
};

enum class CoilStatus : uint8_t {
  Succeeded = 0U,
  Failed,
};

/** Current carried by one SetCurrent option unit. */
inline constexpr uint16_t CoilCurrentOptionStepMilliamps = 25U;

// USB request and options :

} // namespace dda
