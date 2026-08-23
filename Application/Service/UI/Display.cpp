// Implements LCD1602 four-bit transactions over a PCF8574T I2C backpack and
// the blocking startup/status text operations used by the application.
#include "Service/UI/Display.h"

namespace {

constexpr uint8_t ClearDisplayCommand = 0x01U;
constexpr uint8_t EntryModeIncrementCommand = 0x06U;
constexpr uint8_t DisplayOffCommand = 0x08U;
constexpr uint8_t DisplayOnCommand = 0x0CU;
constexpr uint8_t FourBitTwoLineCommand = 0x28U;
constexpr uint8_t SetDdramAddressCommand = 0x80U;

constexpr uint32_t PowerUpDelayMs = 50U;
constexpr uint32_t FirstFunctionSetDelayMs = 5U;
constexpr uint32_t FunctionSetDelayMs = 1U;
constexpr uint32_t StandardCommandDelayMs = 1U;
constexpr uint32_t ClearDelayMs = 3U;

constexpr uint8_t RowAddress[dda::Display::RowCount] = {0x00U, 0x40U};

bool isSingleBit(uint8_t value) noexcept {
  return (value != 0U) && ((value & static_cast<uint8_t>(value - 1U)) == 0U);
}

} // namespace

namespace dda {

Display::Display(I2C_HandleTypeDef &i2c, uint8_t deviceAddress,
                 PinMap pinMap) noexcept
    : _i2c(i2c, deviceAddress), _address(deviceAddress), _pinMap(pinMap),
      _initialized(false) {}

HAL_StatusTypeDef Display::initAutoDetect() noexcept {
  _initialized = false;
  if (!isPinMapValid(_pinMap)) {
    return HAL_ERROR;
  }

  HAL_Delay(PowerUpDelayMs);
  const uint8_t originalAddress = _address;
  for (uint8_t candidate = MinimumAddress; candidate <= MaximumAddress;
       ++candidate) {
    HAL_StatusTypeDef status = _i2c.setDeviceAddress(candidate);
    if (status != HAL_OK) {
      return status;
    }

    status = _i2c.isDeviceReady(2U, TransferTimeoutMs);
    if (status == HAL_BUSY) {
      return status;
    }
    if (status == HAL_OK) {
      _address = candidate;
      return initializeController();
    }
  }

  // Preserve the constructor-selected address after an unsuccessful scan.
  (void)_i2c.setDeviceAddress(originalAddress);
  _address = originalAddress;
  return HAL_ERROR;
}

HAL_StatusTypeDef Display::initializeController() noexcept {
  // First force E and R/W low because every PCF8574 port powers up high.
  HAL_StatusTypeDef status =
      writePort(_pinMap.backlightActiveHigh ? _pinMap.backlight : 0U);
  if (status != HAL_OK) {
    return status;
  }

  // Recover a controller in an unknown state, then select the 4-bit interface.
  status = writeNibble(0x03U, false);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(FirstFunctionSetDelayMs);

  status = writeNibble(0x03U, false);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(FunctionSetDelayMs);

  status = writeNibble(0x03U, false);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(FunctionSetDelayMs);

  status = writeNibble(0x02U, false);
  if (status != HAL_OK) {
    return status;
  }
  HAL_Delay(FunctionSetDelayMs);

  const uint8_t setupCommands[] = {FourBitTwoLineCommand, DisplayOffCommand,
                                   ClearDisplayCommand,
                                   EntryModeIncrementCommand, DisplayOnCommand};
  for (const uint8_t command : setupCommands) {
    status = writeCommand(command);
    if (status != HAL_OK) {
      return status;
    }
  }

  _initialized = true;
  return HAL_OK;
}

bool Display::isInitialized() const noexcept { return _initialized; }

HAL_StatusTypeDef Display::clear() noexcept {
  return _initialized ? writeCommand(ClearDisplayCommand) : HAL_ERROR;
}

HAL_StatusTypeDef Display::setCursor(uint8_t column, uint8_t row) noexcept {
  if (!_initialized || (column >= ColumnCount) || (row >= RowCount)) {
    return HAL_ERROR;
  }

  return writeCommand(static_cast<uint8_t>(SetDdramAddressCommand |
                                           (RowAddress[row] + column)));
}

HAL_StatusTypeDef Display::writeCharacter(char character) noexcept {
  if (!_initialized) {
    return HAL_ERROR;
  }

  const HAL_StatusTypeDef status =
      writeByte(static_cast<uint8_t>(character), true);
  if (status == HAL_OK) {
    HAL_Delay(StandardCommandDelayMs);
  }
  return status;
}

HAL_StatusTypeDef Display::writeLine(uint8_t row, const char *text,
                                     bool padRemaining) noexcept {
  if (!_initialized || (row >= RowCount) || (text == nullptr)) {
    return HAL_ERROR;
  }

  HAL_StatusTypeDef status = setCursor(0U, row);
  if (status != HAL_OK) {
    return status;
  }

  uint8_t column = 0U;
  while ((column < ColumnCount) && (*text != '\0')) {
    status = writeCharacter(*text++);
    if (status != HAL_OK) {
      return status;
    }
    ++column;
  }

  while (padRemaining && (column < ColumnCount)) {
    status = writeCharacter(' ');
    if (status != HAL_OK) {
      return status;
    }
    ++column;
  }
  return HAL_OK;
}

bool Display::isPinMapValid(const PinMap &pinMap) noexcept {
  const uint8_t masks[] = {
      pinMap.registerSelect, pinMap.readWrite, pinMap.enable, pinMap.backlight,
      pinMap.data4,          pinMap.data5,     pinMap.data6,  pinMap.data7};

  uint8_t usedBits = 0U;
  for (const uint8_t mask : masks) {
    if (!isSingleBit(mask) || ((usedBits & mask) != 0U)) {
      return false;
    }
    usedBits = static_cast<uint8_t>(usedBits | mask);
  }
  return true;
}

uint8_t Display::encodeNibble(uint8_t nibble, bool isData) const noexcept {
  uint8_t value = 0U;
  if ((nibble & 0x01U) != 0U) {
    value = static_cast<uint8_t>(value | _pinMap.data4);
  }
  if ((nibble & 0x02U) != 0U) {
    value = static_cast<uint8_t>(value | _pinMap.data5);
  }
  if ((nibble & 0x04U) != 0U) {
    value = static_cast<uint8_t>(value | _pinMap.data6);
  }
  if ((nibble & 0x08U) != 0U) {
    value = static_cast<uint8_t>(value | _pinMap.data7);
  }
  if (isData) {
    value = static_cast<uint8_t>(value | _pinMap.registerSelect);
  }
  if (_pinMap.backlightActiveHigh) {
    value = static_cast<uint8_t>(value | _pinMap.backlight);
  }
  // R/W and E intentionally remain low in the returned base value.
  return value;
}

HAL_StatusTypeDef Display::writePort(uint8_t value) noexcept {
  return _i2c.transmit(&value, 1U, TransferTimeoutMs);
}

HAL_StatusTypeDef Display::writeNibble(uint8_t nibble, bool isData) noexcept {
  const uint8_t baseValue =
      encodeNibble(static_cast<uint8_t>(nibble & 0x0FU), isData);
  // Each acknowledged byte changes the port, producing one bounded E pulse.
  const uint8_t sequence[] = {
      baseValue, static_cast<uint8_t>(baseValue | _pinMap.enable), baseValue};
  return _i2c.transmit(sequence, sizeof(sequence), TransferTimeoutMs);
}

HAL_StatusTypeDef Display::writeByte(uint8_t value, bool isData) noexcept {
  HAL_StatusTypeDef status =
      writeNibble(static_cast<uint8_t>(value >> 4U), isData);
  if (status == HAL_OK) {
    status = writeNibble(static_cast<uint8_t>(value & 0x0FU), isData);
  }
  return status;
}

HAL_StatusTypeDef Display::writeCommand(uint8_t command) noexcept {
  const HAL_StatusTypeDef status = writeByte(command, false);
  if (status == HAL_OK) {
    HAL_Delay(command == ClearDisplayCommand ? ClearDelayMs
                                             : StandardCommandDelayMs);
  }
  return status;
}

} // namespace dda
