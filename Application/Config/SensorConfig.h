/**
 * @file SensorConfig.h
 * @brief Configurable sensor hardware mapping and calibration parameters.
 *
 * @note GPIO assignments must also match the CubeMX configuration in
 * `DDA.ioc`.
 */

#pragma once

#include "Config/BoardConfig.h"
#include "Drivers/Dac088s085.h"
#include "Drivers/Sensors/SensorId.h"

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace dda::config {

/** @brief Fixed GPIO and external-DAC mapping for one sensor channel. */
struct SensorHardwareConfig {
  SensorId id;                   ///< Zero-based sensor identity.
  GPIO_TypeDef *inputPort;       ///< Trigger-input GPIO port.
  uint16_t inputPin;             ///< Trigger-input GPIO pin mask.
  DacChannel ledCurrentChannel;  ///< DAC channel controlling LED current.
  DacChannel tripVoltageChannel; ///< DAC channel controlling trip voltage.
};

/** @brief Complete compile-time hardware mapping for all sensor channels. */
inline const SensorHardwareConfig SensorHardwareConfigs[SensorCount] = {
    {SensorId::SENSOR_1, SENSOR_1_GPIO_Port, SENSOR_1_Pin, DAC_CHANNEL_D,
     DAC_CHANNEL_C},
    {SensorId::SENSOR_2, SENSOR_2_GPIO_Port, SENSOR_2_Pin, DAC_CHANNEL_B,
     DAC_CHANNEL_A},
    {SensorId::SENSOR_3, SENSOR_3_GPIO_Port, SENSOR_3_Pin, DAC_CHANNEL_F,
     DAC_CHANNEL_E},
    {SensorId::SENSOR_4, SENSOR_4_GPIO_Port, SENSOR_4_Pin, DAC_CHANNEL_H,
     DAC_CHANNEL_G},
};

inline constexpr uint32_t SensorDacTimeoutMilliseconds = 100U;
///< Normal blocking external-DAC write timeout.
inline constexpr uint32_t SensorCalibrationDacTimeoutMilliseconds = 1U;
///< Calibration-loop external-DAC write timeout.

inline constexpr uint8_t SensorCalibrationInitialLedCode = 16U;
///< Initial LED-current code used by calibration.
inline constexpr uint8_t SensorCalibrationInitialTripCode = 16U;
///< Initial comparator-trip code used by calibration.
inline constexpr uint8_t SensorCalibrationMaximumDacCode = 200U;
///< Maximum permitted calibration DAC code.
inline constexpr uint32_t SensorCalibrationSettlingMilliseconds = 2U;
///< Settling delay after changing a calibration DAC output.
inline constexpr GPIO_PinState SensorTriggeredState = GPIO_PIN_SET;
///< GPIO state treated as a sensor trigger.
inline constexpr uint8_t DefaultSensorLedCurrentCode = 200U;
inline constexpr uint8_t DefaultSensorTripVoltageCode = 200U;
static_assert(SensorCalibrationInitialLedCode <=
                      SensorCalibrationMaximumDacCode &&
                  SensorCalibrationInitialTripCode <=
                      SensorCalibrationMaximumDacCode,
              "Sensor calibration must start inside its configured range");

} // namespace dda::config
