/** @file CurrentSampleOrder.h @brief Normalizes ADC scan order to bridge order. */
#pragma once

#include <cstdint>

namespace dda {

/** @brief Number of bridge-current channels in each interleaved ADC scan. */
inline constexpr uint32_t BridgeCurrentChannelCount = 4U;

/**
 * @brief Convert each STM32 fixed ADC scan from channel order to H1-H4 order.
 *
 * ADC channels are scanned numerically. The revised .ioc pin labels produce
 * H1, H2, H4, H3 for channels 0, 4, 11, and 18 respectively. The launch wire
 * format remains explicitly H1, H2, H3, H4.
 * @param values Interleaved mutable ADC byte samples.
 * @param sampleCount Number of complete four-channel scans.
 */
inline void normalizeBridgeCurrentSamples(uint8_t *values,
                                          uint32_t sampleCount) noexcept {
  if (values == nullptr) {
    return;
  }
  for (uint32_t sample = 0U; sample < sampleCount; ++sample) {
    uint8_t *const channels = values + (sample * BridgeCurrentChannelCount);
    const uint8_t h3 = channels[3U];
    channels[3U] = channels[2U];
    channels[2U] = h3;
  }
}

} // namespace dda
