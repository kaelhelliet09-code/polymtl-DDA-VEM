/**
 * @file UsbConfig.h
 * @brief USB CDC buffering and framing configuration.
 *
 * These values configure the user-owned transport. Endpoint packet size must
 * remain consistent with the CubeMX CDC descriptors and generated buffers.
 */

#pragma once

#include <cstdint>

namespace dda::config {

inline constexpr uint16_t UsbCdcMaximumPacketBytes = 64U;
///< Full-speed bulk endpoint maximum packet size from the USB descriptors.
inline constexpr uint16_t UsbApplicationReceiveRingCapacityBytes = 256U;
///< User-owned RX ring; sized for two packets plus parsing headroom.
inline constexpr uint32_t UsbPartialRequestTimeoutMilliseconds = 100U;
///< Time allowed for the remainder of a fragmented command frame.
inline constexpr uint32_t UsbTransmitTimeoutMilliseconds = 1'000U;
///< Maximum time for one queued or in-flight CDC packet.

static_assert(UsbApplicationReceiveRingCapacityBytes >
                  (2U * UsbCdcMaximumPacketBytes),
              "The RX ring must hold two complete OUT packets");

} // namespace dda::config
