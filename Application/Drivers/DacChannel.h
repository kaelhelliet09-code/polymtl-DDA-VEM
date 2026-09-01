/** @file DacChannel.h @brief DAC088S085 output-channel addresses. */
#pragma once

#include <cstdint>

namespace dda {

/** @brief DAC088S085 output-channel address encoded in a command frame. */
enum DacChannel : uint8_t {
  DAC_CHANNEL_A = 0x00U, ///< Output A.
  DAC_CHANNEL_B = 0x01U, ///< Output B.
  DAC_CHANNEL_C = 0x02U, ///< Output C.
  DAC_CHANNEL_D = 0x03U, ///< Output D.
  DAC_CHANNEL_E = 0x04U, ///< Output E.
  DAC_CHANNEL_F = 0x05U, ///< Output F.
  DAC_CHANNEL_G = 0x06U, ///< Output G.
  DAC_CHANNEL_H = 0x07U  ///< Output H.
};

} // namespace dda
