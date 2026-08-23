/**
 * @file AdcSampler.h
 * @brief Declares asynchronous DMA and stop helpers for an STM32 ADC.
 * @details Provides exclusive DMA ownership so synchronous, stale, or late
 * HAL callbacks cannot complete the wrong acquisition.
 */

#pragma once

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"

/**
 * @brief Dispatches an ADC DMA completion to the wrapper that owns the
 * transfer.
 * @param hadc HAL ADC handle that completed conversion.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);

/**
 * @brief Dispatches an ADC DMA error to the wrapper that owns the transfer.
 * @param hadc HAL ADC handle that reported the error.
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc);
}

namespace dda {

/**
 * @brief Provides bounded DMA access to a non-owned STM32 ADC.
 * @note The ADC sequence, resolution, alignment, and conversion trigger are
 * configured externally by CubeMX.
 */
class AdcSampler {
public:
  /** @brief Foreground result of one asynchronously started DMA sequence. */
  enum class SequenceResult : uint8_t {
    Idle,     ///< This wrapper does not own a DMA sequence.
    Pending,  ///< The owned sequence has not produced a terminal event.
    Complete, ///< The owned sequence completed successfully.
    Error,    ///< The owned sequence reported an ADC or DMA error.
  };

  /**
   * @brief Binds the wrapper to a HAL ADC handle.
   * @param handle HAL ADC handle; must outlive this object.
   */
  explicit AdcSampler(ADC_HandleTypeDef &handle) noexcept;

  /**
   * @brief Runs the STM32 ADC self-calibration routine.
   * @return STM32 HAL calibration status.
   * @note No conversion may be active while calibration runs.
   */
  HAL_StatusTypeDef calibrate() noexcept;

  /**
   * @brief Validates the fixed CubeMX ADC and DMA configuration.
   * @return `HAL_OK` when the configured scan is safe for the DMA buffer, or
   * `HAL_ERROR` otherwise.
   * @note Call once during application initialization, before calibration or
   * starting a sequence.
   */
  HAL_StatusTypeDef validateConfiguration() const noexcept;

  HAL_StatusTypeDef startDma(uint8_t *values, uint32_t valueCount) noexcept;

  uint32_t transferredValueCount() const noexcept;

  /**
   * @brief Consumes the terminal event for the currently owned DMA sequence.
   * @return Current sequence state. Complete and Error are returned once.
   * @note Events are cleared whenever ownership is published or released.
   */
  SequenceResult takeSequenceResult() noexcept;

  /**
   * @brief Reports whether this wrapper owns the active ADC DMA sequence.
   * @return `true` between a successful start and successful stop.
   */
  bool ownsActiveSequence() const noexcept;

  /**
   * @brief Stops an active regular conversion using the matching HAL path.
   * @return `HAL_OK` if already idle, otherwise the HAL DMA-stop status.
   */
  HAL_StatusTypeDef stop() noexcept;

private:
  /** @brief ISR event associated with the currently owned transfer. */
  enum class SequenceEvent : uint8_t {
    None,
    Complete,
    Error,
  };

  friend void ::HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
  friend void ::HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc);

  /** @brief Records a completion for the currently published owner. */
  void onSequenceCompleteFromIsr() noexcept;

  /** @brief Records an error for the currently published owner. */
  void onSequenceErrorFromIsr() noexcept;

  /** @brief Process-wide owner used only by the HAL ADC callback bridge. */
  static AdcSampler *volatile _activeDmaOwner;

  /** @brief Non-owning reference to the CubeMX HAL ADC handle. */
  ADC_HandleTypeDef &_handle;

  /** @brief Terminal event published by the HAL callback bridge. */
  volatile SequenceEvent _sequenceEvent;
  uint8_t *_buffer;
  uint32_t _valueCount;
  volatile uint32_t _completedValueCount;
  volatile uint16_t _activeSegmentLength;
};

} // namespace dda
