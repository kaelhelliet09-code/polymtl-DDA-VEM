#pragma once

#include <cstdint>

namespace dda {

enum class LaunchCommand : uint8_t {
  StartRun = 0U, // options: run ID
  StopRun,
  AbortRun,
  SetSamplingRate, // options: frequency in 100 Hz units (1..50)
  SetDebugMode,    // options: zero for competition, nonzero for debug
  RunStatus,
};

enum class LaunchStatus : uint8_t {
  Success = 0U,
  TimedOut,
  HostAborted,
  AcquisitionError,
  SafetyFault,
  Busy,
  InvalidCommand,
};

} // namespace dda
