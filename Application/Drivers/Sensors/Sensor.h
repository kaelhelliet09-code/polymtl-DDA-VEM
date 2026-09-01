/**
 * @file Sensor.h
 * @brief Fixed GPIO and VTRIP resources belonging to one optical sensor.
 */

#pragma once

#include "Config/SensorConfig.h"
#include "Drivers/Dac088s085.h"
#include "Drivers/Sensors/SensorEdgeDebounce.h"
#include "Drivers/Sensors/SensorId.h"
#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

namespace dda {
class SensorController;

/** @brief Bounded rising/falling timestamp history for one sensor. */
struct SensorEvents {
  /** @brief Maximum stored timestamps of each polarity. */
  static constexpr uint8_t Capacity = 10U;

  /** @brief Accepted rising-edge TIM2 timestamps. */
  uint32_t risingEdgeTimestamps[Capacity]{};
  /** @brief Accepted falling-edge TIM2 timestamps. */
  uint32_t fallingEdgeTimestamps[Capacity]{};
  /** @brief Number of valid entries in risingEdgeTimestamps. */
  uint8_t risingEdgeCount{0U};
  /** @brief Number of valid entries in fallingEdgeTimestamps. */
  uint8_t fallingEdgeCount{0U};
};

/** @brief Owns one trigger input, IR-LED enable output, and VTRIP channel. */
class Sensor {
public:
  /**
   * @brief Bind one sensor to its fixed input, LED GPIO, and VTRIP channel.
   * @param sensorId Logical sensor identity.
   * @param inputPort Trigger input GPIO bank.
   * @param inputPin Trigger input pin mask.
   * @param irLedEnablePort IR-LED enable GPIO bank.
   * @param irLedEnablePin IR-LED enable pin mask.
   * @param tripVoltageChannel External-DAC channel wired to VTRIP.
   */
  Sensor(SensorId sensorId, GPIO_TypeDef *inputPort, uint16_t inputPin,
         GPIO_TypeDef *irLedEnablePort, uint16_t irLedEnablePin,
         DacChannel tripVoltageChannel) noexcept
      : _inputPin(inputPort, inputPin, GpioDirection::INPUT),
        _irLedEnable(irLedEnablePort, irLedEnablePin, GpioDirection::OUTPUT),
        _tripVoltageChannel(tripVoltageChannel),
        _debounce(config::SensorCaptureDebounceTicks), _sensorId(sensorId) {}

  /** @brief Access the trigger input. @return Bound input GPIO. */
  const GpioPin &inputPin() const noexcept { return _inputPin; }

  /**
   * @brief Drive only this sensor's normal GPIO IR-LED enable.
   * @param enabled Desired output state.
   * @return Whether the GPIO write succeeded.
   */
  bool setIrLedEnabled(bool enabled) noexcept {
    return _irLedEnable.write(enabled);
  }

  /**
   * @brief Write this sensor's comparator trip voltage through the DAC.
   * @param dac Shared external DAC.
   * @param code Raw eight-bit VTRIP code.
   * @param timeoutMilliseconds Foreground SPI timeout.
   * @return STM32 HAL SPI status.
   */
  HAL_StatusTypeDef
  setVoltageTripCode(Dac088s085 &dac, uint8_t code,
                     uint32_t timeoutMilliseconds =
                         config::SensorDacTimeoutMilliseconds) noexcept {
    return dac.write(_tripVoltageChannel, code, timeoutMilliseconds);
  }

  /** @brief Return the rising-edge latch. @return Whether it was triggered. */
  bool wasTriggered() const noexcept { return _triggered; }
  /** @brief Clear the rising-edge latch. */
  void clearTriggered() noexcept { _triggered = false; }
  /** @brief Access captured events. @return This sensor's event history. */
  const SensorEvents &events() const noexcept { return _events; }

private:
  friend class SensorController;

  bool acceptEdge(SensorEdge edge, uint32_t edgeTimestamp) noexcept {
    return _debounce.accept(edge, edgeTimestamp);
  }

  void resetDebounce() noexcept { _debounce.reset(); }

  void clearEvents() noexcept {
    _events = {};
    _risingEdgeCount = 0U;
    _fallingEdgeCount = 0U;
  }

  void recordEdge(SensorEdge edge, uint32_t edgeTimestamp) noexcept {
    volatile uint8_t &count =
        edge == SensorEdge::Rising ? _risingEdgeCount : _fallingEdgeCount;
    if (count >= SensorEvents::Capacity) {
      return;
    }
    uint32_t *const timestamps = edge == SensorEdge::Rising
                                     ? _events.risingEdgeTimestamps
                                     : _events.fallingEdgeTimestamps;
    timestamps[count] = edgeTimestamp;
    ++count;
    _events.risingEdgeCount = _risingEdgeCount;
    _events.fallingEdgeCount = _fallingEdgeCount;
  }

  GpioPin _inputPin;
  GpioPin _irLedEnable;
  DacChannel _tripVoltageChannel;
  SensorEdgeDebounce _debounce;
  volatile bool _triggered{false};
  SensorEvents _events{};
  volatile uint8_t _risingEdgeCount{0U};
  volatile uint8_t _fallingEdgeCount{0U};
  SensorId _sensorId;
};

} // namespace dda
