#include "Service/Usb/USBcontroller.h"

#include "Config/UsbConfig.h"
#include "Service/Launch/LaunchDataSource.h"

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

namespace {

constexpr uint8_t LaunchFramePayloadSize = static_cast<uint8_t>(
    config::UsbCdcMaximumPacketBytes - launch_wire::HeaderSize);

void writeUint32(uint8_t *output, uint32_t value) noexcept {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8U);
  output[2] = static_cast<uint8_t>(value >> 16U);
  output[3] = static_cast<uint8_t>(value >> 24U);
}

uint32_t updateCrc32(uint32_t crc, const uint8_t *data,
                     uint8_t length) noexcept {
  for (uint8_t index = 0U; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? launch_wire::CrcPolynomial : 0U);
    }
  }
  return crc;
}

} // namespace

USBcontroller::USBcontroller(RequestManager &requestManager) noexcept
    : _transport{}, _requestManager(requestManager),
      _partialPacketActive(false), _partialPacketStartMilliseconds(0U),
      _launchData(nullptr), _launchDataOffset(0U), _launchDataSize(0U),
      _launchDataCrc(0xFFFFFFFFUL), _launchChunkIndex(0U), _launchRunId(0U),
      _launchTransferState(LaunchTransferState::Idle),
      _launchFrameQueued(false), _queuedFrameIsFinal(false),
      _queuedPayloadLength(0U), _queuedCrc(0xFFFFFFFFUL),
      _deferLaunchFrame(false), _communicationFailure(false),
      _communicationHealthy(true) {
  _requestManager.registerService(Service::UsbControl, *this);
}

void USBcontroller::processLaunchDataTransfer() noexcept {
  if (_deferLaunchFrame) {
    _deferLaunchFrame = false;
    return;
  }
  if ((_launchTransferState == LaunchTransferState::Sending) &&
      !_launchFrameQueued && _transport.isTransmitIdle()) {
    (void)queueNextLaunchFrame();
  }
}

bool USBcontroller::startLaunchDataTransfer(const LaunchDataSource &data,
                                            uint8_t runId) noexcept {
  if (isLaunchDataTransferActive() || !_transport.isConnected()) {
    return false;
  }
  _launchData = &data;
  _launchDataSize = data.serializedSize();
  _launchRunId = runId;
  resetLaunchTransferProgress();
  _deferLaunchFrame = true;
  _launchTransferState = LaunchTransferState::Sending;
  return true;
}

void USBcontroller::cancelLaunchDataTransfer() noexcept {
  _launchData = nullptr;
  _launchDataSize = 0U;
  _launchTransferState = LaunchTransferState::Idle;
  _deferLaunchFrame = false;
  resetLaunchTransferProgress();
}

bool USBcontroller::isLaunchDataTransferActive() const noexcept {
  return _launchTransferState != LaunchTransferState::Idle;
}

bool USBcontroller::isCommunicationReady() const noexcept {
  return _transport.isOperational() && _communicationHealthy;
}

bool USBcontroller::takeCommunicationFailure() noexcept {
  const bool failed = _communicationFailure;
  _communicationFailure = false;
  return failed;
}

void USBcontroller::init() noexcept {
  _transport.init();
  resetReceiveState();
  cancelLaunchDataTransfer();
  _communicationFailure = false;
  _communicationHealthy = true;
}

void USBcontroller::process() noexcept {
  _transport.process();
  const UsbTransport::TransmitResult result = _transport.takeTransmitResult();
  if (result != UsbTransport::TransmitResult::None) {
    handleTransmitResult(result);
  }

  if (!_requestManager.hasCapacity()) {
    return;
  }

  UsbPacket packet{};
  switch (takeNextPacket(packet)) {
  case ReceiveState::Complete:
    (void)queueHostRequest(packet);
    break;
  case ReceiveState::Invalid:
    _transport.clearReceived();

    resetReceiveState();
    break;
  case ReceiveState::NotReady:
    break;
  }
}

void USBcontroller::processRequest(Request &request) noexcept {
  if ((request.state != RequestState::Outgoing) ||
      (request.destination != Service::UsbControl)) {
    return;
  }

  if (sendServiceResponse(request)) {
    request.complete(Service::UsbControl);
  }
}

UsbTransport &USBcontroller::transport() noexcept { return _transport; }

USBcontroller::ReceiveState
USBcontroller::takeNextPacket(UsbPacket &packet) noexcept {
  if (_transport.takeReceiveOverflow()) {
    return ReceiveState::Invalid;
  }

  if (_transport.receivedSize() < UsbPacket::SerializedSize) {
    return waitForPacketBytes();
  }

  uint8_t bytes[UsbPacket::SerializedSize]{};
  if (!_transport.consumeReceived(bytes, UsbPacket::SerializedSize)) {
    return ReceiveState::NotReady;
  }
  resetReceiveState();
  return decodeUsbPacket(bytes, UsbPacket::SerializedSize, packet)
             ? ReceiveState::Complete
             : ReceiveState::Invalid;
}

