/**
 * @file CoilController.h
 * @brief Declares coordinated bridge-state and current-reference control.
 * @details Coordinates four drivers, four external-DAC VREF channels, shared
 * PMODE, wake qualification, live fault sampling, and immediate shutdown.
 */

#pragma once

#include "Config/BoardConfig.h"
#include "Drivers/Dac088s085.h"
#include "Drivers/Drv8874.h"
#include "Platform/Stm32/Gpio/GpioPin.h"
#include "Service/Safety/SafetyManager.h"

#include <cstdint>

namespace dda {

/** @brief Board-level index of one coil bridge. */
enum class Driver : uint8_t {
  H1 = 0 /**< Coil bridge H1. */,
  H2 = 1 /**< Coil bridge H2. */,
  H3 = 2 /**< Coil bridge H3. */,
  H4 = 3 /**< Coil bridge H4. */
};

/**
 * @brief Owns four bridge drivers and the shared DRV configuration GPIO.
 * @note Each bridge has an independent DAC088S085 VREF channel.
 */
class CoilController {
public:
  /**
   * @brief Binds the controller to the board's fixed GpioPin and DAC resources.
   * @param safetyManager Shared fault-policy service.
   * @param dac Shared external DAC that owns all four VREF outputs.
   * @param pmode Shared DRV8874 PMODE output.
   */
  CoilController(SafetyManager &safetyManager, Dac088s085 &dac,
                 GpioPin &pmode) noexcept;

  /**
   * @brief Disables every bridge, selects PWM mode, and zeros all four VREFs.
   * @return `HAL_OK` on success or the first GPIO/DAC error.
   */
  HAL_StatusTypeDef init() noexcept;

  /**
   * @brief Applies one state to all four bridges.
   * @param state Requested Drv8874 truth-table state.
   * @return `true` when every cached driver state matches @p state.
   * @note Energized commands apply the requested current first and are rejected
   * while any live or latched fault is present.
   */
  bool setDriverState(Drv8874::State state) noexcept;

  /**
   * @brief Applies one state to a selected bridge.
   * @param driver Board bridge index H1 through H4.
   * @param state Requested Drv8874 truth-table state.
   * @return `false` for an invalid driver index or state mismatch.
   * @note Energized commands apply the requested current first and are rejected
   * while any live or latched fault is present.
   */
  bool setDriverState(Driver driver, Drv8874::State state) noexcept;

  /** @brief Process one routed coil request. @param request Request to handle. */
  void processRequest(Request &request) noexcept;
  /**
   * @brief Returns the last commanded state for one bridge.
   * @param driver Board bridge index H1 through H4.
   * @return Cached bridge state, or `Sleep` for an invalid index.
   */
  Drv8874::State driverState(Driver driver) const noexcept;

  /**
   * @brief Changes all four retained current setpoints.
   * @param currentMilliamps Requested setpoint in the inclusive range 0..3000.
   * @return `true` when every bridge accepted the value and no blocking fault
   * is present. Awake bridges update VREF immediately; sleeping bridges retain
   * the value while their VREF remains zero.
   */
  bool setRequestedCurrentMilliamps(uint16_t currentMilliamps) noexcept;

  /**
   * @brief Change one bridge's current threshold, including while it is active.
   * @param driver Bridge H1 through H4.
   * @param currentMilliamps Threshold from 0 through 3000 mA in protocol use.
   * @return `true` when the value was retained and, if awake, VREF updated.
   */
  bool setCurrentThresholdMilliamps(Driver driver,
                                    uint16_t currentMilliamps) noexcept;

  /**
   * @brief Returns the retained production current setpoint.
   * @return Requested current in milliamperes.
   */
  uint16_t requestedCurrentMilliamps() const noexcept;

  /**
   * @brief Return one bridge's retained current threshold.
   * @param driver Bridge H1 through H4.
   * @return Configured threshold, or zero for an invalid index.
   */
  uint16_t currentThresholdMilliamps(Driver driver) const noexcept;

  /**
   * @brief Returns the greatest current reference applied to any bridge.
   * @return Maximum applied current in milliamperes.
   */
  uint16_t appliedCurrentMilliamps() const noexcept;

  /**
   * @brief Return one bridge's currently applied VREF threshold.
   * @param driver Bridge H1 through H4.
   * @return Applied threshold, or zero for an invalid index.
   */
  uint16_t appliedCurrentMilliamps(Driver driver) const noexcept;

  /**
   * @brief Report whether all four external-DAC VREF outputs are zero.
   * @return Whether every cached applied threshold is zero.
   */
  bool allCurrentThresholdOutputsDisabled() const noexcept;

  /**
   * @brief Change the shared PMODE level after all drivers enter sleep.
   * @param pwmMode `true` for PWM mode, `false` for PH/EN mode.
   * @return `false` unless every bridge is asleep and VREF is zero.
   */
  bool setPmode(bool pwmMode) noexcept;

  /** @brief Return the cached shared PMODE level. @return `true` for PWM. */
  bool pmode() const noexcept;

  /**
   * @brief Wakes one driver in the active mode's non-driving state.
   * @param driver Bridge selected for a subsequent command.
   * @return `true` when nSLEEP was raised and its blocking wake delay elapsed.
   * @note A short nFAULT assertion is recorded but not classified until
   * `completeDriverWakeQualification()` samples the pin after tWAKE.
   */
  bool beginTestDriverWake(Driver driver) noexcept;

  /**
   * @brief Qualifies a driver after its blocking wake delay has elapsed.
   * @param driver Bridge whose wake interval completed.
   * @return `true` only when nFAULT and POWER_ALERT are released.
   */
  bool completeDriverWakeQualification(Driver driver) noexcept;

