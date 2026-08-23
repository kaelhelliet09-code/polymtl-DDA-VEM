#pragma once

#include <cstdint>

namespace dda {

namespace launch_wire {
/** @brief LaunchData frame marker. */
inline constexpr uint8_t Marker = 0xDAU;
/** @brief Current LaunchData wire-format version. */
inline constexpr uint8_t Version = 4U;
/** @brief Frame type containing a serialized-data segment. */
inline constexpr uint8_t DataFrame = 0U;
/** @brief Frame type containing total length and CRC-32. */
inline constexpr uint8_t FinalFrame = 1U;
/** @brief Bytes preceding each frame payload. */
inline constexpr uint8_t HeaderSize = 8U;
/** @brief Reflected CRC-32/ISO-HDLC polynomial. */
inline constexpr uint32_t CrcPolynomial = 0xEDB88320UL;
} // namespace launch_wire

/**
 * @brief Random-access serialized source for chunked LaunchData transmission.
 * @details USBcontroller requests bounded segments so the complete launch data
 * never needs a second contiguous RAM buffer.
 */
class LaunchDataSource {
public:
  /** @brief Permit destruction through the abstract interface. */
  virtual ~LaunchDataSource() = default;
  /** @brief Return the complete serialized byte count. @return Byte count. */
  virtual uint32_t serializedSize() const noexcept = 0;
  /**
   * @brief Serialize one bounded segment.
   * @param offset Byte offset in the logical serialized stream.
   * @param output Destination buffer.
   * @param capacity Available destination bytes.
   * @return Number of bytes written.
   */
  virtual uint8_t serialize(uint32_t offset, uint8_t *output,
                            uint8_t capacity) const noexcept = 0;
};

} // namespace dda
