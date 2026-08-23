#include "Drivers/Sensors/VelocitySensor.h"

#include "Config/BoardConfig.h"
#include "Platform/Stm32/System/InterruptGuard.h"
#include "stm32g0xx_hal.h"

namespace dda {

bool VelocitySensor::getEvent() const noexcept { return _speedCaptureEvent; }

void VelocitySensor::clearEvent() noexcept {
  InterruptGuard interruptGuard;
  _speedCaptureEvent = false;
}

void VelocitySensor::reset() noexcept {
  InterruptGuard interruptGuard;
  _tickDelta = 0U;
  _speedCaptureEvent = false;
}

uint32_t VelocitySensor::getTickDelta() const noexcept {
  InterruptGuard interruptGuard;
  return _tickDelta;
}

double VelocitySensor::getSpeed() noexcept {
  uint32_t tickDelta = 0U;
  {
    InterruptGuard interruptGuard;
    tickDelta = _tickDelta;
    _speedCaptureEvent = false;
  }

  if (tickDelta == 0U) {
    return 0.0;
  }

  const double spacingMeters = config::VelocitySensorSpacingMillimeters / 1000.0;
  const double elapsedSeconds =
      static_cast<double>(tickDelta) / config::Tim2FrequencyHz;
  return spacingMeters / elapsedSeconds;
}

void VelocitySensor::markSpeedCaptureEvent(uint32_t tickDelta) noexcept {
  if (tickDelta == 0U) {
    return;
  }
  _tickDelta = tickDelta;
  _speedCaptureEvent = true;
}

} // namespace dda
