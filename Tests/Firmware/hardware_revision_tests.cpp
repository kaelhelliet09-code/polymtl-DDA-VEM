#include "Config/ExternalDacConfig.h"
#include "Config/PowerConfig.h"
#include "Drivers/Sensors/SensorEdgeDebounce.h"
#include "Service/Launch/CurrentSampleOrder.h"
#include "Service/Power/CoilRequest.h"

#include <cstddef>
#include <cstdint>

namespace {

static_assert(dda::config::DriverCurrentLimitDacChannels[0] ==
              dda::DAC_CHANNEL_C);
static_assert(dda::config::DriverCurrentLimitDacChannels[1] ==
              dda::DAC_CHANNEL_D);
static_assert(dda::config::DriverCurrentLimitDacChannels[2] ==
              dda::DAC_CHANNEL_H);
static_assert(dda::config::DriverCurrentLimitDacChannels[3] ==
              dda::DAC_CHANNEL_G);
static_assert(dda::config::SensorTripVoltageDacChannels[0] ==
              dda::DAC_CHANNEL_A);
static_assert(dda::config::SensorTripVoltageDacChannels[1] ==
              dda::DAC_CHANNEL_B);
static_assert(dda::config::SensorTripVoltageDacChannels[2] ==
              dda::DAC_CHANNEL_F);
static_assert(dda::config::SensorTripVoltageDacChannels[3] ==
              dda::DAC_CHANNEL_E);

static_assert(dda::config::currentMilliampsToReferenceCode(0U) == 0U);
static_assert(dda::config::currentMilliampsToReferenceCode(1000U) == 63U);
static_assert(dda::config::currentMilliampsToReferenceCode(3000U) == 190U);

static_assert(static_cast<uint8_t>(dda::CoilCommand::SetCurrentH1) == 7U);
static_assert(static_cast<uint8_t>(dda::CoilCommand::GetCurrentH4) == 14U);
static_assert(static_cast<uint8_t>(dda::CoilCommand::SetPmode) == 15U);
static_assert(static_cast<uint8_t>(dda::CoilCommand::GetPmode) == 16U);

} // namespace

int main() {
  uint8_t samples[] = {1U, 2U, 4U, 3U, 11U, 12U, 14U, 13U};
  dda::normalizeBridgeCurrentSamples(samples, 2U);
  constexpr uint8_t expected[] = {1U, 2U, 3U, 4U,
                                  11U, 12U, 13U, 14U};
  for (std::size_t index = 0U; index < sizeof(samples); ++index) {
    if (samples[index] != expected[index]) {
      return 1;
    }
  }

  dda::SensorEdgeDebounce debounce(100U);
  if (!debounce.accept(dda::SensorEdge::Rising, 1000U) ||
      debounce.accept(dda::SensorEdge::Rising, 1099U) ||
      !debounce.accept(dda::SensorEdge::Falling, 1001U) ||
      !debounce.accept(dda::SensorEdge::Rising, 1100U)) {
    return 2;
  }

  debounce.reset();
  if (!debounce.accept(dda::SensorEdge::Rising, 0xFFFFFFF0U) ||
      debounce.accept(dda::SensorEdge::Rising, 0x00000010U) ||
      !debounce.accept(dda::SensorEdge::Rising, 0x00000060U)) {
    return 3;
  }
  return 0;
}
