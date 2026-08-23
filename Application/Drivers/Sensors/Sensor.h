/**
 * @file Sensor.h
 * @brief Stores the fixed hardware resources belonging to one sensor.
 */

#pragma once

#include "Config/SensorConfig.h"
#include "Drivers/Dac088s085.h"
#include "Drivers/Sensors/SensorId.h"
#include "Platform/Stm32/Gpio/GpioPin.h"
#include <cstdint>

namespace dda {
class SensorController;

enum class SensorEdge : uint8_t { Rising = 0U, Falling = 1U };

struct SensorEvents {
  static constexpr uint8_t Capacity = 10U;

  uint32_t risingEdgeTimestamps[Capacity]{};
  uint32_t fallingEdgeTimestamps[Capacity]{};
  uint8_t risingEdgeCount{0U};
  uint8_t fallingEdgeCount{0U};
};

/** @brief Owns the fixed GPIO and external-DAC channels for one sensor. */
class Sensor {
public:
  /**
   * @brief Binds one sensor to its trigger input and analog-output channels.
   * @param sensorId Zero-based sensor identity.
   * @param inputPort GPIO port containing the trigger input.
   * @param inputPin GPIO pin mask for the trigger input.
   * @param ledCurrentChannel External-DAC channel controlling LED current.
   * @param tripVoltageChannel External-DAC channel controlling trip voltage.
   */
  Sensor(SensorId sensorId, GPIO_TypeDef *inputPort, uint16_t inputPin,
         DacChannel ledCurrentChannel, DacChannel tripVoltageChannel) noexcept
      : _inputPin(inputPort, inputPin, GpioDirection::INPUT),
        _ledCurrentChannel(ledCurrentChannel),
        _tripVoltageChannel(tripVoltageChannel), _sensorId(sensorId) {}

  /**
   * @brief Returns the sensor's trigger-input wrapper.
   * @return Non-owning reference valid for this sensor's lifetime.
   */
  const GpioPin &inputPin() const noexcept { return _inputPin; }

  /**
   * @brief Writes the raw external-DAC code controlling LED current.
   * @param dac External DAC that owns the configured channel.
   * @param code Raw eight-bit DAC code.
   * @param timeoutMilliseconds Blocking SPI timeout.
   * @return HAL status from the DAC write.
   */
  HAL_StatusTypeDef
  setCurrentLedCode(Dac088s085 &dac, uint8_t code,
                    uint32_t timeoutMilliseconds =
                        config::SensorDacTimeoutMilliseconds) noexcept {
    return dac.write(_ledCurrentChannel, code, timeoutMilliseconds);
  }

  /**
   * @brief Writes the raw external-DAC code controlling trip voltage.
   * @param dac External DAC that owns the configured channel.
   * @param code Raw eight-bit DAC code.
   * @param timeoutMilliseconds Blocking SPI timeout.
   * @return HAL status from the DAC write.
   */
  HAL_StatusTypeDef
  setVoltageTripCode(Dac088s085 &dac, uint8_t code,
                     uint32_t timeoutMilliseconds =
                         config::SensorDacTimeoutMilliseconds) noexcept {
    return dac.write(_tripVoltageChannel, code, timeoutMilliseconds);
  }

  /**
   * @brief Reports whether a trigger has been latched for this sensor.
   * @return True after a trigger until clearTriggered() is called.
   */
  bool wasTriggered() const noexcept { return _triggered; }

  /** @brief Clears the sensor's latched trigger state. */
  void clearTriggered() noexcept { _triggered = false; }

  const SensorEvents &events() const noexcept { return _events; }

private:
  friend class SensorController;

  bool refreshTriggered() noexcept {
    if (_triggered) {
      return true;
    }
    bool inputState = false;
    if (!_inputPin.read(inputState)) {
      return false;
    }
    if (inputState == (config::SensorTriggeredState == GPIO_PIN_SET)) {
      _triggered = true;
    }
    return _triggered;
  }

  void clearEvents() noexcept {
    _events = {};
    _risingEdgeCount = 0U;
    _fallingEdgeCount = 0U;
  }

  void recordEdge(SensorEdge edge, uint32_t timestamp) noexcept {
    volatile uint8_t &count =
        edge == SensorEdge::Rising ? _risingEdgeCount : _fallingEdgeCount;
    if (count >= SensorEvents::Capacity) {
      return;
    }
    uint32_t *const timestamps = edge == SensorEdge::Rising
                                     ? _events.risingEdgeTimestamps
                                     : _events.fallingEdgeTimestamps;
    timestamps[count] = timestamp;
    ++count;
    _events.risingEdgeCount = _risingEdgeCount;
    _events.fallingEdgeCount = _fallingEdgeCount;
  }

  GpioPin _inputPin;
  DacChannel _ledCurrentChannel;
  DacChannel _tripVoltageChannel;
  volatile bool _triggered = false;
  SensorEvents _events{};
  volatile uint8_t _risingEdgeCount{0U};
  volatile uint8_t _fallingEdgeCount{0U};
  SensorId _sensorId;
};

} // namespace dda
