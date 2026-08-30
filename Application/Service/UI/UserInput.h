/**
 * @file UserInput.h
 * @brief Declares a debounced foreground interface for three user buttons.
 * @details Publishes new-press events used by foreground UI workflows.
 */

#pragma once

#include "Config/BoardConfig.h"
#include "Platform/Stm32/Gpio/GpioPin.h"

#include <cstdint>

namespace dda {

/** @brief Identifies one of the three CubeMX-configured user inputs. */
enum class UserInputId : uint8_t {
  Input1 = 0U, ///< USER_IN1 on PD1.
  Input2 = 1U, ///< USER_IN2 on PD2.
  Input3 = 2U, ///< USER_IN3 on PD3.
};

/**
 * @brief Polls and debounces three active-high button inputs.
 *
 * Call process() regularly from the foreground loop. Press events are latched
 * until consumed, so no interrupt or dynamic allocation is required.
 *
 * @note BoardApplication uses active-high buttons with CubeMX pull-down
 * resistors; a pressed switch should connect its input to the 3.3 V logic rail.
 */
class UserInput {
public:
  /** @brief Stable sampling interval used to reject contact bounce, in ms. */
  static constexpr uint32_t DebounceTimeMs =
      config::UserInputDebounceMilliseconds;

  /**
   * @brief Binds the three already-configured button GPIOs.
   * @param input1 USER_IN1 GpioPin retained by reference.
   * @param input2 USER_IN2 GpioPin retained by reference.
   * @param input3 USER_IN3 GpioPin retained by reference.
   * @note All referenced GpioPin objects must outlive this object.
   */
  UserInput(GpioPin &input1, GpioPin &input2, GpioPin &input3) noexcept;

  /**
   * @brief Samples all buttons and updates their debounced state.
   * @note Call regularly; slower polling increases response latency.
   */
  void process() noexcept;

  /**
   * @brief Waits for a new debounced press after requiring an initial release.
   * @param input Button whose next fresh press should complete the wait.
   */
  void waitForFreshPressBlocking(UserInputId input) noexcept;
  /**
   * @brief Consumes one pending pressed-edge event.
   * @param input Button whose event should be consumed.
   * @return `true` once per debounced press; otherwise `false`.
   * @note The button must be released before another press can be generated.
   */
  bool takePress(UserInputId input) noexcept;

  /**
   * @brief Clears an event and suppresses presses until a stable release.
   * @param input Button that must be released before it can fire again.
   * @note Use when entering a new UI state to reject an earlier press.
   */
  void requireRelease(UserInputId input) noexcept;

private:
  /** @brief Number of button GPIOs managed by this class. */
  static constexpr uint8_t InputCount = 3U;

  /** @brief Debounce state for one button. */
  struct ButtonState {
    bool sampledPressed;      ///< Most recently sampled logical state.
    bool stablePressed;       ///< State accepted after the debounce interval.
    bool pressPending;        ///< Unconsumed stable pressed-edge event.
    bool releaseRequired;     ///< Whether events are blocked pending release.
    uint32_t sampleChangedMs; ///< Tick when sampledPressed last changed.
  };

  /**
   * @brief Converts a public identifier to an array index.
   * @param input Button identifier to convert.
   * @return Index in `[0, 2]`, or InputCount for an invalid value.
   */
  static uint8_t indexOf(UserInputId input) noexcept;

  /**
   * @brief Reads one active-high GpioPin.
   * @param index Button array index in `[0, 2]`.
   * @return Logical pressed state.
   */
  bool readPressed() const noexcept;

  GpioPin _buttonPin;  ///< Non-owning button GpioPin pointers.
  ButtonState _states; ///< Fixed debounce and event storage.
};

} // namespace dda
