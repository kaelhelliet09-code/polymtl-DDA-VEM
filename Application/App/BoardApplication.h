/**
 * @file BoardApplication.h
 * @brief Board composition root and C lifecycle entry points.
 * @details Owns initialization order, foreground dispatch, fault indication,
 * and the global transition to a safe power state.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Construct, initialize, and safely start the C++ application. */
void DdaApplication_Initialize(void);
/** @brief Advance all foreground safety and application state machines. */
void DdaApplication_Process(void);

#ifdef __cplusplus
}

#include "Drivers/Dac088s085.h"
#include "Drivers/Ina226.h"
#include "Platform/Stm32/Gpio/GpioPin.h"
#include "Service/Launch/LaunchManager.h"
#include "Service/Power/CoilController.h"
#include "Service/RequestManager/RequestManager.h"
#include "Service/Safety/SafetyManager.h"
#include "Service/Sensor/SensorController.h"
#include "Service/UI/StatusLed.h"
#include "Service/UI/UserInput.h"
#include "Service/Usb/USBcontroller.h"

#include <cstdint>

namespace dda {

/** @brief Identifies the first subsystem that failed during startup. */
enum class BoardInitializationFailure : uint8_t {
  None,           ///< No mandatory initialization step failed.
  Timestamp,      ///< The TIM2 timestamp counter could not start.
  ExternalDac,    ///< The shared external DAC could not be made safe.
  CoilController, ///< Bridge or current-reference initialization failed.
  PowerMonitor,   ///< The INA226 could not initialize.
  PowerAlert,     ///< The INA226 power alert could not be configured.
  Acquisition,    ///< Launch timers or ADC acquisition could not initialize.
  SafeState,      ///< The complete startup safe-state command failed.
};

/**
 * @brief Owns and services every user-maintained board subsystem.
 * @note Construct only after CubeMX initializes the referenced peripherals.
 */
class BoardApplication {
public:
  /** @brief Bind application objects to CubeMX handles and generated pins. */
  BoardApplication() noexcept;

  /** @brief Initialize all services. @return First mandatory HAL failure. */
  HAL_StatusTypeDef init() noexcept;

  /**
   * @brief Disable bridges, zero DAC outputs, and cancel active acquisition.
   * @param updateFaultIndicator Refresh the single status LED when true.
   * @return `HAL_OK` when every synchronous safe-state operation succeeded.
   */
  HAL_StatusTypeDef enterSafeState(bool updateFaultIndicator = true) noexcept;

  /** @brief Service fault sampling, isolation, and guarded fault clearing. */
  void processSafety() noexcept;

  /** @brief Advance all foreground services in safety-first order. */
  void process() noexcept;

  /**
   * @brief Set all retained bridge current thresholds through the safety gate.
   * @param currentMilliamps Requested threshold from 0 through 3000 mA.
   * @return `true` when every bridge accepted the request.
   */
  bool setRequestedCoilCurrentMilliamps(uint16_t currentMilliamps) noexcept;

  /** @brief Clear released fault latches. @return Whether clearing succeeded.
   */
  bool clearFault() noexcept;

  /** @brief Report whether power-stage commands are permitted. @return State.
   */
  bool powerStageReadyForCommands() const noexcept;

  /** @brief Report INA226 alert configuration. @return Verified state. */
  bool powerAlertConfigured() const noexcept;

  /** @brief Return the board-wide safety state. @return Current state. */
  SystemState systemState() const noexcept;

  /** @brief Report mandatory initialization. @return Success state. */
  bool isInitialized() const noexcept;

  /** @brief Return the first failure. @return Initialization result. */
  BoardInitializationFailure initializationFailure() const noexcept;

  /** @brief Access the fault-indicator service. @return Owned LED service. */
  StatusLed &statusLed() noexcept;

  /** @brief Access the shared external DAC. @return Owned DAC driver. */
  Dac088s085 &externalDac() noexcept;

  /** @brief Access runtime sensor control. @return Owned sensor controller. */
  SensorController &sensorController() noexcept;

  /** @brief Access power-stage control. @return Owned coil controller. */
  CoilController &coilController() noexcept;

private:
  void enableSafetyInterrupts() noexcept;
  void updateFaultIndicator() noexcept;

  GpioPin _statusLedPin;
  GpioPin _externalDacData;
  GpioPin _externalDacSync;
  GpioPin _externalDacClock;
  GpioPin _powerAlert;
  GpioPin _userButton;
  GpioPin _pmode;
  UserInput _userInput;
  StatusLed _statusLed;
  Dac088s085 _externalDac;
  RequestManager _requestManager;
  SensorController _sensorController;
  SafetyManager _safetyManager;
  CoilController _coilController;
  Ina226 _powerMonitor;
  USBcontroller _usbController;
  LaunchManager _launchManager;
  BoardInitializationFailure _initializationFailure;
  bool _initialized;
};

/** @brief Return the application composition root. @return Sole instance. */
BoardApplication &boardApplication() noexcept;

} // namespace dda

#endif /* __cplusplus */
