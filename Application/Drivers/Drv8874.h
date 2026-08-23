/**
 * @file Drv8874.h
 * @brief Declares GpioPin control and fault latching for one Drv8874 bridge.
 * @details Encodes the board's PH/EN truth table, active-high nSLEEP
 * sequencing, active-low nFAULT handling, and direction-change dead time.
 */

#pragma once

#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

namespace dda {

/**
 * @brief Controls one Drv8874 full bridge through nSLEEP, EN, and PH pins.
 * @note PMODE is tied low: the board nets named IN1 and IN2 are respectively
 * drive enable (EN) and phase (PH). The board enable net controls nSLEEP.
 */
class Drv8874 {
public:
  /** @brief Commanded bridge truth-table state. */
  enum class State : uint8_t {
    Sleep,     ///< nSLEEP low; H-bridge high impedance.
    CoilOff,   ///< nSLEEP high and EN low; low-side brake.
    Forward,   ///< nSLEEP high, EN high, and PH high.
    Reverse,   ///< nSLEEP high, EN high, and PH low.
    SlowDecay, ///< Same hardware command as CoilOff in PH/EN mode.
  };

  /**
   * @brief Binds the driver to its four board-level GpioPin signals.
   * @param driveEnablePort HAL port for the board IN1 net (DRV8874 EN).
   * @param driveEnablePin HAL pin mask for drive enable.
   * @param phasePort HAL port for the board IN2 net (DRV8874 PH).
   * @param phasePin HAL pin mask for phase selection.
   * @param sleepControlPort HAL port for the active-high nSLEEP signal.
   * @param sleepControlPin HAL pin mask for nSLEEP.
   * @param faultPort HAL port for the active-low fault signal.
   * @param faultPin HAL pin mask for the fault signal.
   */
  Drv8874(GPIO_TypeDef *driveEnablePort, uint16_t driveEnablePin,
          GPIO_TypeDef *phasePort, uint16_t phasePin,
          GPIO_TypeDef *sleepControlPort, uint16_t sleepControlPin,
          GPIO_TypeDef *faultPort, uint16_t faultPin) noexcept;

  Drv8874(const Drv8874 &) = delete;
  Drv8874 &operator=(const Drv8874 &) = delete;

  /**
   * @brief Disables the bridge and clears the software fault latch.
   * @note This does not clear a fault that remains asserted by the hardware.
   */
  void init() noexcept;

  /**
   * @brief Applies one bridge truth-table state.
   * @param state Requested bridge state.
   * @return `true` when the state is valid and every GpioPin command succeeds.
   * @note Every transition from Sleep to an awake state observes the
   * configured all-off interval. If the independent microsecond timer is not
   * running, the request fails with the bridge left disabled.
   */
  bool setState(State state) noexcept;

  /**
   * @brief Returns the last commanded bridge state.
   * @return Cached command state; this is not hardware readback.
   */
  State state() const noexcept;

  /**
   * @brief Reports a latched interrupt, asserted fault pin, or pin-read
   * failure.
   * @return `true` when the active-low fault is present or cannot be sampled.
   */
  bool hasFault() const noexcept;

  /**
   * @brief Clears the software fault latch if the hardware fault is released.
   * @return `true` when the active-low fault input was read high and the latch
   * was cleared; otherwise `false`.
   */
  bool clearFault() noexcept;

  /**
   * @brief Reports whether the active-low hardware fault input is released.
   * @return `true` only when the GpioPin read succeeds and the pin is high.
   */
  bool faultInputReleased() const noexcept;

  /**
   * @brief Latches a bridge fault reported by the GpioPin interrupt.
   * @note This ISR-facing method performs no GpioPin access or bridge shutdown.
   */
  void onFaultInterrupt() noexcept;

  /**
   * @brief Validates an underlying state value.
   * @param state Candidate truth-table value.
   * @return `true` only for one of the declared State enumerators.
   */
  static bool isValidState(State state) noexcept;

private:
  /** @brief Applies one validated bridge state. */
  bool applyState(State state) noexcept;

  /** @brief Non-owning wrapper for the bridge drive-enable input. */
  GpioPin _driveEnable;

  /** @brief Non-owning wrapper for the bridge phase input. */
  GpioPin _phase;

  /** @brief Non-owning wrapper for the active-high nSLEEP control. */
  GpioPin _sleepControl;

  /** @brief Non-owning wrapper for the active-low fault input. */
  GpioPin _fault;

  /** @brief Last state commanded through `setState()`. */
  volatile State _state;

  /** @brief TIM2 count captured when the bridge most recently became disabled.
   */
  uint32_t _disabledAtTicks;

  /** @brief Whether `_disabledAtTicks` came from a running TIM2. */
  bool _disabledAtValid;

  /** @brief Fault flag written by interrupt context and read in foreground. */
  volatile bool _faultLatched;

};

} // namespace dda
