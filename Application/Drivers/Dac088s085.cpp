// Implements validated DAC088S085 command framing over three GPIOs.
#include "Drivers/Dac088s085.h"

namespace {
constexpr uint16_t WriteThroughMode = 0x9000U;
constexpr uint16_t BroadcastCommand = 0xC000U;
} // namespace

namespace dda {

Dac088s085::Dac088s085(GpioPin &data, GpioPin &sync, GpioPin &clock) noexcept
    : _data(data), _sync(sync), _clock(clock) {}

HAL_StatusTypeDef Dac088s085::init(uint32_t timeoutMs) noexcept {
  if (!_sync.set() || !_clock.reset() || !_data.reset()) {
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
  return (_sync.set() && _clock.reset() && _data.reset()) ? HAL_OK
                                                          : HAL_ERROR;
}

HAL_StatusTypeDef Dac088s085::writeDac(uint8_t channel, uint8_t value,
                                       uint32_t timeoutMs) noexcept {
  return transmit(static_cast<uint16_t>((static_cast<uint16_t>(channel) << 12U) |
                                        (static_cast<uint16_t>(value) << 4U)),
                  timeoutMs);
}

HAL_StatusTypeDef Dac088s085::transmit(uint16_t frame,
                                       uint32_t timeoutMs) noexcept {
  if ((timeoutMs == 0U) || !_sync.reset()) {
    return HAL_ERROR;
  }

  for (uint32_t mask = 0x8000U; mask != 0U; mask >>= 1U) {
    if (!_data.write((frame & mask) != 0U) || !_clock.set() ||
        !_clock.reset()) {
      (void)_sync.set();
      return HAL_ERROR;
    }
  }
  return _sync.set() ? HAL_OK : HAL_ERROR;
}

} // namespace dda
