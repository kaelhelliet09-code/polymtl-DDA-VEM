/** @file ExternalDacConfig.h @brief Exclusive ownership of all DAC outputs. */
#pragma once

#include "Config/BoardConfig.h"
#include "Drivers/DacChannel.h"

#include <cstddef>
#include <cstdint>

namespace dda::config {

/** @brief VTRIP channels assigned to sensors S1 through S4. */
inline constexpr DacChannel SensorTripVoltageDacChannels[SensorCount] = {
    DAC_CHANNEL_A, DAC_CHANNEL_B, DAC_CHANNEL_F, DAC_CHANNEL_E};

/** @brief VREF current-limit channels assigned to bridges H1 through H4. */
inline constexpr DacChannel DriverCurrentLimitDacChannels[DriverCount] = {
    DAC_CHANNEL_C, DAC_CHANNEL_D, DAC_CHANNEL_H, DAC_CHANNEL_G};

namespace detail {

/**
 * @brief Verify that every external-DAC channel has exactly one owner.
 * @return Whether the combined channel map is valid and unique.
 */
constexpr bool externalDacChannelsAreUnique() noexcept {
  bool used[8] = {};
  for (const DacChannel channel : SensorTripVoltageDacChannels) {
    const auto index = static_cast<uint8_t>(channel);
    if ((index >= 8U) || used[index]) {
      return false;
    }
    used[index] = true;
  }
  for (const DacChannel channel : DriverCurrentLimitDacChannels) {
    const auto index = static_cast<uint8_t>(channel);
    if ((index >= 8U) || used[index]) {
      return false;
    }
    used[index] = true;
  }
  return true;
}

} // namespace detail

static_assert(SensorCount + DriverCount == 8U,
              "The revised board assigns all eight external-DAC outputs");
static_assert(detail::externalDacChannelsAreUnique(),
              "VTRIP and driver VREF channels must not overlap");

} // namespace dda::config
