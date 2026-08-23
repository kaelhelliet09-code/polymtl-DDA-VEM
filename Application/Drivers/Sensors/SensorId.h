/** @file SensorId.h @brief Declares logical external-sensor identifiers. */

#pragma once

#include <cstdint>

namespace dda {
/** @brief Identifies one of the four fixed external-sensor channels. */
enum class SensorId : uint8_t {
  SENSOR_1 = 0U, ///< First sensor channel.
  SENSOR_2 = 1U, ///< Second sensor channel.
  SENSOR_3 = 2U, ///< Third sensor channel.
  SENSOR_4 = 3U, ///< Fourth sensor channel.
};
} // namespace dda
