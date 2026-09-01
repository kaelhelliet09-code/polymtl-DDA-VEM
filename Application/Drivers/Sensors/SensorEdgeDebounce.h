/** @file SensorEdgeDebounce.h @brief Non-blocking sensor edge filter. */
#pragma once

#include <cstdint>

namespace dda {

/** @brief GPIO edge polarity reported by a sensor EXTI line. */
enum class SensorEdge : uint8_t {
  Rising = 0U, ///< Low-to-high transition.
  Falling = 1U ///< High-to-low transition.
};

/**
 * @brief Reject repeated edges of the same polarity inside a tick interval.
 *
 * Rising and falling histories are deliberately independent. This filters
 * contact/comparator chatter without rejecting a valid short pulse merely
 * because its opposite edge follows quickly. Unsigned subtraction makes the
 * elapsed-time check safe across a 32-bit timer wrap.
 */
class SensorEdgeDebounce {
public:
  /**
   * @brief Construct a filter.
   * @param intervalTicks Minimum same-polarity interval in timer ticks.
   */
  explicit constexpr SensorEdgeDebounce(uint32_t intervalTicks) noexcept
      : _intervalTicks(intervalTicks) {}

  /**
   * @brief Test and record one edge without blocking.
   * @param edge Edge polarity.
   * @param timestamp Free-running unsigned timer timestamp.
   * @return `true` when the edge is outside its polarity's debounce interval.
   */
  bool accept(SensorEdge edge, uint32_t timestamp) noexcept {
    const uint8_t index = static_cast<uint8_t>(edge);
    if (_timestampValid[index] &&
        ((timestamp - _lastTimestamp[index]) < _intervalTicks)) {
      return false;
    }
    _lastTimestamp[index] = timestamp;
    _timestampValid[index] = true;
    return true;
  }

  /** @brief Forget both polarity histories. */
  void reset() noexcept {
    _lastTimestamp[0] = 0U;
    _lastTimestamp[1] = 0U;
    _timestampValid[0] = false;
    _timestampValid[1] = false;
  }

private:
  const uint32_t _intervalTicks;
  uint32_t _lastTimestamp[2]{0U, 0U};
  bool _timestampValid[2]{false, false};
};

} // namespace dda
