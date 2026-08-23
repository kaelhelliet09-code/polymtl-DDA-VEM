// Implements exclusive ADC DMA ownership, rollback on failed starts, and
// foreground callback consumption.
#include "Platform/Stm32/Analog/AdcSampler.h"
#include "Platform/Stm32/System/InterruptGuard.h"

namespace {

constexpr uint16_t MaximumDmaSegmentLength = 65'532U;

} // namespace

namespace dda {

AdcSampler *volatile AdcSampler::_activeDmaOwner = nullptr;

AdcSampler::AdcSampler(ADC_HandleTypeDef &handle) noexcept
    : _handle(handle), _sequenceEvent(SequenceEvent::None), _buffer(nullptr),
      _valueCount(0U), _completedValueCount(0U),
      _activeSegmentLength(0U) {}

HAL_StatusTypeDef AdcSampler::calibrate() noexcept {
  bool adcIsOwned = false;
  {
    InterruptGuard interruptGuard;
    AdcSampler *const owner = _activeDmaOwner;
    adcIsOwned = (owner != nullptr) && (&owner->_handle == &_handle);
  }
  if (adcIsOwned) {
    return HAL_BUSY;
  }
  return HAL_ADCEx_Calibration_Start(&_handle);
}

HAL_StatusTypeDef AdcSampler::validateConfiguration() const noexcept {
  if (_handle.DMA_Handle == nullptr) {
    return HAL_ERROR;
  }
  DMA_HandleTypeDef &dma = *_handle.DMA_Handle;
  const bool valid =
      (dma.Instance == DMA1_Channel2) &&
      (_handle.Init.Resolution == ADC_RESOLUTION_8B) &&
      (_handle.Init.DataAlign == ADC_DATAALIGN_RIGHT) &&
      (_handle.Init.EOCSelection == ADC_EOC_SEQ_CONV) &&
      (_handle.Init.ContinuousConvMode == DISABLE) &&
      (_handle.Init.DMAContinuousRequests == ENABLE) &&
      (dma.Init.Direction == DMA_PERIPH_TO_MEMORY) &&
      (dma.Init.PeriphInc == DMA_PINC_DISABLE) &&
      (dma.Init.MemInc == DMA_MINC_ENABLE) &&
      (dma.Init.PeriphDataAlignment == DMA_PDATAALIGN_BYTE) &&
      (dma.Init.MemDataAlignment == DMA_MDATAALIGN_BYTE) &&
      (dma.Init.Mode == DMA_NORMAL);
  return valid ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef AdcSampler::startDma(uint8_t *values,
                                      uint32_t valueCount) noexcept {
  if ((values == nullptr) || (valueCount == 0U) ||
      (_handle.DMA_Handle == nullptr)) {
    return HAL_ERROR;
  }

  DMA_HandleTypeDef &dma = *_handle.DMA_Handle;
  if (HAL_DMA_GetState(&dma) != HAL_DMA_STATE_READY) {
    return HAL_BUSY;
  }

  {
    InterruptGuard interruptGuard;
    if (_activeDmaOwner != nullptr) {
      return HAL_BUSY;
    }

    /*
     * DMA1 channel 2 is the only user of its configured transfer in this
     * project. Clear terminal peripheral/channel flags and the shared channel
     * 2/3 NVIC pending bit while interrupts remain masked, before publishing
     * the next owner. A callback delayed from a stopped or failed transfer
     * then has no pending source that could be attributed to the new owner.
     */
    __HAL_DMA_CLEAR_FLAG(&dma, __HAL_DMA_GET_GI_FLAG_INDEX(&dma));
    __HAL_ADC_CLEAR_FLAG(&_handle, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);
    HAL_NVIC_ClearPendingIRQ(DMA1_Channel2_3_IRQn);

    _sequenceEvent = SequenceEvent::None;
    _buffer = values;
    _valueCount = valueCount;
    _completedValueCount = 0U;
    _activeSegmentLength = static_cast<uint16_t>(
        valueCount < MaximumDmaSegmentLength ? valueCount
                                             : MaximumDmaSegmentLength);
    _activeDmaOwner = this;
  }

  const HAL_StatusTypeDef status =
      HAL_ADC_Start_DMA(&_handle, reinterpret_cast<uint32_t *>(values),
                        _activeSegmentLength);
  if (status == HAL_OK) {
    return HAL_OK;
  }

  // A callback that ran synchronously is discarded because HAL did not accept
  // the start.
  {
    InterruptGuard interruptGuard;
    if (_activeDmaOwner == this) {
      _activeDmaOwner = nullptr;
      _sequenceEvent = SequenceEvent::None;
      _buffer = nullptr;
      _valueCount = 0U;
      _completedValueCount = 0U;
      _activeSegmentLength = 0U;
    }
  }
  return status;
}

uint32_t AdcSampler::transferredValueCount() const noexcept {
  InterruptGuard interruptGuard;
  uint32_t transferred = _completedValueCount;
  if ((_activeDmaOwner == this) && (transferred < _valueCount) &&
      (_handle.DMA_Handle != nullptr) &&
      (HAL_DMA_GetState(_handle.DMA_Handle) == HAL_DMA_STATE_BUSY)) {
    transferred += static_cast<uint32_t>(_activeSegmentLength) -
                   __HAL_DMA_GET_COUNTER(_handle.DMA_Handle);
  }
  if (transferred > _valueCount) {
    transferred = _valueCount;
  }
  return transferred;
}

AdcSampler::SequenceResult AdcSampler::takeSequenceResult() noexcept {
  SequenceEvent event = SequenceEvent::None;
  bool owned = false;

  {
    InterruptGuard interruptGuard;
    owned = _activeDmaOwner == this;
    event = _sequenceEvent;
    if (event != SequenceEvent::None) {
      _sequenceEvent = SequenceEvent::None;
    }
  }

  if (!owned) {
    return SequenceResult::Idle;
  }
  if (event != SequenceEvent::None) {
    return event == SequenceEvent::Complete ? SequenceResult::Complete
                                            : SequenceResult::Error;
  }

  DMA_HandleTypeDef &dma = *_handle.DMA_Handle;
  if ((HAL_ADC_GetError(&_handle) != HAL_ADC_ERROR_NONE) ||
      (HAL_DMA_GetError(&dma) != HAL_DMA_ERROR_NONE)) {
    return SequenceResult::Error;
  }
  return SequenceResult::Pending;
}

bool AdcSampler::ownsActiveSequence() const noexcept {
  InterruptGuard interruptGuard;
  return _activeDmaOwner == this;
}

HAL_StatusTypeDef AdcSampler::stop() noexcept {
  if (ownsActiveSequence()) {
    const HAL_StatusTypeDef status = HAL_ADC_Stop_DMA(&_handle);
    if (status != HAL_OK) {
      return status;
    }

    {
      InterruptGuard interruptGuard;
      if (_activeDmaOwner == this) {
        _activeDmaOwner = nullptr;
      }
      _sequenceEvent = SequenceEvent::None;
    }
    return HAL_OK;
  }

  bool ownedByAnotherWrapper = false;
  {
    InterruptGuard interruptGuard;
    AdcSampler *const owner = _activeDmaOwner;
    ownedByAnotherWrapper =
        (owner != nullptr) && (owner != this) && (&owner->_handle == &_handle);
  }
  if (ownedByAnotherWrapper) {
    return HAL_BUSY;
  }

  return HAL_OK;
}

void AdcSampler::onSequenceCompleteFromIsr() noexcept {
  _completedValueCount += _activeSegmentLength;
  if (_completedValueCount < _valueCount) {
    const uint32_t remaining = _valueCount - _completedValueCount;
    _activeSegmentLength = static_cast<uint16_t>(
        remaining < MaximumDmaSegmentLength ? remaining
                                             : MaximumDmaSegmentLength);
    const uint32_t peripheralAddress = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(&_handle.Instance->DR));
    const uint32_t memoryAddress = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(_buffer + _completedValueCount));
    if (HAL_DMA_Start_IT(_handle.DMA_Handle, peripheralAddress, memoryAddress,
                         _activeSegmentLength) != HAL_OK) {
      onSequenceErrorFromIsr();
    }
    return;
  }
  _sequenceEvent = SequenceEvent::Complete;
}

void AdcSampler::onSequenceErrorFromIsr() noexcept {
  _sequenceEvent = SequenceEvent::Error;
}

} // namespace dda

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  dda::AdcSampler *const owner = dda::AdcSampler::_activeDmaOwner;
  if ((owner != nullptr) && (&owner->_handle == hadc)) {
    owner->onSequenceCompleteFromIsr();
  }
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
  dda::AdcSampler *const owner = dda::AdcSampler::_activeDmaOwner;
  if ((owner != nullptr) && (&owner->_handle == hadc)) {
    owner->onSequenceErrorFromIsr();
  }
}
