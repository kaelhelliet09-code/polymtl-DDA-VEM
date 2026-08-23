// Implements the narrow C-to-C++ bridge used by generated CDC callbacks.
// Callbacks remain inert until the application-owned transport is published.
#include "Platform/Stm32/Usb/UsbCdcBridge.h"

#include "Platform/Stm32/System/InterruptGuard.h"
#include "Platform/Stm32/Usb/UsbTransport.h"

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace {

// Generated callbacks can run only after BoardApplication publishes this
// non-owning target. Null leaves early startup callbacks harmless.
dda::UsbTransport *_transport = nullptr;

bool isBridgeInitialized() noexcept { return _transport != nullptr; }

} // namespace

namespace dda {

void bindUsbTransport(UsbTransport &transport) noexcept {
  // Publish the target atomically so callbacks cannot observe a partially
  // written pointer.
  InterruptGuard interruptGuard;
  _transport = &transport;
}

} // namespace dda

extern "C" void DdaUsbCdc_OnReceive(const uint8_t *data, uint32_t length) {
  /// Here instead put _requestManager.queue resquest.
  if (isBridgeInitialized()) {
    _transport->onReceive(data, length);
  }
}

extern "C" void DdaUsbCdc_OnTransmitComplete(void) {
  if (isBridgeInitialized()) {
    _transport->onTransmitComplete();
  }
}

extern "C" void DdaUsbCdc_OnConnected(void) {
  if (isBridgeInitialized()) {
    _transport->onConnected();
  }
}

extern "C" void DdaUsbCdc_OnDisconnected(void) {
  if (isBridgeInitialized()) {
    _transport->onDisconnected();
  }
}
