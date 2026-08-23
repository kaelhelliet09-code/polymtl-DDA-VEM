/**
 * @file Ina226.h
 * @brief Declares the board-specific Ina226 power-monitor interface.
 * @details Owns synchronous startup/alert programming and an asynchronous
 * DMA-backed raw-register read state machine.
 */

#pragma once

#include "Platform/Stm32/I2c/I2cDevice.h"

#include <cstdint>

namespace dda {

/**
 * @brief Measures board voltage, current, and power with Ina226.
 * @note The driver owns its I2cDevice wrapper but not the HAL peripheral
 * handle. Asynchronous reads require regular `process()` calls and enabled HAL
 * I2C DMA resources.
 */
class Ina226 {
public:
  /** @brief Unscaled register words produced by an asynchronous snapshot. */
  struct RawMeasurements {
    uint16_t busVoltage; ///< Unsigned Ina226 bus-voltage register word.
    uint16_t current;    ///< Two's-complement Ina226 current register word.
    uint16_t power;      ///< Unsigned Ina226 power register word.
  };

  /** @brief Bit mask selecting registers for an asynchronous raw snapshot. */
  enum MeasurementMask : uint8_t {
    RawPower = 0x01U,      ///< Include the power register.
    RawBusVoltage = 0x02U, ///< Include the bus-voltage register.
    RawCurrent = 0x04U,    ///< Include the current register.
  };

  /** @brief Default unshifted 7-bit I2C address. */
  static constexpr uint8_t DefaultAddress = 0x40U;

  /** @brief Public address of the mask/enable register. */
  static constexpr uint8_t MaskEnableRegister = 0x06U;

  /** @brief Public address of the alert-limit register. */
  static constexpr uint8_t AlertLimitRegister = 0x07U;

  /** @brief Enables the active-low, transparent power-over-limit alert. */
  static constexpr uint16_t PowerOverLimitAlertMask = 0x0800U;

  /** @brief Writable alert-function, polarity, and latch-enable bits. */
  static constexpr uint16_t AlertConfigurationWritableMask = 0xFC03U;

  /** @brief Address of the manufacturer-identification register. */
  static constexpr uint8_t ManufacturerIdRegister = 0xFEU;

  /** @brief Address of the die-identification register. */
  static constexpr uint8_t DieIdRegister = 0xFFU;

  /** @brief Expected Texas Instruments manufacturer identifier. */
  static constexpr uint16_t ExpectedManufacturerId = 0x5449U;

  /** @brief Expected Ina226 die identifier after applying @ref DieIdMask. */
  static constexpr uint16_t ExpectedDieId = 0x2260U;

  /** @brief Mask excluding revision bits from the die identifier. */
  static constexpr uint16_t DieIdMask = 0xFFF0U;

  /**
   * @brief Constructs a monitor for one Ina226 and shunt.
   * @param i2c HAL I2C handle; must outlive this object.
   * @param shuntResistanceOhms Shunt resistance in ohms.
   * @param maximumExpectedCurrentA Required signed measurement range in
   * amperes.
   * @param address Unshifted 7-bit Ina226 address.
   */
  Ina226(I2C_HandleTypeDef &i2c, float shuntResistanceOhms,
         float maximumExpectedCurrentA,
         uint8_t address = DefaultAddress) noexcept;

  Ina226(const Ina226 &) = delete;
  Ina226 &operator=(const Ina226 &) = delete;

  /**
   * @brief Computes calibration and synchronously applies the board config.
   * @return `HAL_OK`, `HAL_ERROR` for invalid calibration inputs, or the first
   * HAL I2C error.
   */
  HAL_StatusTypeDef init() noexcept;

  /**
   * @brief Synchronously reads one 16-bit Ina226 register.
   * @param registerAddress Eight-bit Ina226 register address.
   * @param[out] value Register value on success.
   * @param timeoutMs HAL transaction timeout.
   * @return HAL transaction status.
   */
  HAL_StatusTypeDef readRegister(uint8_t registerAddress, uint16_t &value,
                                 uint32_t timeoutMs) noexcept;

