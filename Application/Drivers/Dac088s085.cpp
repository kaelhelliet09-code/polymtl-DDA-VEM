// Implements validated DAC088S085 command framing and bounded SPI transfers
// with explicit chip-select ownership.
#include "Drivers/Dac088s085.h"

namespace {
constexpr uint16_t WriteThroughMode = 0x9000U;
constexpr uint16_t BroadcastCommand = 0xC000U;
} // namespace

namespace dda {

Dac088s085::Dac088s085(SPI_HandleTypeDef &spi, GpioPin &chipSelect) noexcept
    : _spi(spi), _chipSelect(chipSelect) {}

HAL_StatusTypeDef Dac088s085::init(uint32_t timeoutMs) noexcept {
  if (!_chipSelect.set()) {
    return HAL_ERROR;
  }
  return transmit(WriteThroughMode, timeoutMs);
}

HAL_StatusTypeDef Dac088s085::write(DacChannel channel, uint8_t value,
                                    uint32_t timeoutMs) noexcept {
  const uint8_t numericChannel = static_cast<uint8_t>(channel);
  return numericChannel < ChannelCount
             ? writeDac(numericChannel, value, timeoutMs)
             : HAL_ERROR;
}

HAL_StatusTypeDef Dac088s085::write(uint8_t channel, uint8_t value,
                                    uint32_t timeoutMs) noexcept {
  if (channel >= ChannelCount) {
    return HAL_ERROR;
  }
  return writeDac(channel, value, timeoutMs);
}

HAL_StatusTypeDef Dac088s085::writeAll(uint8_t value,
                                       uint32_t timeoutMs) noexcept {
  return transmit(static_cast<uint16_t>(BroadcastCommand |
                                        (static_cast<uint16_t>(value) << 4U)),
                  timeoutMs);
}

HAL_StatusTypeDef Dac088s085::abort() noexcept {
  const HAL_StatusTypeDef status =
      (HAL_SPI_GetState(&_spi) == HAL_SPI_STATE_READY) ? HAL_OK
                                                       : HAL_SPI_Abort(&_spi);
  // Chip select must be high even if HAL_SPI_Abort reports an error.
  return _chipSelect.set() ? status : HAL_ERROR;
}

HAL_StatusTypeDef Dac088s085::writeDac(uint8_t channel, uint8_t value,
                                       uint32_t timeoutMs) noexcept {
  return transmit(static_cast<uint16_t>((static_cast<uint16_t>(channel) << 12U) |
                                        (static_cast<uint16_t>(value) << 4U)),
                  timeoutMs);
}

HAL_StatusTypeDef Dac088s085::transmit(uint16_t frame,
                                       uint32_t timeoutMs) noexcept {
  if ((timeoutMs == 0U) || !_chipSelect.reset()) {
    return HAL_ERROR;
  }

  const HAL_StatusTypeDef status = HAL_SPI_Transmit(
      &_spi, reinterpret_cast<uint8_t *>(&frame), 1U, timeoutMs);
  // A rising edge latches the 16-bit DAC command.
  if (!_chipSelect.set()) {
    return HAL_ERROR;
  }
  return status;
}

} // namespace dda
