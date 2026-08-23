#pragma once

#include "Config/UsbConfig.h"
#include "Service/Usb/UsbPacket.h"

#include <cstdint>

namespace dda {

uint8_t serializeUsbPacket(const UsbPacket &packet, uint8_t *output,
                           uint8_t outputCapacity) noexcept;

bool decodeUsbPacket(const uint8_t *input, uint8_t inputLength,
                     UsbPacket &packet) noexcept;

bool usbTimeoutElapsed(uint32_t startMilliseconds, uint32_t nowMilliseconds,
                       uint32_t timeoutMilliseconds) noexcept;

/** Callback-to-foreground receive ring. */
class UsbReceiveAccumulator {
public:
  static constexpr uint16_t Capacity =
      config::UsbApplicationReceiveRingCapacityBytes;

  static_assert((Capacity & (Capacity - 1U)) == 0U,
                "USB receive capacity must be a power of two");

  UsbReceiveAccumulator() noexcept;

  bool append(const uint8_t *data, uint32_t length) noexcept;
  uint16_t size() const noexcept;
  bool consume(uint8_t *output, uint16_t length) noexcept;
  void clear() noexcept;
  bool takeOverflow() noexcept;

private:
  static constexpr uint16_t IndexMask = Capacity - 1U;

  uint8_t _data[Capacity];
  volatile uint16_t _readIndex;
  volatile uint16_t _writeIndex;
  volatile bool _overflow;
};

} // namespace dda
