/**
 * @file AdcSampler.h
 * @brief Exclusive asynchronous DMA access to the CubeMX ADC sequence.
 * @details Publishes a single DMA owner so stale or late HAL callbacks cannot
 * complete the wrong acquisition.
 */

#pragma once

#include <cstdint>

extern "C" {
#include "stm32g0xx_hal.h"

/** @brief Dispatch ADC DMA completion. @param hadc Completed ADC handle. */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
/** @brief Dispatch an ADC DMA error. @param hadc Failed ADC handle. */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc);
}

namespace dda {

/** @brief Provides bounded DMA access to a non-owned STM32 ADC. */
class AdcSampler {
public:
  /** @brief Foreground result of one asynchronously started DMA sequence. */
  enum class SequenceResult : uint8_t {
    Idle,     ///< This wrapper does not own a sequence.
    Pending,  ///< The owned sequence has no terminal event yet.
    Complete, ///< The owned sequence completed successfully.
    Error     ///< The owned sequence reported an ADC or DMA error.
  };

  /** @brief Bind a HAL ADC. @param handle Handle that outlives this object. */
  explicit AdcSampler(ADC_HandleTypeDef &handle) noexcept;

  /** @brief Run ADC self-calibration. @return STM32 HAL status. */
  HAL_StatusTypeDef calibrate() noexcept;

  /** @brief Validate fixed ADC/DMA settings. @return Validation HAL status. */
  HAL_StatusTypeDef validateConfiguration() const noexcept;

  /**
   * @brief Start one byte-wide DMA sequence.
   * @param values Destination buffer.
   * @param valueCount Number of byte samples to acquire.
   * @return STM32 HAL start status.
   */
  HAL_StatusTypeDef startDma(uint8_t *values, uint32_t valueCount) noexcept;

  /** @brief Return the completed byte count. @return Transferred samples. */
  uint32_t transferredValueCount() const noexcept;

  /** @brief Consume the current sequence result. @return Sequence state. */
  SequenceResult takeSequenceResult() noexcept;

  /** @brief Report active ownership. @return Whether this object owns DMA. */
  bool ownsActiveSequence() const noexcept;

  /** @brief Stop an active sequence. @return STM32 HAL stop status. */
  HAL_StatusTypeDef stop() noexcept;

private:
  enum class SequenceEvent : uint8_t { None, Complete, Error };

  friend void ::HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
  friend void ::HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc);

  void onSequenceCompleteFromIsr() noexcept;
  void onSequenceErrorFromIsr() noexcept;

  static AdcSampler *volatile _activeDmaOwner;
  ADC_HandleTypeDef &_handle;
  volatile SequenceEvent _sequenceEvent;
  uint8_t *_buffer;
  uint32_t _valueCount;
  volatile uint32_t _completedValueCount;
  volatile uint16_t _activeSegmentLength;
};

} // namespace dda