  /**
   * @brief Programs and verifies an active-low power-over-limit alert.
   * @param thresholdMilliwatts Requested threshold in integer milliwatts.
   * @param timeoutMs Timeout for each bounded register operation.
   * @return `HAL_OK` after successful readback, otherwise the first error.
   */
  HAL_StatusTypeDef configurePowerOverLimitAlert(uint32_t thresholdMilliwatts,
                                                 uint32_t timeoutMs) noexcept;

  /**
   * @brief Starts a DMA-backed raw measurement snapshot.
   * @param measurementMask Nonzero OR-combination of @ref MeasurementMask.
   * @return HAL start status, or `HAL_BUSY` when another operation is active.
   */
  HAL_StatusTypeDef
  readMeasurementsNonBlocking(uint8_t measurementMask) noexcept;

  /** @brief Advances asynchronous state from HAL completion notifications. */
  void process() noexcept;

  /** @brief Cooperatively cancels an asynchronous measurement snapshot. */
  void cancelRead() noexcept;

  /**
   * @brief Requests HAL abortion of an active measurement-register read.
   * @return HAL status from requesting the abort.
   */
  HAL_StatusTypeDef abortRead() noexcept;

  /**
   * @brief Consumes one completed raw measurement snapshot.
   * @param[out] values Raw selected register values on success.
   * @return `true` when one complete snapshot was consumed.
   */
  bool takeRawMeasurements(RawMeasurements &values) noexcept;

  /**
   * @brief Consumes a latched asynchronous operation failure.
   * @return `true` when one failure notification was consumed.
   */
  bool takeAsyncFailure() noexcept;

  /**
   * @brief Reports whether initialization is complete and no result pending.
   * @return `true` when another measurement may be started.
   */
  bool isReady() const noexcept;

  /**
   * @brief Reports whether an asynchronous HAL read or abort is active.
   * @return `true` while asynchronous HAL work remains active.
   */
  bool isBusy() const noexcept;

private:
  /** @brief Ina226 registers used by production board operations. */
  enum class Register : uint8_t {
    Configuration = 0x00U,
    BusVoltage = 0x02U,
    Power = 0x03U,
    Current = 0x04U,
    Calibration = 0x05U,
  };

  /** @brief Lifecycle of initialization and raw measurement operations. */
  enum class AsyncState : uint8_t {
    Idle,
    Ready,
    ReadingPower,
    ReadingBusVoltage,
    ReadingCurrent,
    MeasurementComplete,
    Failed,
  };

  /** @brief Computes a representable calibration and exact quantized LSB. */
  bool configureCalibration() noexcept;

  /** @brief Starts the next selected DMA register read. */
  HAL_StatusTypeDef startNextMeasurementRead() noexcept;

  /** @brief Resets active work and latches a foreground-visible failure. */
  void failAsyncOperation() noexcept;

  /** @brief Returns the calibrated power-register scale. */
  float powerLsbW() const noexcept;

  I2cDevice _i2c;                       ///< Register transport.
  const float _shuntResistanceOhms;     ///< Shunt resistance in ohms.
  const float _maximumExpectedCurrentA; ///< Required signed current range.
  float _currentLsbA;                   ///< Amperes per current-register bit.
  uint16_t _calibration;                ///< Calibration-register value.
  RawMeasurements _rawMeasurements;    ///< Active raw snapshot accumulator.
  uint8_t _remainingMeasurementMask;    ///< Selected reads not yet started.
  AsyncState _asyncState;               ///< Current lifecycle state.
  bool _asyncFailureAvailable;          ///< Consumable asynchronous failure.
  bool _cancelReadRequested;            ///< Discard or abort active read.
};

} // namespace dda
