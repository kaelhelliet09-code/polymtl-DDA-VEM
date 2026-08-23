#include "Service/Usb/UsbProtocol.h"

namespace dda {

uint8_t serializeUsbPacket(const UsbPacket &packet, uint8_t *output,
                           uint8_t outputCapacity) noexcept {
  if ((output == nullptr) ||
      (outputCapacity < UsbPacket::SerializedSize) ||
      (packet.requiresAnswer &&
       ((packet.options & ~UsbPacket::OptionsMask) != 0U))) {
    return 0U;
  }

  output[0] = packet.destination;
  output[1] = packet.command;
  output[2] = packet.requiresAnswer
                  ? static_cast<uint8_t>(packet.options |
                                         UsbPacket::RequiresAnswerMask)
                  : packet.options;
  return UsbPacket::SerializedSize;
}

bool decodeUsbPacket(const uint8_t *input, uint8_t inputLength,
                     UsbPacket &packet) noexcept {
  if ((input == nullptr) || (inputLength != UsbPacket::SerializedSize)) {
    return false;
  }

  packet.destination = input[0];
  packet.command = input[1];
  packet.options = static_cast<uint8_t>(input[2] & UsbPacket::OptionsMask);
  packet.requiresAnswer =
      (input[2] & UsbPacket::RequiresAnswerMask) != 0U;
  return true;
}

bool usbTimeoutElapsed(uint32_t startMilliseconds, uint32_t nowMilliseconds,
                       uint32_t timeoutMilliseconds) noexcept {
  return (nowMilliseconds - startMilliseconds) >= timeoutMilliseconds;
}

UsbReceiveAccumulator::UsbReceiveAccumulator() noexcept
    : _data{}, _readIndex(0U), _writeIndex(0U), _overflow(false) {}

bool UsbReceiveAccumulator::append(const uint8_t *data,
                                   uint32_t length) noexcept {
  if ((data == nullptr) || (length == 0U) ||
      (length >= static_cast<uint32_t>(Capacity))) {
    return false;
  }

  const uint16_t available = static_cast<uint16_t>(
      (static_cast<uint16_t>(_readIndex - _writeIndex) - 1U) & IndexMask);
  if (length > available) {
    _overflow = true;
    return false;
  }

  uint16_t writeIndex = _writeIndex;
  for (uint32_t index = 0U; index < length; ++index) {
    _data[writeIndex] = data[index];
    writeIndex = static_cast<uint16_t>((writeIndex + 1U) & IndexMask);
  }
  _writeIndex = writeIndex;
  return true;
}

uint16_t UsbReceiveAccumulator::size() const noexcept {
  return static_cast<uint16_t>((_writeIndex - _readIndex) & IndexMask);
}

bool UsbReceiveAccumulator::consume(uint8_t *output,
                                    uint16_t length) noexcept {
  if (((output == nullptr) && (length != 0U)) || (length > size())) {
    return false;
  }

  uint16_t readIndex = _readIndex;
  for (uint16_t index = 0U; index < length; ++index) {
    output[index] = _data[readIndex];
    readIndex = static_cast<uint16_t>((readIndex + 1U) & IndexMask);
  }
  _readIndex = readIndex;
  return true;
}

void UsbReceiveAccumulator::clear() noexcept {
  _readIndex = _writeIndex;
  _overflow = false;
}

bool UsbReceiveAccumulator::takeOverflow() noexcept {
  const bool overflow = _overflow;
  _overflow = false;
  return overflow;
}

} // namespace dda
