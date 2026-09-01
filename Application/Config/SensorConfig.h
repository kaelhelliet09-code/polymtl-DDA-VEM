/**
 * @file SensorConfig.h
 * @brief Configurable sensor GPIO/VTRIP mapping and capture debounce.
 *
 * @note GPIO assignments must also match the CubeMX configuration in
 * `DDA_V2.ioc`.
 */

#pragma once

#include "Config/BoardConfig.h"
#include "Config/ExternalDacConfig.h"
#include "Drivers/Sensors/SensorId.h"

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace dda::config {

/** @brief Fixed GPIO and external-DAC mapping for one sensor channel. */
struct SensorHardwareConfig {
  SensorId id;                    ///< Zero-based sensor identity.
  GPIO_TypeDef *inputPort;        ///< Trigger-input GPIO port.
  uint16_t inputPin;              ///< Trigger-input GPIO pin mask.
  GPIO_TypeDef *irLedEnablePort;  ///< GPIO output driving the sensor IR LED.
  uint16_t irLedEnablePin;        ///< IR-LED enable GPIO pin mask.
  DacChannel tripVoltageChannel;  ///< DAC channel controlling trip voltage.
};

/** @brief Complete compile-time hardware mapping for all sensor channels. */
inline const SensorHardwareConfig SensorHardwareConfigs[SensorCount] = {
    {SensorId::SENSOR_1, SENSOR_1_GPIO_Port, SENSOR_1_Pin,
     SENSOR_ENA_GPIO_Port, SENSOR_ENA_Pin, SensorTripVoltageDacChannels[0]},
    {SensorId::SENSOR_2, SENSOR_2_GPIO_Port, SENSOR_2_Pin,
     SENSOR_ENA_GPIO_Port, SENSOR_ENA_Pin, SensorTripVoltageDacChannels[1]},
    {SensorId::SENSOR_3, SENSOR_3_GPIO_Port, SENSOR_3_Pin,
     SENSOR_ENA_GPIO_Port, SENSOR_ENA_Pin, SensorTripVoltageDacChannels[2]},
    {SensorId::SENSOR_4, SENSOR_4_GPIO_Port, SENSOR_4_Pin,
     SENSOR_ENA_GPIO_Port, SENSOR_ENA_Pin, SensorTripVoltageDacChannels[3]},
};

/**
 * @brief Records the unresolved CubeMX limitation in the current pin map.
 *
 * `DDA_V2.ioc` exposes only one `SENSOR_ENA` GPIO. The Sensor abstraction is
 * intentionally per-sensor, but all four entries above must remain bound to
 * PC12 until the missing three board nets are added to the `.ioc` file.
 */
inline constexpr bool SensorIrLedEnablePinsAreIndependent = false;

inline constexpr uint32_t SensorDacTimeoutMilliseconds = 100U;
///< Normal blocking external-DAC write timeout.
inline constexpr GPIO_PinState SensorTriggeredState = GPIO_PIN_SET;
///< GPIO state treated as a sensor trigger.
inline constexpr uint8_t DefaultSensorTripVoltageCode = 200U;
///< Raw VTRIP code applied when default sensor levels are requested.

/**
 * @brief Same-polarity sensor edges inside this interval are treated as bounce.
 *
 * Rising and falling edges have independent histories so a legitimate short
 * sensor pulse is not discarded merely because its opposite edge follows
 * quickly. The TIM2-based check is non-blocking and wrap-safe.
 */
inline constexpr uint32_t SensorCaptureDebounceMicroseconds = 100U;
inline constexpr uint32_t SensorCaptureDebounceTicks =
    SensorCaptureDebounceMicroseconds * Tim2TicksPerMicrosecond;
///< Same-polarity debounce interval expressed in free-running TIM2 ticks.
static_assert(SensorCaptureDebounceTicks > 0U,
              "Sensor capture debounce must be enabled");

} // namespace dda::config
