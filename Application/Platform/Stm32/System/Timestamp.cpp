// Implements timestamps from the free-running 64 MHz TIM2 counter.
#include "Platform/Stm32/System/Timestamp.h"

extern "C" {
extern TIM_HandleTypeDef htim2;
}

namespace dda {

Timestamp::Timestamp(TIM_HandleTypeDef &timer) noexcept
    : _timer(timer), _started(false) {}

HAL_StatusTypeDef Timestamp::start() noexcept {
  if (_started) {
    return HAL_OK;
  }

  // Reset before each attempt; successful later calls preserve the time base.
  __HAL_TIM_SET_COUNTER(&_timer, 0U);
  const HAL_StatusTypeDef status = HAL_TIM_Base_Start(&_timer);
  _started = status == HAL_OK;
  return status;
}

Timestamp::Tick Timestamp::now() const noexcept {
  return static_cast<Tick>(__HAL_TIM_GET_COUNTER(&_timer));
}

/** @cond DOXYGEN_IGNORE_TIMESTAMP_DEFINITION */
Timestamp timestamp(htim2);
/** @endcond */

} // namespace dda
