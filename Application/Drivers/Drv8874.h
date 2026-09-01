/**
 * @file Drv8874.h
 * @brief GPIO and external-DAC control for one DRV8874 bridge.
 */

#pragma once

#include "Drivers/Dac088s085.h"
#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

namespace dda {

/**
 * @brief Controls one DRV8874 using the truth table selected by PMODE.
 *
 * Every instance retains a reference to the same board-level DAC object and
 * owns only its assigned DAC channel. PMODE itself is shared by all four
 * devices and is therefore controlled by CoilController.
 */
class Drv8874 {
public:
  /** @brief Commanded logical bridge state. */
  enum class State : uint8_t {
    Sleep,     ///< nSLEEP low; outputs high impedance.
    CoilOff,   ///< Awake and non-driving; coast in PWM, brake in PH/EN.
    Forward,   ///< Forward output polarity in the active truth table.
    Reverse,   ///< Reverse output polarity in the active truth table.
    SlowDecay, ///< Low-side brake; equivalent to CoilOff in PH/EN.
  };

  /**
   * @brief Bind one bridge to GPIOs and one channel of the shared external DAC.
   * @param in1Port GPIO port for DRV8874 IN1.
   * @param in1Pin GPIO pin mask for IN1.
   * @param in2Port GPIO port for DRV8874 IN2.
   * @param in2Pin GPIO pin mask for IN2.
   * @param sleepPort GPIO port for active-high nSLEEP.
   * @param sleepPin GPIO pin mask for nSLEEP.
   * @param faultPort GPIO port for active-low nFAULT.
   * @param faultPin GPIO pin mask for nFAULT.
   * @param dac Shared board DAC; must outlive this driver.
   * @param dacChannel DAC output wired to this device's VREF input.
   */
  Drv8874(GPIO_TypeDef *in1Port, uint16_t in1Pin, GPIO_TypeDef *in2Port,
          uint16_t in2Pin, GPIO_TypeDef *sleepPort, uint16_t sleepPin,
          GPIO_TypeDef *faultPort, uint16_t faultPin, Dac088s085 &dac,
          DacChannel dacChannel) noexcept;

  Drv8874(const Drv8874 &) = delete;
  Drv8874 &operator=(const Drv8874 &) = delete;

  /**
   * @brief Put the bridge to sleep, clear its software fault, and zero VREF.
   * @return DAC write status, or `HAL_ERROR` if the GPIO state was not applied.
   */
  HAL_StatusTypeDef init() noexcept;

  /**
   * @brief Apply one bridge state using the cached PMODE truth table.
   * @param state Requested logical bridge state.
   * @return Whether every GPIO operation and safety precondition succeeded.
   */
  bool setState(State state) noexcept;

  /**
   * @brief Return the last successfully commanded bridge state.
   * @return Cached state.
   */
  State state() const noexcept;

  /**
   * @brief Select the truth table associated with the shared PMODE pin.
   * @param pwmMode `true` for PWM mode, `false` for PH/EN mode.
   * @return `true` only while this driver is asleep.
   */
  bool setPwmMode(bool pwmMode) noexcept;

  /** @brief Return the cached PMODE interpretation. @return `true` for PWM. */
  bool pwmMode() const noexcept;

  /**
   * @brief Retain this bridge's current threshold and apply it when awake.
   * @param currentMilliamps Threshold from 0 through the configured maximum.
   * @return External-DAC write status, or `HAL_OK` when retained in sleep.
   * @note VREF is a live analog input; changing it does not require nSLEEP.
   * While asleep, VREF remains at its safe zero and the value is retained for
   * the next energized transition.
   */
  HAL_StatusTypeDef
  setCurrentThresholdMilliamps(uint16_t currentMilliamps) noexcept;

  /**
   * @brief Apply the retained threshold after a safe-state zeroing operation.
   * @return External-DAC write status.
   */
  HAL_StatusTypeDef applyCurrentThreshold() noexcept;

  /**
   * @brief Write zero to VREF without changing the retained threshold.
   * @return External-DAC write status.
   */
  HAL_StatusTypeDef disableCurrentThresholdOutput() noexcept;

  /**
   * @brief Return the retained current threshold in milliamperes.
   * @return Configured current threshold.
   */
  uint16_t currentThresholdMilliamps() const noexcept;

  /**
   * @brief Return the threshold most recently applied to VREF.
   * @return Applied current threshold.
   */
  uint16_t appliedCurrentThresholdMilliamps() const noexcept;

  /**
   * @brief Return this bridge's external-DAC channel.
   * @return Assigned DAC output.
   */
  DacChannel dacChannel() const noexcept;

  /**
   * @brief Report a latched interrupt, asserted nFAULT, or read failure.
   * @return Whether a fault is present.
   */
  bool hasFault() const noexcept;

  /**
   * @brief Clear the software latch only while nFAULT is released.
   * @return Whether the latch was cleared.
   */
  bool clearFault() noexcept;

  /**
   * @brief Report whether the active-low hardware fault input is high.
   * @return Whether nFAULT is released and readable.
   */
  bool faultInputReleased() const noexcept;

  /** @brief Latch a driver fault from interrupt context. */
  void onFaultInterrupt() noexcept;

  /**
   * @brief Validate an underlying State value.
   * @param state Value to validate.
   * @return Whether the value names a supported state.
   */
  static bool isValidState(State state) noexcept;

private:
  bool applyState(State state) noexcept;
  HAL_StatusTypeDef writeCurrentThreshold(uint16_t currentMilliamps) noexcept;

  GpioPin _in1;
  GpioPin _in2;
  GpioPin _sleep;
  GpioPin _fault;
  Dac088s085 &_dac;
  const DacChannel _dacChannel;
  volatile State _state;
  uint16_t _currentThresholdMilliamps;
  uint16_t _appliedCurrentThresholdMilliamps;
  bool _pwmMode;
  volatile bool _faultLatched;
};

} // namespace dda
