/**
 * @file I2cDevice.h
 * @brief Declares guarded blocking and DMA-backed I2C device access.
 * @details Serializes transactions and owns the callback context required for
 * one asynchronous register read at a time.
 */

#pragma once

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"

/**
 * @brief Routes a HAL register-read completion to the active wrapper.
 * @param handle I2C peripheral reporting completion.
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *handle);

/**
 * @brief Routes a HAL bus or transfer error to the active wrapper.
 * @param handle I2C peripheral reporting the error.
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *handle);

/**
 * @brief Routes HAL abort completion to the active wrapper.
 * @param handle I2C peripheral reporting abort completion.
 */
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *handle);
}

namespace dda {

/**
 * @brief Provides raw writes and 16-bit big-endian register transactions.
 * @note Only one asynchronous register read may be active across all wrapper
 * instances because HAL callbacks do not carry user context.
 */
class I2cDevice {
public:
  /**
   * @brief Binds a wrapper to an I2C peripheral and 7-bit target address.
   * @param handle Non-owning HAL I2C handle.
   * @param deviceAddress Unshifted seven-bit target address.
   */
  I2cDevice(I2C_HandleTypeDef &handle, uint8_t deviceAddress) noexcept;

  /**
   * @brief Changes the target address used by later transactions.
   * @param deviceAddress Unshifted seven-bit target address.
   * @return `HAL_OK` when accepted, otherwise `HAL_BUSY` or `HAL_ERROR`.
   */
  HAL_StatusTypeDef setDeviceAddress(uint8_t deviceAddress) noexcept;

  /**
   * @brief Writes one 16-bit register value synchronously, MSB first.
   * @param registerAddress Eight-bit target register address.
   * @param value Register value to write.
   * @param timeoutMs HAL transaction timeout.
   * @return HAL transaction status.
   */
  HAL_StatusTypeDef writeRegister(uint8_t registerAddress, uint16_t value,
                                  uint32_t timeoutMs) noexcept;

  /**
   * @brief Reads one 16-bit register value synchronously, MSB first.
   * @param registerAddress Eight-bit target register address.
   * @param[out] value Decoded register value on success.
   * @param timeoutMs HAL transaction timeout.
   * @return HAL transaction status.
   */
  HAL_StatusTypeDef readRegister(uint8_t registerAddress, uint16_t &value,
                                 uint32_t timeoutMs) noexcept;

  /**
   * @brief Polls the target address for an acknowledgement.
   * @param trials Maximum HAL acknowledgement attempts.
   * @param timeoutMs Timeout for each attempt.
   * @return HAL readiness status.
   */
  HAL_StatusTypeDef isDeviceReady(uint32_t trials,
                                  uint32_t timeoutMs) noexcept;

  /**
   * @brief Writes raw bytes to a target with no register-address phase.
   * @param data Source bytes.
   * @param length Number of bytes to transmit.
   * @param timeoutMs HAL transaction timeout.
   * @return HAL transaction status.
   */
  HAL_StatusTypeDef transmit(const uint8_t *data, uint16_t length,
                             uint32_t timeoutMs) noexcept;

  /**
   * @brief Starts a DMA-backed 16-bit register read.
   * @param registerAddress 8-bit target register address.
   * @return Status from starting the HAL transfer, or `HAL_BUSY` if occupied.
   */
  HAL_StatusTypeDef readRegisterDma(uint8_t registerAddress) noexcept;

  /**
   * @brief Requests asynchronous abortion of the active DMA read.
   * @return HAL status from requesting the abort.
   */
  HAL_StatusTypeDef requestAbort() noexcept;

  /**
   * @brief Consumes a completed asynchronous read and decodes its value.
   * @param[out] value Decoded register value when completion is available.
   * @return `true` when one completed value was consumed.
   */
  bool takeReadComplete(uint16_t &value) noexcept;

  /**
   * @brief Consumes an asynchronous transfer error or abort notification.
   * @return `true` when one error notification was consumed.
   */
  bool takeError() noexcept;

  /**
   * @brief Reports whether a DMA read or abort is still in flight.
   * @return `true` while asynchronous HAL work remains active.
   */
  bool isBusy() const noexcept;

private:
  /** @brief Internal lifecycle of the shared asynchronous transfer buffer. */
  enum class AsyncState : uint8_t {
    Idle,
    Reading,
    ReadComplete,
    Aborting,
    Error,
  };

  friend void ::HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *handle);
  friend void ::HAL_I2C_ErrorCallback(I2C_HandleTypeDef *handle);
  friend void ::HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *handle);

  /** @brief Wrapper currently owning the process-wide HAL callback context. */
  static I2cDevice *volatile _activeTransaction;

  /** @brief Publishes a terminal state for the matching callback handle. */
  static void completeTransaction(I2C_HandleTypeDef *handle,
                                  AsyncState completionState) noexcept;

  I2C_HandleTypeDef &_handle; ///< Non-owning HAL handle reference.
  uint16_t _deviceAddress;    ///< HAL-formatted 8-bit target address.
  uint8_t _transactionData[2]; ///< Persistent big-endian DMA buffer.
  volatile AsyncState _asyncState; ///< Callback/foreground lifecycle state.
};

} // namespace dda
