/**
 * @file SensorController.h
 * @brief Four independent sensor GPIO actions, VTRIP control, and capture.
 */

#pragma once

#include "Config/SensorConfig.h"
#include "Drivers/Dac088s085.h"
#include "Drivers/Sensors/Sensor.h"
#include "Drivers/Sensors/VelocitySensor.h"
#include "Service/RequestManager/RequestManager.h"

#include <cstdint>

namespace dda {

/** @brief Coordinates four sensor GPIOs, VTRIP outputs, and EXTI capture. */
class SensorController {
public:
  /**
   * @brief Bind sensors to the shared DAC and request manager.
   * @param dac Shared board DAC; must outlive this controller.
   * @param requestManager Request router; must outlive this controller.
   */
  SensorController(Dac088s085 &dac, RequestManager &requestManager) noexcept;

  /**
   * @brief Set one comparator threshold.
   * @param sensor Sensor to update.
   * @param code Raw eight-bit VTRIP code.
   * @return STM32 HAL SPI status.
   */
  HAL_StatusTypeDef setTripVoltageCode(Sensor &sensor, uint8_t code) noexcept;

  /**
   * @brief Control one sensor's IR LED through its GPIO.
   * @param sensor Sensor to update.
   * @param enabled Desired LED-enable state.
   * @return Whether the GPIO write succeeded.
   */
  bool setIrLedEnabled(Sensor &sensor, bool enabled) noexcept;

  /**
   * @brief Disable every sensor LED and zero only the four VTRIP channels.
   * @return First GPIO/DAC failure, or `HAL_OK`.
   */
  HAL_StatusTypeDef clearSensorDacOutputs() noexcept;

  /** @brief Apply defaults to all sensors. @return First HAL/GPIO failure. */
  HAL_StatusTypeDef setAllDefault() noexcept;

  /**
   * @brief Apply the configured VTRIP and enable one LED.
   * @param sensor Sensor to configure.
   * @return STM32 HAL/GPIO status.
   */
  HAL_StatusTypeDef setToDefault(Sensor &sensor) noexcept;

  /** @brief Access a fixed sensor. @param index Index 0..3. @return Sensor. */
  Sensor &sensor(uint8_t index) noexcept;

  /**
   * @brief Debounce, capture, and publish one sensor edge from EXTI context.
   * @param sensor Source sensor.
   * @param edge Captured polarity.
   */
  void handleSensorInterrupt(Sensor &sensor, SensorEdge edge) noexcept;

  /** @brief Reset event/debounce history and begin launch capture. */
  void beginLaunchCapture() noexcept;
  /** @brief Stop adding edges to launch history. */
  void endLaunchCapture() noexcept;

  /**
   * @brief Copy all four event histories under an interrupt guard.
   * @param events Destination array.
   * @param capacity Destination sensor capacity; must be at least four.
   */
  void copyLaunchEvents(SensorEvents *events, uint8_t capacity) const noexcept;

  /**
   * @brief Atomically consume one rising-edge trigger latch.
   * @param sensor Sensor whose latch is consumed.
   * @return Whether the latch was set.
   */
  bool takeTrigger(Sensor &sensor) noexcept;

  /** @brief Consume all trigger bits. @return Bit mask for sensors S1..S4. */
  uint8_t takePendingTriggers() noexcept;

  /** @brief Velocity measurement assembled by TIM2 capture callbacks. */
  VelocitySensor _velocitySensor;

  /** @brief Process one sensor request. @param request Routed request. */
  void processRequest(Request &request) noexcept;

private:
  Sensor _sensors[config::SensorCount];
  Dac088s085 &_dac;
  RequestManager &_requestManager;
  volatile uint8_t _pendingTriggers;
  volatile bool _launchCaptureActive;
};

} // namespace dda