USBcontroller::ReceiveState USBcontroller::waitForPacketBytes() noexcept {
  if (_transport.receivedSize() == 0U) {
    resetReceiveState();
    return ReceiveState::NotReady;
  }

  const uint32_t nowMilliseconds = HAL_GetTick();
  if (!_partialPacketActive) {
    _partialPacketActive = true;
    _partialPacketStartMilliseconds = nowMilliseconds;
    return ReceiveState::NotReady;
  }

  if (usbTimeoutElapsed(_partialPacketStartMilliseconds, nowMilliseconds,
                        config::UsbPartialRequestTimeoutMilliseconds)) {
    _communicationFailure = true;
    _communicationHealthy = false;
    return ReceiveState::Invalid;
  }
  return ReceiveState::NotReady;
}

bool USBcontroller::queueHostRequest(const UsbPacket &packet) noexcept {
  if (!_communicationHealthy) {
    return false;
  }
  const uint8_t destination = packet.destination;
  if ((destination == static_cast<uint8_t>(Service::None)) ||
      (destination >= static_cast<uint8_t>(Service::Count)) ||
      (destination == static_cast<uint8_t>(Service::UsbControl))) {
    return false;
  }

  Request request{};
  request.destination = static_cast<Service>(destination);
  request.source = Service::UsbControl;
  request.command = packet.command;
  request.options = packet.options;
  request.requiresAnswer = packet.requiresAnswer;
  return _requestManager.queueRequest(request);
}

bool USBcontroller::sendServiceResponse(const Request &request) noexcept {
  if (!_transport.isTransmitIdle()) {
    return false;
  }

  return _transport.queuePacket({static_cast<uint8_t>(request.source),
                                 request.command, request.options, false});
}

void USBcontroller::resetReceiveState() noexcept {
  _partialPacketActive = false;
  _partialPacketStartMilliseconds = 0U;
}

void USBcontroller::resetLaunchTransferProgress() noexcept {
  _launchDataOffset = 0U;
  _launchDataCrc = 0xFFFFFFFFUL;
  _launchChunkIndex = 0U;
  _launchFrameQueued = false;
  _queuedFrameIsFinal = false;
  _queuedPayloadLength = 0U;
  _queuedCrc = 0xFFFFFFFFUL;
}

void USBcontroller::handleTransmitResult(
    UsbTransport::TransmitResult result) noexcept {
  if (result == UsbTransport::TransmitResult::Connected) {
    _communicationHealthy = true;
    return;
  }
  if (result == UsbTransport::TransmitResult::Complete) {
    if (_launchFrameQueued) {
      commitQueuedLaunchFrame();
    }
    return;
  }
  if ((result == UsbTransport::TransmitResult::Failed) ||
      (result == UsbTransport::TransmitResult::TimedOut) ||
      (result == UsbTransport::TransmitResult::Disconnected)) {
    _transport.clearReceived();
    resetReceiveState();
    cancelLaunchDataTransfer();
    _communicationFailure = true;
    _communicationHealthy = false;
  }
}

void USBcontroller::commitQueuedLaunchFrame() noexcept {
  _launchFrameQueued = false;
  if (_queuedFrameIsFinal) {
    cancelLaunchDataTransfer();
    return;
  }
  _launchDataOffset += _queuedPayloadLength;
  _launchDataCrc = _queuedCrc;
  ++_launchChunkIndex;
}

bool USBcontroller::queueNextLaunchFrame() noexcept {
  uint8_t frame[config::UsbCdcMaximumPacketBytes]{};
  frame[0] = launch_wire::Marker;
  frame[1] = launch_wire::Version;
  frame[3] = _launchRunId;
  frame[4] = static_cast<uint8_t>(_launchChunkIndex);
  frame[5] = static_cast<uint8_t>(_launchChunkIndex >> 8U);

  if (_launchDataOffset < _launchDataSize) {
    frame[2] = launch_wire::DataFrame;
    const uint8_t payloadLength = _launchData->serialize(
        _launchDataOffset, frame + launch_wire::HeaderSize,
        LaunchFramePayloadSize);
    if (payloadLength == 0U) {
      return false;
    }
    frame[6] = payloadLength;
    if (!_transport.queueBytes(
            frame,
            static_cast<uint8_t>(launch_wire::HeaderSize + payloadLength))) {
      return false;
    }
    _launchFrameQueued = true;
    _queuedFrameIsFinal = false;
    _queuedPayloadLength = payloadLength;
    _queuedCrc = updateCrc32(_launchDataCrc, frame + launch_wire::HeaderSize,
                             payloadLength);
    return true;
  }

  frame[2] = launch_wire::FinalFrame;
  frame[6] = 8U;
  writeUint32(frame + launch_wire::HeaderSize, _launchDataSize);
  writeUint32(frame + launch_wire::HeaderSize + 4U,
              _launchDataCrc ^ 0xFFFFFFFFUL);
  if (!_transport.queueBytes(
          frame, static_cast<uint8_t>(launch_wire::HeaderSize + frame[6]))) {
    return false;
  }
  _launchFrameQueued = true;
  _queuedFrameIsFinal = true;
  _queuedPayloadLength = 0U;
  _queuedCrc = _launchDataCrc;
  return true;
}

} // namespace dda
