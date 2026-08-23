// Implements validated access to one STM32 internal DAC channel used for a
// DRV8874 VREF output.
#include "Platform/Stm32/Analog/InternalDacChannel.h"

namespace dda {

InternalDacChannel::InternalDacChannel(DAC_HandleTypeDef &handle,
                                       uint32_t channel) noexcept
    : _handle(handle), _channel(channel) {}

HAL_StatusTypeDef InternalDacChannel::start() noexcept {
  return HAL_DAC_Start(&_handle, _channel);
}

HAL_StatusTypeDef InternalDacChannel::stop() noexcept {
  return HAL_DAC_Stop(&_handle, _channel);
}

HAL_StatusTypeDef InternalDacChannel::write(uint16_t value) noexcept {
  // Saturate before HAL access so no upper bits leak into the 12-bit register.
  return HAL_DAC_SetValue(&_handle, _channel, DAC_ALIGN_12B_R,
                          value > MaximumValue ? MaximumValue : value);
}

} // namespace dda
