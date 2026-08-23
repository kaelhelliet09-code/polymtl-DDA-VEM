#pragma once

#include "Drivers/Ina226.h"
#include "Platform/Stm32/Analog/AdcSampler.h"
#include "Service/Launch/LaunchData.hpp"
#include "Service/Launch/LaunchManagerRequest.h"
#include "Service/RequestManager/RequestManager.h"

#include <cstdint>

namespace dda {

class SensorController;
class CoilController;
class USBcontroller;

/**
 * @brief Coordinates one bounded launch acquisition and its USB publication.
 * @details Owns ADC current capture, periodic power sampling, sensor-event
 * capture, launch reference timestamps, and the LaunchData lifetime. Completed
 * request snapshots are attached only when RequestManager is in debug mode;
 * competition-mode LaunchData contains a zero snapshot count.
 */
class LaunchManager {
public:
  /** @brief Maximum selectable current-sampling rate. */
  static constexpr uint32_t MaximumSamplingFrequencyHz =
      MaximumLaunchSamplingFrequencyHz;
  /** @brief Automatic launch timeout in milliseconds. */
  static constexpr uint32_t RunDurationMilliseconds =
      LaunchRunDurationMilliseconds;

  /**
   * @brief Bind launch acquisition to board services and CubeMX peripherals.
   * @param requestManager Shared request router and snapshot owner.
   * @param sensors Sensor capture service.
   * @param coils Power-stage control service.
   * @param adc ADC used for bridge-current DMA capture.
   * @param powerMonitor INA226 board-power monitor.
   * @param usb USB service used to publish LaunchData.
   * @param adcSamplingTimer ADC trigger timer.
   * @param powerSamplingTimer periodic INA226 scheduling timer.
   * @param attemptTimer fixed launch-duration timer.
   */
  LaunchManager(RequestManager &requestManager, SensorController &sensors,
                CoilController &coils, ADC_HandleTypeDef &adc,
                Ina226 &powerMonitor, USBcontroller &usb,
                TIM_HandleTypeDef &adcSamplingTimer,
                TIM_HandleTypeDef &powerSamplingTimer,
                TIM_HandleTypeDef &attemptTimer) noexcept;

  /** @brief Validate and prepare acquisition timers. @return HAL status. */
  HAL_StatusTypeDef init() noexcept;
  /** @brief Advance foreground acquisition and timeout work. */
  void process() noexcept;
  /** @brief Process one request addressed to LaunchManager. @param request Request. */
  void processRequest(Request &request) noexcept;
  /** @brief Stop an active run without publishing data or notifying the host. */
  void abortForSafety() noexcept;
  /** @brief Report whether launch acquisition is active. @return Active state. */
  bool isRunActive() const noexcept;
  /** @brief Consume a deferred system-reset request. @return Whether requested. */
  bool takeSystemResetRequest() noexcept;

  /**
   * @brief Record a power-sample or run-timeout timer event from interrupt context.
   * @param timer HAL timer that raised the callback.
   */
  void handleTimerFromIsr(TIM_HandleTypeDef &timer) noexcept;

private:
  bool startRun(uint8_t runId) noexcept;
  void finishRun(LaunchStatus status, bool sendData,
                 bool notifyHost = true) noexcept;
  HAL_StatusTypeDef configureTimers() noexcept;
  HAL_StatusTypeDef startTimers() noexcept;
  void stopTimers() noexcept;
  void collectPowerSample() noexcept;
  void queueRunStatus(LaunchStatus status) noexcept;

  RequestManager &_requestManager;
  SensorController &_sensors;
  CoilController &_coils;
  AdcSampler _adc;
  Ina226 &_powerMonitor;
  USBcontroller &_usb;
  TIM_HandleTypeDef &_adcSamplingTimer;
  TIM_HandleTypeDef &_powerSamplingTimer;
  TIM_HandleTypeDef &_attemptTimer;

  LaunchData _data{};
  uint32_t _samplingFrequencyHz{MaximumSamplingFrequencyHz};
  volatile uint32_t _missedPowerSamples{0U};
  volatile bool _runActive{false};
  volatile bool _timeoutRequested{false};
  bool _systemResetRequested{false};
  bool _adcActive{false};
  uint8_t _runId{0U};
};

} // namespace dda
