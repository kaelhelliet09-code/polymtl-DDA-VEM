/**
 * @file Display.h
 * @brief Declares a blocking LCD1602 driver for a PCF8574T I2cDevice backpack.
 * @details Encapsulates four-bit LCD signaling, cursor positioning, display
 * initialization, and fixed-width status text used during startup.
 */

#pragma once

#include "Platform/Stm32/I2c/I2cDevice.h"

#include <cstdint>

namespace dda {

/**
 * @brief Drives a 16x2 HD44780-compatible LCD through a PCF8574T expander.
 *
 * The driver uses write-only 4-bit LCD transfers and fixed execution delays,
 * so the backpack's R/W line is always held low. The supplied HAL I2cDevice
 * handle is referenced but not owned.
 *
 * @note PCF8574T uses unshifted 7-bit addresses `0x20` through `0x27`; the T
 * suffix denotes its SO16 package. A0, A1, and A2 determine the exact address.
 * @warning Configure the shared bus for at most 100 kHz, the PCF8574T limit.
 * @warning Backpack-to-LCD wiring is not standardized. The default pin map is
 * common but must be checked against the backpack schematic if no text appears.
 */
class Display {
public:
  /** @brief PCF8574T address with A2, A1, and A0 tied low. */
  static constexpr uint8_t MinimumAddress = 0x20U;

  /** @brief PCF8574T address with A2, A1, and A0 tied high. */
  static constexpr uint8_t MaximumAddress = 0x27U;

  /** @brief Common LCD-backpack address when all address straps are high. */
  static constexpr uint8_t DefaultAddress = MaximumAddress;

  /** @brief Number of visible character columns. */
  static constexpr uint8_t ColumnCount = 16U;

  /** @brief Number of visible character rows. */
  static constexpr uint8_t RowCount = 2U;

  /**
   * @brief Maps PCF8574 port bits to LCD signals and backlight polarity.
   * @note Every signal mask must select a different, nonzero PCF8574 bit.
   */
  struct PinMap {
    uint8_t registerSelect;   ///< LCD RS signal mask.
    uint8_t readWrite;        ///< LCD R/W signal mask; always driven low.
    uint8_t enable;           ///< LCD E signal mask.
    uint8_t backlight;        ///< Backpack backlight-control mask.
    uint8_t data4;            ///< LCD D4 signal mask.
    uint8_t data5;            ///< LCD D5 signal mask.
    uint8_t data6;            ///< LCD D6 signal mask.
    uint8_t data7;            ///< LCD D7 signal mask.
    bool backlightActiveHigh; ///< `true` when a set bit enables the backlight.
  };

  /**
   * @brief Common backpack mapping P0=RS, P1=R/W, P2=E, P3=backlight,
   * and P4 through P7=D4 through D7.
   */
  inline static constexpr PinMap DefaultPinMap{
      0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, true};

  /**
   * @brief Binds the LCD to an existing I2cDevice peripheral and PCF8574T
   * address.
   * @param i2c HAL I2cDevice handle retained by reference; it must outlive this
   * object.
   * @param deviceAddress Unshifted 7-bit address in `[0x20, 0x27]`.
   * @param pinMap Backpack signal mapping and backlight polarity.
   */
  explicit Display(I2C_HandleTypeDef &i2c,
                   uint8_t deviceAddress = DefaultAddress,
                   PinMap pinMap = DefaultPinMap) noexcept;

  /** @brief Prevents copying a display with fixed hardware ownership. */
  Display(const Display &) = delete;

  /** @brief Prevents copy assignment of the fixed hardware binding. */
  Display &operator=(const Display &) = delete;

  /** @brief Prevents moving a display with fixed hardware ownership. */
  Display(Display &&) = delete;

  /** @brief Prevents move assignment of the fixed hardware binding. */
  Display &operator=(Display &&) = delete;

  /**
   * @brief Finds a responding PCF8574T and initializes the attached LCD.
   * @return `HAL_OK` after initialization, `HAL_BUSY` if the shared I2cDevice
   * callback context is occupied, or `HAL_ERROR` when no address responds.
   * @note Scans unshifted addresses `0x20` through `0x27`.
   * @warning An acknowledgement cannot identify the target. Use auto-detection
   * only when no unrelated device occupies the PCF8574T address range.
   */
  HAL_StatusTypeDef initAutoDetect() noexcept;

