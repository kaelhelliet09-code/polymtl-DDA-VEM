/**
 * @file Timestamp.h
 * @brief Declares a wrap-safe timestamp source backed by an STM32 timer.
 */

#pragma once

#include "Config/BoardConfig.h"

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"
}

namespace dda {

/**
 * @brief Provides timestamps from the free-running 64 MHz TIM2 counter.
 *
 * @note The supplied timer must be configured as a 64 MHz up-counter with a
 * 32-bit auto-reload value.
 */
class Timestamp {
public:
  /** @brief Unsigned raw timer tick count. */
  using Tick = uint32_t;
  static constexpr uint32_t FrequencyHz = config::Tim2FrequencyHz;
  static constexpr uint32_t TicksPerMicrosecond =
      config::Tim2TicksPerMicrosecond;

  /**
   * @brief Constructs a timestamp source over an existing HAL timer handle.
   * @param timer Timer handle retained by reference; it must outlive this
   * object.
   */
  explicit Timestamp(TIM_HandleTypeDef &timer) noexcept;

  /**
   * @brief Starts the base timer, resetting it before each start attempt.
   * @return `HAL_OK` if already running or if the HAL start succeeds.
   * @note Once running, later calls leave the counter unchanged. A failed start
   * may be retried.
   */
  HAL_StatusTypeDef start() noexcept;

  /**
   * @brief Reads the current free-running timer counter.
   * @return Raw 64 MHz TIM2 counter value.
   * @warning The value is meaningful only after the timer has been initialized
   * by CubeMX and start() has succeeded.
   */
  Tick now() const noexcept;

  /**
   * @brief Computes a wrap-safe unsigned duration.
   * @param start Earlier counter value.
   * @param end Later counter value.
   * @return Elapsed timer ticks modulo 2^32.
   * @note The duration is unambiguous only for intervals shorter than one full
   * counter period.
   */
  static constexpr Tick elapsed(Tick start, Tick end) noexcept {
    return end - start;
  }

  /**
   * @brief Converts a raw TIM2 timestamp to the USB millisecond word.
   * @param timestampTicks Raw TIM2 timestamp.
   * @return Whole milliseconds truncated to the low 16 bits.
   * @note Sub-millisecond precision is discarded and the result wraps every
   * 65,536 ms.
   */
  static constexpr uint16_t
  toUsbTimestampMilliseconds(Tick timestampTicks) noexcept {
    return static_cast<uint16_t>(timestampTicks / (FrequencyHz / 1000U));
  }

private:
  TIM_HandleTypeDef &_timer; ///< Non-owning reference to the HAL timer handle.
  bool _started;             ///< Prevents resetting an already running timer.
};

/**
 * @brief Global timestamp source backed by the CubeMX TIM2 handle.
 */
extern Timestamp timestamp;

} // namespace dda
