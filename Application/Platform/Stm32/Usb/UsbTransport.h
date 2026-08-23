#pragma once

#include "Service/Usb/UsbProtocol.h"

#include <cstdint>

namespace dda {

/** Low-level USB CDC transmit and receive buffering. */
class UsbTransport {
public:
  enum class TransmitResult : uint8_t {
    None,
    Connected,
    Complete,
    Failed,
    TimedOut,
    Disconnected,
  };

  UsbTransport() noexcept;

  void init() noexcept;
  void process() noexcept;

  bool queuePacket(const UsbPacket &packet) noexcept;
  bool queueBytes(const uint8_t *data, uint8_t length) noexcept;
  bool isTransmitIdle() const noexcept;
  bool isConnected() const noexcept;
  bool isOperational() const noexcept;
  TransmitResult takeTransmitResult() noexcept;

  bool takeReceiveOverflow() noexcept;
  uint16_t receivedSize() const noexcept;
  bool consumeReceived(uint8_t *output, uint16_t length) noexcept;
  void clearReceived() noexcept;

  /** Called from the generated CDC receive callback. */
  void onReceive(const uint8_t *data, uint32_t length) noexcept;

  /** Called from the generated CDC transmit-complete callback. */
  void onTransmitComplete() noexcept;
  void onConnected() noexcept;
  void onDisconnected() noexcept;

private:
  enum class TxState : uint8_t { Empty, Ready, InFlight, TimedOut };

  void processConnectionEvents() noexcept;
  void processTransmitCompletion() noexcept;
  void processTransmit() noexcept;
  void failTransmit(TransmitResult result) noexcept;

  UsbReceiveAccumulator _receiveAccumulator;
  uint8_t _transmitBuffer[config::UsbCdcMaximumPacketBytes];
  uint8_t _transmitLength;
  TxState _transmitState;
  uint32_t _transmitStartedMilliseconds;
  TransmitResult _transmitResult;
  volatile bool _transmitCompleted;
  volatile bool _connected;
  volatile bool _connectedEvent;
  volatile bool _disconnectedEvent;
};

} // namespace dda