  /**
   * @brief Reports whether the latest initialization completed successfully.
   * @return `true` after initialization returns `HAL_OK`.
   */
  bool isInitialized() const noexcept;

  /**
   * @brief Clears the display and returns the cursor to row 0, column 0.
   * @return HAL status or `HAL_ERROR` before successful initialization.
   * @note Blocks for the LCD clear-command execution delay.
   */
  HAL_StatusTypeDef clear() noexcept;

  /**
   * @brief Positions the cursor within the visible 16x2 area.
   * @param column Zero-based column in `[0, 15]`.
   * @param row Zero-based row in `[0, 1]`.
   * @return HAL status, or `HAL_ERROR` for invalid coordinates or state.
   */
  HAL_StatusTypeDef setCursor(uint8_t column, uint8_t row) noexcept;

  /**
   * @brief Writes at most 16 characters and optionally clears the rest of a
   * row.
   * @param row Zero-based row in `[0, 1]`.
   * @param text Null-terminated text; characters after column 15 are ignored.
   * @param padRemaining Whether to overwrite unused columns with spaces.
   * @return First HAL error or `HAL_ERROR` for invalid state or input.
   */
  HAL_StatusTypeDef writeLine(uint8_t row, const char *text,
                              bool padRemaining = true) noexcept;

private:
  /** @brief HAL timeout for one short blocking PCF8574 write, in milliseconds.
   */
  static constexpr uint32_t TransferTimeoutMs = 10U;

  /**
   * @brief Validates that the eight LCD signals use unique expander bits.
   * @param pinMap Backpack signal mapping to validate.
   * @return `true` when every signal selects a distinct, nonzero bit.
   */
  static bool isPinMapValid(const PinMap &pinMap) noexcept;

  /**
   * @brief Encodes one LCD nibble and the current control signals.
   * @param nibble Low four bits to map onto LCD D4 through D7.
   * @param isData Whether to assert RS for character data.
   * @return PCF8574 output byte with E and R/W held low.
   */
  uint8_t encodeNibble(uint8_t nibble, bool isData) const noexcept;

  /**
   * @brief Writes one PCF8574 output byte without an LCD enable pulse.
   * @param value Complete PCF8574 port state.
   * @return HAL status from the blocking I2cDevice transfer.
   */
  HAL_StatusTypeDef writePort(uint8_t value) noexcept;

  /**
   * @brief Sends one LCD nibble with a low-high-low enable sequence.
   * @param nibble Low four bits to transfer.
   * @param isData Whether the transfer contains character data.
   * @return HAL status from the blocking I2cDevice transfer.
   */
  HAL_StatusTypeDef writeNibble(uint8_t nibble, bool isData) noexcept;

  /**
   * @brief Sends both nibbles of one command or character byte.
   * @param value Byte to transfer, most-significant nibble first.
   * @param isData Whether the byte contains character data.
   * @return First HAL I2cDevice error, or `HAL_OK`.
   */
  HAL_StatusTypeDef writeByte(uint8_t value, bool isData) noexcept;

  /** @brief Writes one character at the current cursor position. */
  HAL_StatusTypeDef writeCharacter(char character) noexcept;

  /**
   * @brief Sends one command and waits for its worst-case execution time.
   * @param command HD44780 instruction byte.
   * @return HAL status from the command transfer.
   */
  HAL_StatusTypeDef writeCommand(uint8_t command) noexcept;

  /**
   * @brief Applies the HD44780 reset and four-bit initialization sequence.
   * @return First HAL I2cDevice error, or `HAL_OK` after the display is ready.
   * @note The PCF8574 address must already have acknowledged.
   */
  HAL_StatusTypeDef initializeController() noexcept;

  I2cDevice _i2c;       ///< Blocking raw-byte transport over the shared bus.
  uint8_t _address;     ///< Current unshifted 7-bit PCF8574T address.
  const PinMap _pinMap; ///< Backpack-specific signal assignment.
  bool _initialized;    ///< Whether the LCD initialization completed.
};

} // namespace dda
