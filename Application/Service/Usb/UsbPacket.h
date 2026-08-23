#pragma once

#include <cstdint>

namespace dda {

/** Fixed request/response frame exchanged with the USB host. */
struct UsbPacket {
  static constexpr uint8_t SerializedSize = 3U;
  static constexpr uint8_t RequiresAnswerMask = 0x80U;
  static constexpr uint8_t OptionsMask = 0x7FU;

  uint8_t destination{0U};
  uint8_t command{0U};
  uint8_t options{0U};
  bool requiresAnswer{false};
};

} // namespace dda
