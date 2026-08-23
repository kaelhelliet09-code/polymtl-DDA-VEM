#include "Platform/Stm32/Usb/UsbTransport.h"
#include "Platform/Stm32/System/InterruptGuard.h"

#include <cstring>

extern "C" {
#include "stm32g0xx_hal.h"
#include "usbd_cdc_if.h"
}

namespace dda {

UsbTransport::UsbTransport() noexcept
    : _receiveAccumulator{}, _transmitBuffer{}, _transmitLength(0U),
      _transmitState(TxState::Empty), _transmitStartedMilliseconds(0U),
      _transmitResult(TransmitResult::None), _transmitCompleted(false),
      _connected(false), _connectedEvent(false), _disconnectedEvent(false) {}

void UsbTransport::init() noexcept {
  {
    InterruptGuard interruptGuard;
    _receiveAccumulator.clear();
    _transmitCompleted = false;
    _connectedEvent = false;
    _disconnectedEvent = false;
  }

  _transmitLength = 0U;
  _transmitState = TxState::Empty;
  _transmitStartedMilliseconds = 0U;
  _transmitResult = TransmitResult::None;
  _connected = false;
}

void UsbTransport::process() noexcept {
  processConnectionEvents();
  processTransmitCompletion();
  processTransmit();
}

bool UsbTransport::queuePacket(const UsbPacket &packet) noexcept {
  uint8_t bytes[UsbPacket::SerializedSize]{};
  const uint8_t length =
      serializeUsbPacket(packet, bytes, sizeof(bytes));
  if (length == 0U) {
    return false;
  }
  return queueBytes(bytes, length);
}

bool UsbTransport::queueBytes(const uint8_t *data, uint8_t length) noexcept {
  if (!_connected || (_transmitState != TxState::Empty) || (data == nullptr) ||
      (length == 0U) || (length > sizeof(_transmitBuffer))) {
    return false;
  }

  std::memcpy(_transmitBuffer, data, length);
  _transmitLength = length;
  _transmitState = TxState::Ready;
  _transmitStartedMilliseconds = HAL_GetTick();
  return true;
}

bool UsbTransport::isTransmitIdle() const noexcept {
  return _transmitState == TxState::Empty;
}

bool UsbTransport::isConnected() const noexcept { return _connected; }

bool UsbTransport::isOperational() const noexcept {
  return _connected && (_transmitState != TxState::TimedOut);
}

UsbTransport::TransmitResult UsbTransport::takeTransmitResult() noexcept {
  const TransmitResult result = _transmitResult;
  _transmitResult = TransmitResult::None;
  return result;
}

bool UsbTransport::takeReceiveOverflow() noexcept {
  InterruptGuard interruptGuard;
  return _receiveAccumulator.takeOverflow();
}

uint16_t UsbTransport::receivedSize() const noexcept {
  InterruptGuard interruptGuard;
  return _receiveAccumulator.size();
}

bool UsbTransport::consumeReceived(uint8_t *output,
                                   uint16_t length) noexcept {
  InterruptGuard interruptGuard;
  return _receiveAccumulator.consume(output, length);
}

void UsbTransport::clearReceived() noexcept {
  InterruptGuard interruptGuard;
  _receiveAccumulator.clear();
}

void UsbTransport::onReceive(const uint8_t *data, uint32_t length) noexcept {
  _connected = true;
  (void)_receiveAccumulator.append(data, length);
}

void UsbTransport::onTransmitComplete() noexcept {
  _transmitCompleted = true;
}

void UsbTransport::onConnected() noexcept { _connectedEvent = true; }

void UsbTransport::onDisconnected() noexcept { _disconnectedEvent = true; }

void UsbTransport::processConnectionEvents() noexcept {
  bool connected = false;
  bool disconnected = false;
  {
    InterruptGuard interruptGuard;
    connected = _connectedEvent;
    disconnected = _disconnectedEvent;
    _connectedEvent = false;
    _disconnectedEvent = false;
  }

  if (disconnected) {
    _connected = false;
    _transmitLength = 0U;
    _transmitState = TxState::Empty;
    _transmitCompleted = false;
    _transmitResult = TransmitResult::Disconnected;
  }
  if (connected) {
    _connected = true;
    if (!disconnected) {
      _transmitResult = TransmitResult::Connected;
    }
  }
}

void UsbTransport::processTransmitCompletion() noexcept {
  bool completed = false;
  {
    InterruptGuard interruptGuard;
    if (_transmitCompleted) {
      _transmitCompleted = false;
      completed = true;
    }
  }

  if (completed && (_transmitState == TxState::InFlight)) {
    _transmitLength = 0U;
    _transmitState = TxState::Empty;
    _transmitResult = TransmitResult::Complete;
  } else if (completed && (_transmitState == TxState::TimedOut)) {
    _transmitLength = 0U;
    _transmitState = TxState::Empty;
  }
}

void UsbTransport::processTransmit() noexcept {
  if ((_transmitState == TxState::InFlight) &&
      usbTimeoutElapsed(_transmitStartedMilliseconds, HAL_GetTick(),
                        config::UsbTransmitTimeoutMilliseconds)) {
    failTransmit(TransmitResult::TimedOut);
    return;
  }
  if (_transmitState != TxState::Ready) {
    return;
  }

  const uint8_t result = CDC_Transmit_FS(_transmitBuffer, _transmitLength);
  if (result == USBD_OK) {
    _transmitState = TxState::InFlight;
    _transmitStartedMilliseconds = HAL_GetTick();
  } else if (result != USBD_BUSY) {
    failTransmit(TransmitResult::Failed);
  } else if (usbTimeoutElapsed(_transmitStartedMilliseconds, HAL_GetTick(),
                               config::UsbTransmitTimeoutMilliseconds)) {
    failTransmit(TransmitResult::TimedOut);
  }
}

void UsbTransport::failTransmit(TransmitResult result) noexcept {
  _transmitResult = result;
  if (_transmitState == TxState::InFlight) {
    _transmitState = TxState::TimedOut;
  } else {
    _transmitLength = 0U;
    _transmitState = TxState::Empty;
  }
}

} // namespace dda
