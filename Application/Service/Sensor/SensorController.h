/**
 * @file SensorController.h
 * @brief Declares four sensor interrupt actions and analog-level controls.
 * @details Maps logical sensors to external-DAC channels, publishes debounced
 * interrupt triggers, and delegates persistence to CalibrationStore.
 */

#pragma once

#include "Config/SensorConfig.h"
#include "Drivers/Dac088s085.h"
#include "Drivers/Sensors/Sensor.h"
#include "Drivers/Sensors/VelocitySensor.h"
#include "Service/Calibration/CalibrationStore.h"
#include "Service/RequestManager/RequestManager.h"
#include "stm32g0xx_hal_def.h"
#include <cstdint>

namespace dda {

class CoilController;

/**
 * @brief Controls sensor DAC outputs and dispatches four independent actions.
 *
 * DAC values are raw 8-bit codes because the LED-current and comparator-voltage
 * transfer functions are properties of the external analog circuit.
 */
class SensorController {
public:
  /**
   * @brief Binds the controller to the board's external eight-channel DAC.
   * @param dac DAC retained by reference; it must outlive this object.
   * @param calibrationStore Persistent store retained by reference; it must
   * outlive this object.
   */
  SensorController(Dac088s085 &dac, CalibrationStore &calibrationStore,
                   RequestManager &requestManager) noexcept;

  /**
   * @brief Loads and validates all persistent sensor calibration points.
   * @return Storage load status; `StorageEmpty` is normal on an unused board.
   * @note Invalid records are ignored and no point is applied automatically.
   */
  CalibrationStoreResult initializeCalibrationStore() noexcept;

  /**
   * @brief Sets one sensor's IR-LED current command.
   * @param sensor Sensor whose LED current is controlled.
   * @param code Raw DAC code in the range 0 to 255.
   * @return HAL SPI status.
   * @warning Uses the normal foreground SPI timeout.
   */
  HAL_StatusTypeDef setLedCurrentCode(Sensor &sensor, uint8_t code) noexcept;

  /**
   * @brief Sets one sensor comparator's trip-voltage command.
   * @param sensor Sensor whose comparator threshold is controlled.
   * @param code Raw DAC code in the range 0 to 255.
   * @return HAL SPI status.
   * @warning Uses blocking SPI and must not be called from interrupt context.
   */
  HAL_StatusTypeDef setTripVoltageCode(Sensor &sensor, uint8_t code) noexcept;

  /**
   * @brief Writes zero to every sensor DAC output.
   * @return HAL SPI status from the broadcast write.
   * @warning Uses blocking SPI and must not be called from interrupt context.
   */
  HAL_StatusTypeDef clearSensorDacOutputs() noexcept;

  /**
   * @brief Sweeps one sensor's IR-LED DAC until its input becomes high.
   * @param sensor Sensor to calibrate.
   * @return `true` when the sensor triggered and its two DAC codes were saved.
   * @note This blocking foreground routine uses HAL_Delay between DAC steps.
   */
  bool calibrateSensor(Sensor &sensor) noexcept;

  /** Run host-requested calibration without local button interaction. */
  bool runAutomaticCalibrationBlocking(CoilController &coils) noexcept;

  /** Consume a pending host calibration request. */
  bool takeHostCalibrationRequest() noexcept;

  /** Publish the result so the pending USB request can be answered. */
  void completeHostCalibration(bool succeeded) noexcept;

  /**
   * @brief Returns one fixed sensor.
   * @param index Zero-based index below SensorCount.
   * @return Mutable reference to the selected fixed sensor.
   */
  Sensor &sensor(uint8_t index) noexcept;

  /**
   * @brief Dispatches one EXTI event to its dedicated sensor action.
   * @param sensor Sensor associated with the asserted EXTI line.
   * @note Runs in interrupt context; each action must remain non-blocking.
   */
  void handleSensorInterrupt(Sensor &sensor, SensorEdge edge) noexcept;

  void beginLaunchCapture() noexcept;
  void endLaunchCapture() noexcept;
  void copyLaunchEvents(SensorEvents *events, uint8_t capacity) const noexcept;

  /**
   * @brief Reads and clears one sensor's pending trigger latch atomically.
   * @param sensor Sensor whose event latch is consumed.
   * @return `true` once after at least one corresponding interrupt.
   */
  bool takeTrigger(Sensor &sensor) noexcept;

  /**
   * @brief Atomically reads and clears every pending sensor trigger.
   * @return Bit zero through three represent Sensor1 through Sensor4.
   */
  uint8_t takePendingTriggers() noexcept;

  CalibrationStoreResult
  readCalibration(const Sensor &sensor,
                  SensorCalibrationData &data) const noexcept;

  /**
   * @brief Saves successful calibration values without disturbing other data.
   * @param sensor Sensor that owns the calibration.
   * @param currentLedCode IR-LED current DAC code.
   * @param voltageTripCode Comparator-trip DAC code.
   * @return Verified Flash transaction status.
   */
  CalibrationStoreResult
  saveCalibration(const Sensor &sensor, CalibrationDacCode currentLedCode,
                  CalibrationDacCode voltageTripCode) noexcept;

  /**
   * @brief Returns the persistent-storage initialization status.
   * @return Latest status returned by initializeCalibrationStore().
   */
  CalibrationStoreResult calibrationStoreStatus() const noexcept;

  /**
   * @brief Returns raw HAL Flash error flags from the latest update.
   * @return Bit mask returned by HAL_FLASH_GetError().
   */
  uint32_t calibrationFlashErrorFlags() const noexcept;
  /** @brief Single velocity-measurement latch shared with the capture ISR. */

  HAL_StatusTypeDef setAllDefault() noexcept;
  HAL_StatusTypeDef setAllCalibrated() noexcept;
  HAL_StatusTypeDef setToCalibrated(Sensor &sensor) noexcept;
  HAL_StatusTypeDef setToDefault(Sensor &sensor) noexcept;

  VelocitySensor _velocitySensor;

  void processRequest(Request &request) noexcept;

private:
  /** @brief Fixed GPIO and DAC-channel mapping for Sensor1 through Sensor4.
   */
  Sensor _sensors[config::SensorCount];
  Dac088s085 &_dac; ///< Non-owning sensor-output DAC reference.
  CalibrationStore &_calibrationStore; ///< Non-owning persistent store.
  RequestManager &_requestManager;
  volatile uint8_t
      _pendingTriggers; ///< One latched foreground event per sensor.
  volatile bool _hostCalibrationRequested;
  bool _hostCalibrationActive;
  bool _hostCalibrationResultReady;
  bool _hostCalibrationSucceeded;
  volatile bool _launchCaptureActive;
};

} // namespace dda
