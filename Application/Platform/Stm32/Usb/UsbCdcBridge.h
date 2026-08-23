/**
 * @file UsbCdcBridge.h
 * @brief Connects generated USB CDC callbacks to preinitialized C++ services.
 * @details Keeps the generated C callback surface independent of application
 * construction while forwarding only to explicitly published objects.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace dda {

class UsbTransport;

/**
 * @brief Publish the process-lifetime callback target before USB interrupts
 * run.
 * @param transport Initialized CDC transport.
 */
void bindUsbTransport(UsbTransport &transport) noexcept;

} // namespace dda

extern "C" {
#endif

/**
 * @brief Forward one generated CDC receive callback when the bridge is bound.
 * @param data Received CDC bytes.
 * @param length Number of received bytes.
 */
void DdaUsbCdc_OnReceive(const uint8_t *data, uint32_t length);

/** @brief Forward generated CDC transmit completion after bridge binding. */
void DdaUsbCdc_OnTransmitComplete(void);

/** @brief Notify the transport that the CDC interface was configured. */
void DdaUsbCdc_OnConnected(void);

/** @brief Notify the transport that the CDC interface was deinitialized. */
void DdaUsbCdc_OnDisconnected(void);

#ifdef __cplusplus
}
#endif