  /**
   * @brief Commands all bridges off before requesting a zero-current reference.
   * @return `HAL_OK` when all GpioPin commands are accepted and both DAC writes
   * succeed.
   * @warning Bridge state is cached command state, not hardware readback.
   */
  HAL_StatusTypeDef disableAll() noexcept;

  /**
   * @brief Checks whether every bridge's cached state is disabled.
   * @return `true` when all bridge states are `Sleep`.
   * @note This does not read the physical enable pins.
   */
  bool allDriversDisabled() const noexcept;

  /**
   * @brief Checks every bridge for a live, latched, or unreadable fault.
   * @return `true` when at least one bridge reports a fault.
   */
  bool hasAnyFault() const noexcept;

  /**
   * @brief Checks for a fault that requires every bridge to be disabled.
   * @return `true` for power-alert or safe-state faults.
   */
  bool hasGlobalFault() const noexcept;

  /**
   * @brief Checks that every bridge with a latched driver fault is off.
   * @return `true` after all faulted bridges have been isolated.
   */
  bool faultedDriversDisabled() const noexcept;

  /**
   * @brief Checks one bridge for a fault.
   * @param driver Board bridge index H1 through H4.
   * @return `true` for a valid faulted driver; invalid indices return `false`.
   */
  bool hasFault(Driver driver) const noexcept;

  /**
   * @brief Clears driver-local hardware latches after release validation.
   * @param expectedFaultEpoch Epoch captured before the release-validation
   * interval began.
   * @return `true` only with all commands off, zero applied current, and all
   * nFAULT/POWER_ALERT pins high, with no newer or pending safety edge.
   * @note SafetyManager fault memory is cleared separately by BoardApplication.
   * This never pulses or toggles nSLEEP.
   */
  bool clearFaultHardware(uint32_t expectedFaultEpoch) noexcept;

  /**
   * @brief Returns the complete software fault mask.
   * @return OR-combination of `PowerStageFault` values.
   */
  uint32_t faultMask() const noexcept;

  /**
   * @brief Returns the monotonic epoch advanced by every latched fault.
   * @return Current fault epoch.
   */
  uint32_t faultEpoch() const noexcept;

  /**
   * @brief Samples POWER_ALERT and the nFAULT inputs of awake drivers.
   * @note Sleeping-driver nFAULT levels are qualified when that driver wakes.
   */
  void sampleAndLatchFaultInputs() noexcept;

  /**
   * @brief Returns whether all nFAULT and POWER_ALERT inputs are high.
   * @return `true` only when every observable fault input is released.
   */
  bool allFaultInputsReleased() const noexcept;

  /**
   * @brief Completes pending per-driver isolation work.
   * @return `HAL_OK` when no shutdown is pending or isolation completed.
   */
  HAL_StatusTypeDef servicePendingDriverShutdowns() noexcept;

  /**
   * @brief ISR entry for an active-low driver fault edge.
   * @param driver Bridge associated with the asserted nFAULT input.
   * @note Immediately disables and cancels timing for an awake bridge. Edges
   *       while nSLEEP is low or wake qualification is active are ignored.
   */
  static void handleDriverFaultFromIsr(Driver driver) noexcept;

  /**
   * @brief Latch every safety edge already pending in the EXTI controller.
   * @note Used before deferred EXTI enable so startup pulses are not discarded.
   * @return Falling-edge mask captured and represented in the software latch.
   */
  static uint32_t latchPendingSafetyEdges() noexcept;

  /** @brief ISR entry for an active-low board-power alert edge. */
  static void handlePowerAlertFromIsr() noexcept;

private:
  /** @brief Number of coil bridges present on the board. */
  static constexpr uint8_t DriverCount = config::DriverCount;

  /** @brief Apply one retained hardware reference before energizing. */
  HAL_StatusTypeDef applyCurrentThreshold(uint8_t index) noexcept;

  /** @brief Zero every bridge VREF without changing retained thresholds. */
  HAL_StatusTypeDef disableCurrentThresholdOutputs() noexcept;

  /** @brief Forces every cached bridge state to Sleep. */
  bool disableDriverGpios() noexcept;

  /** Performs an atomic precheck, energized state change, and postcheck. */
  bool setEnergizedDriverStateSafely(uint8_t index,
                                     Drv8874::State state) noexcept;

  /** Returns whether the physical inputs permit enabling one driver. */
  bool driverEnableInputsAreSafe(uint8_t index) const noexcept;

  /** Rechecks an enabled driver and immediately isolates an observed fault. */
  bool verifyEnabledDriverOrIsolate(uint8_t index) noexcept;

  /** Restores the appropriate disabled state after an enable failure. */
  void isolateAfterFailedEnable(uint8_t index) noexcept;

  /** Immediately isolates and reports one driver fault. */
  void latchDriverFault(Driver driver) noexcept;

  /** Immediately clears all four nSLEEP outputs through their GPIO banks. */
  static void forceBridgeEnablesLow() noexcept;

  /** Reports an unrecoverable failure to complete full safe state. */
  static void latchSafeStateFailure() noexcept;

  /** @brief Bridge interfaces indexed by @ref Driver. */
  Drv8874 _drivers[DriverCount];

  /** @brief Non-owning shared PMODE GPIO output. */
  GpioPin &_pmode;

  /** @brief Cached shared PMODE level used by every Drv8874. */
  bool _pwmMode;

  /** Board-wide safety service receiving fault reports. */
  SafetyManager &_safetyManager;
};

} // namespace dda
