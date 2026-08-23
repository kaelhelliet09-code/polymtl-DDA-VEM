#pragma once

#include "Platform/Stm32/Usb/UsbTransport.h"
#include "Service/RequestManager/RequestManager.h"

#include <cstdint>

namespace dda {

class LaunchDataSource;

/**
 * @brief Converts USB frames to requests and responses back to USB frames.
 * @details Normal commands use fixed three-byte packets. LaunchData uses a
 * separate chunked, CRC-protected stream whose source remains owned by
 * LaunchManager throughout transmission.
 */
class USBcontroller {
public:
  /** @brief Bind the USB service to the shared router. @param requestManager Router. */
  explicit USBcontroller(RequestManager &requestManager) noexcept;

  /** @brief Initialize CDC transport and clear transfer state. */
  void init() noexcept;
  /** @brief Process received packets and transport completion events. */
  void process() noexcept;
  /** @brief Queue the next LaunchData frame when the transport is idle. */
  void processLaunchDataTransfer() noexcept;

  /**
   * @brief Begin a chunked LaunchData transfer.
   * @param data Process-lifetime source retained until transfer finishes.
   * @param runId Launch identifier copied into every frame.
   * @return `true` when the transfer was accepted.
   */
  bool startLaunchDataTransfer(const LaunchDataSource &data,
                               uint8_t runId) noexcept;
  /** @brief Cancel the current LaunchData transfer and discard its progress. */
  void cancelLaunchDataTransfer() noexcept;
  /** @brief Report whether LaunchData is being sent. @return Transfer state. */
  bool isLaunchDataTransferActive() const noexcept;
  /** @brief Report whether CDC can accept application traffic. @return Readiness. */
  bool isCommunicationReady() const noexcept;
  /** @brief Consume the communication-failure latch. @return Previous latch. */
  bool takeCommunicationFailure() noexcept;

  /**
   * @brief Queue a RequestManager response addressed to USB.
   * @param request Outgoing response request.
   */
  void processRequest(Request &request) noexcept;

  /** @brief Access the owned CDC transport. @return USB transport. */
  UsbTransport &transport() noexcept;

private:
  enum class ReceiveState : uint8_t { NotReady, Invalid, Complete };
  enum class LaunchTransferState : uint8_t { Idle, Sending };

  ReceiveState takeNextPacket(UsbPacket &packet) noexcept;
  ReceiveState waitForPacketBytes() noexcept;
  bool queueHostRequest(const UsbPacket &packet) noexcept;
  bool sendServiceResponse(const Request &request) noexcept;
  void resetReceiveState() noexcept;
  void resetLaunchTransferProgress() noexcept;
  void handleTransmitResult(UsbTransport::TransmitResult result) noexcept;
  void commitQueuedLaunchFrame() noexcept;
  bool queueNextLaunchFrame() noexcept;

  UsbTransport _transport;
  RequestManager &_requestManager;
  bool _partialPacketActive;
  uint32_t _partialPacketStartMilliseconds;
  const LaunchDataSource *_launchData;
  uint32_t _launchDataOffset;
  uint32_t _launchDataSize;
  uint32_t _launchDataCrc;
  uint16_t _launchChunkIndex;
  uint8_t _launchRunId;
  LaunchTransferState _launchTransferState;
  bool _launchFrameQueued;
  bool _queuedFrameIsFinal;
  uint8_t _queuedPayloadLength;
  uint32_t _queuedCrc;
  bool _deferLaunchFrame;
  bool _communicationFailure;
  bool _communicationHealthy;
};

} // namespace dda
