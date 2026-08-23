#pragma once

#include <cstdint>

namespace dda {

/**
 * @brief Converts two ordered TIM2 sensor captures into projectile velocity.
 * @details The ISR publishes only an elapsed tick count; floating-point unit
 * conversion is deferred until foreground code consumes the measurement.
 */
class VelocitySensor {
public:
  /** @brief Constructs an empty velocity measurement latch. */
  VelocitySensor() noexcept = default;

  /**
   * @brief Reports whether a new complete capture interval is available.
   * @return `true` until clearEvent() or getSpeed() consumes the event.
   */
  bool getEvent() const noexcept;

  /** @brief Clears the pending event without changing the captured interval. */
  void clearEvent() noexcept;

  /** @brief Clears both the captured interval and its pending event. */
  void reset() noexcept;

  /** @return Latest raw TIM2 capture interval, or zero before a capture. */
  uint32_t getTickDelta() const noexcept;

  /**
   * @brief Atomically consumes the latest interval and calculates velocity.
   * @return Velocity in metres per second, or zero before a valid capture.
   */
  double getSpeed() noexcept;

  /**
   * @brief Publishes a completed capture interval from interrupt context.
   * @param tickDelta Nonzero elapsed TIM2 ticks between sensor 1 and sensor 2.
   */
  void markSpeedCaptureEvent(uint32_t tickDelta) noexcept;

private:
  volatile uint32_t _tickDelta = 0U;
  volatile bool _speedCaptureEvent = false;
};

} // namespace dda
