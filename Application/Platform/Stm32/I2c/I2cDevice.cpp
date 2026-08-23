// Implements serialized blocking transactions and one callback-owned DMA read.
#include "Platform/Stm32/I2c/I2cDevice.h"

namespace dda {

I2cDevice *volatile I2cDevice::_activeTransaction = nullptr;

I2cDevice::I2cDevice(I2C_HandleTypeDef &handle, uint8_t deviceAddress) noexcept
    : _handle(handle),
      _deviceAddress(static_cast<uint16_t>((deviceAddress & 0x7FU) << 1U)),
      _transactionData{0U, 0U}, _asyncState(AsyncState::Idle) {}

HAL_StatusTypeDef I2cDevice::setDeviceAddress(uint8_t deviceAddress) noexcept {
  if ((_asyncState != AsyncState::Idle) || (_activeTransaction != nullptr)) {
    return HAL_BUSY;
  }

  _deviceAddress = static_cast<uint16_t>((deviceAddress & 0x7FU) << 1U);
  return HAL_OK;
}

HAL_StatusTypeDef I2cDevice::writeRegister(uint8_t registerAddress,
                                           uint16_t value,
                                           uint32_t timeoutMs) noexcept {
  if ((_asyncState != AsyncState::Idle) || (_activeTransaction != nullptr)) {
    return HAL_BUSY;
  }

  uint8_t data[2] = {static_cast<uint8_t>(value >> 8U),
                     static_cast<uint8_t>(value)};
  return HAL_I2C_Mem_Write(&_handle, _deviceAddress, registerAddress,
                           I2C_MEMADD_SIZE_8BIT, data, sizeof(data), timeoutMs);
}

HAL_StatusTypeDef I2cDevice::readRegister(uint8_t registerAddress,
                                          uint16_t &value,
                                          uint32_t timeoutMs) noexcept {
  if ((_asyncState != AsyncState::Idle) || (_activeTransaction != nullptr)) {
    return HAL_BUSY;
  }

  uint8_t data[2] = {0U, 0U};
  const HAL_StatusTypeDef status =
      HAL_I2C_Mem_Read(&_handle, _deviceAddress, registerAddress,
                       I2C_MEMADD_SIZE_8BIT, data, sizeof(data), timeoutMs);
  if (status == HAL_OK) {
    value =
        static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) | data[1]);
  }
  return status;
}

HAL_StatusTypeDef I2cDevice::isDeviceReady(uint32_t trials,
                                           uint32_t timeoutMs) noexcept {
  if ((trials == 0U) || (timeoutMs == 0U)) {
    return HAL_ERROR;
  }
  if ((_asyncState != AsyncState::Idle) || (_activeTransaction != nullptr)) {
    return HAL_BUSY;
  }
  return HAL_I2C_IsDeviceReady(&_handle, _deviceAddress, trials, timeoutMs);
}

HAL_StatusTypeDef I2cDevice::transmit(const uint8_t *data, uint16_t length,
                                      uint32_t timeoutMs) noexcept {
  if ((data == nullptr) || (length == 0U) || (timeoutMs == 0U)) {
    return HAL_ERROR;
  }
  if ((_asyncState != AsyncState::Idle) || (_activeTransaction != nullptr)) {
    return HAL_BUSY;
  }
  return HAL_I2C_Master_Transmit(
      &_handle, _deviceAddress, const_cast<uint8_t *>(data), length, timeoutMs);
}

HAL_StatusTypeDef I2cDevice::readRegisterDma(uint8_t registerAddress) noexcept {
  if ((_asyncState != AsyncState::Idle) || (_activeTransaction != nullptr)) {
    return HAL_BUSY;
  }

  _transactionData[0] = 0U;
  _transactionData[1] = 0U;
  _asyncState = AsyncState::Reading;
  _activeTransaction = this;

  const HAL_StatusTypeDef status = HAL_I2C_Mem_Read_DMA(
      &_handle, _deviceAddress, registerAddress, I2C_MEMADD_SIZE_8BIT,
      _transactionData, sizeof(_transactionData));
  if (status != HAL_OK) {
    _activeTransaction = nullptr;
    _asyncState = AsyncState::Idle;
  }
  return status;
}

HAL_StatusTypeDef I2cDevice::requestAbort() noexcept {
  if ((_activeTransaction != this) || (_asyncState != AsyncState::Reading)) {
    return HAL_ERROR;
  }

  _asyncState = AsyncState::Aborting;
  const HAL_StatusTypeDef status =
      HAL_I2C_Master_Abort_IT(&_handle, _deviceAddress);
  if (status != HAL_OK) {
    // The original read may still invoke its callback if abort cannot start.
    _asyncState = AsyncState::Reading;
  }
  return status;
}

bool I2cDevice::takeReadComplete(uint16_t &value) noexcept {
  if (_asyncState != AsyncState::ReadComplete) {
    return false;
  }

  value = static_cast<uint16_t>(
      (static_cast<uint16_t>(_transactionData[0]) << 8U) | _transactionData[1]);
  _asyncState = AsyncState::Idle;
  return true;
}

bool I2cDevice::takeError() noexcept {
  if (_asyncState != AsyncState::Error) {
    return false;
  }

  _asyncState = AsyncState::Idle;
  return true;
}

bool I2cDevice::isBusy() const noexcept {
  return (_asyncState == AsyncState::Reading) ||
         (_asyncState == AsyncState::Aborting);
}

void I2cDevice::completeTransaction(I2C_HandleTypeDef *handle,
                                    AsyncState completionState) noexcept {
  I2cDevice *const transaction = _activeTransaction;
  if ((transaction == nullptr) || (&transaction->_handle != handle)) {
    return;
  }

  transaction->_asyncState = completionState;
  _activeTransaction = nullptr;
}

} // namespace dda

extern "C" void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *handle) {
  dda::I2cDevice::completeTransaction(handle,
                                      dda::I2cDevice::AsyncState::ReadComplete);
}

extern "C" void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *handle) {
  dda::I2cDevice::completeTransaction(handle,
                                      dda::I2cDevice::AsyncState::Error);
}

extern "C" void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *handle) {
  dda::I2cDevice::completeTransaction(handle,
                                      dda::I2cDevice::AsyncState::Error);
}
