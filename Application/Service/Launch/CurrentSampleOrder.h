/** @file CurrentSampleOrder.h @brief Normalizes ADC scan order to bridge order. */
#pragma once

#include <cstdint>

namespace dda {

inline constexpr uint32_t BridgeCurrentChannelCount = 4U;

/**
 * @brief Convert each STM32 fixed ADC scan from channel order to H1-H4 order.
 *
 * ADC channels are scanned numerically. The .ioc pin labels therefore produce
 * H1, H3, H4, H2 for channels 3, 8, 15, and 18 respectively. The launch wire
 * format is explicitly H1, H2, H3, H4.
 */
inline void normalizeBridgeCurrentSamples(uint8_t *values,
                                          uint32_t sampleCount) noexcept {
  if (values == nullptr) {
    return;
  }
  for (uint32_t sample = 0U; sample < sampleCount; ++sample) {
    uint8_t *const channels = values + (sample * BridgeCurrentChannelCount);
    const uint8_t h2 = channels[3U];
    channels[3U] = channels[2U];
    channels[2U] = channels[1U];
    channels[1U] = h2;
  }
}

} // namespace dda
