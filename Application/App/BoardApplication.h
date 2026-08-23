/**
 * @file BoardApplication.h
 * @brief Declares the board composition root and C lifecycle entry points.
 * @details Owns initialization order, foreground service dispatch, guarded
 * fault acknowledgement, and the global transition to a safe power state.
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
#include "Service/Calibration/CalibrationStore.h"
#include "Service/UI/Display.h"
#include "Service/UI/StatusLed.h"
#include "Service/UI/UserInput.h"
#include "Platform/Stm32/Gpio/GpioPin.h"
#include "Service/Launch/LaunchManager.h"
#include "Service/Power/CoilController.h"
#include "Service/RequestManager/RequestManager.h"
#include "Service/Safety/SafetyManager.h"
#include "Service/Sensor/SensorController.h"
#include "Service/Usb/USBcontroller.h"
#include <cstdint>


namespace dda {

/** @brief Identifies the first subsystem that failed during startup. */
enum class BoardInitializationFailure : uint8_t {
  None,           ///< No mandatory initialization step failed.
  Timestamp,      ///< The TIM2 timestamp counter could not be started.
  CoilController, ///< Bridge or current-reference initialization failed.
  ExternalDac,    ///< The external sensor DAC could not be made safe.
  PowerMonitor,   ///< The INA226 could not be initialized.
  PowerAlert,     ///< The INA226 power alert could not be configured.
  Acquisition,    ///< Launch timers or ADC acquisition could not initialize.
  SafeState,      ///< The complete startup safe-state command failed.
};

/**
 * @brief Owns the user-maintained board drivers, services, storage, and UI.
 *
 * Construct this only after CubeMX initializes every referenced peripheral.
 * EXTI vectors remain disabled until initialization establishes the safe state
 * and publishes all interrupt targets.
 */
class BoardApplication {
public:
  /** @brief Bind all application objects to CubeMX handles and board pins. */
  BoardApplication() noexcept;

  /**
   * @brief Initialize interfaces, configure the 120 W alert, and command safe.
   * @return The first startup failure while still attempting every safe-state
   * operation.
   */
  HAL_StatusTypeDef init() noexcept;

  /**
   * @brief Disable all bridges, zero applied references, and cancel activity.
   * @param turnLedsOff Also command every status LED off when true.
   * @return `HAL_OK` when every synchronous safe-state command was accepted.
   */
  HAL_StatusTypeDef enterSafeState(bool turnLedsOff = true) noexcept;

  /** @brief Service ISR-requested shutdown and fault-release validation. */
  void processSafety() noexcept;

  /** @brief Advance all foreground application services in safety-first order.
   */
  void process() noexcept;

  /**
   * @brief Retain a production setpoint without energizing a bridge.
   * @param currentMilliamps Requested setpoint from 0 through 3000 mA.
   * @return `true` when the request passed all safety and range guards.
   */
  bool setRequestedCoilCurrentMilliamps(uint16_t currentMilliamps) noexcept;

  /**
   * @brief Acknowledge the software fault latch after all release guards pass.
   * @return `true` only when the latch was atomically cleared.
   * @note This operation never toggles nSLEEP.
   */
  bool clearFault() noexcept;

  /**
   * @brief Report whether startup and current safety gates permit commands.
   * @return `true` when high-power commands are currently allowed.
   */
  bool powerStageReadyForCommands() const noexcept;

  /**
   * @brief Report whether the INA226 power alert was configured and verified.
   * @return `true` after successful register readback.
   */
  bool powerAlertConfigured() const noexcept;

  /** @brief Return the board-wide safety state. */
  SystemState systemState() const noexcept;

  /**
   * @brief Report whether all mandatory board initialization passed.
   * @return `true` after successful initialization.
   */
  bool isInitialized() const noexcept;

  /**
   * @brief Return the first failure from the latest initialization.
   * @return Stored initialization result.
   */
  BoardInitializationFailure initializationFailure() const noexcept;

  /** @brief Access the owned user-input service. @return User-input service. */
  UserInput &userInput() noexcept;
  /** @brief Access the owned display. @return Display service. */
  Display &display() noexcept;
  /** @brief Report whether the display initialized. @return Display state. */
  bool isDisplayInitialized() const noexcept;
  /** @brief Access the owned status LEDs. @return Status-LED service. */
  StatusLeds &statusLeds() noexcept;
  /** @brief Access the external sensor DAC. @return External DAC driver. */
  Dac088s085 &externalDac() noexcept;
  /** @brief Access calibration persistence. @return Calibration store. */
  CalibrationStore &calibrationStore() noexcept;
  /** @brief Access runtime sensor control. @return Sensor controller. */
  SensorController &sensorController() noexcept;
  /** @brief Access power-stage control. @return Coil controller. */
  CoilController &coilController() noexcept;

private:
  void enableSafetyInterrupts() noexcept;

  GpioPin _led1;
  GpioPin _led2;
  GpioPin _led3;
  GpioPin _externalDacChipSelect;
  GpioPin _powerAlert;
  GpioPin _userInput1;
  GpioPin _userInput2;
  GpioPin _userInput3;
  UserInput _userInput;
  Display _display;
  StatusLeds _statusLeds;
  Dac088s085 _externalDac;
  CalibrationStore _calibrationStore;
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

/**
 * @brief Return the process-lifetime application composition root.
 * @return The sole foreground-owned board application.
 */
BoardApplication &boardApplication() noexcept;

} // namespace dda

#endif /* __cplusplus */
