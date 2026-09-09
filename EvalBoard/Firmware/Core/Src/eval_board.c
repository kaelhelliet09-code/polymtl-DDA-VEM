#include "eval_board.h"
#include "usbd_cdc_if.h"

#include <stdbool.h>

#define EVENT_QUEUE_SIZE 8U
#define EVENT_QUEUE_MASK (EVENT_QUEUE_SIZE - 1U)
#define TIM2_FREQUENCY_HZ 64000000.0F
#define SENSOR_SPACING_METERS 0.050F

_Static_assert((EVENT_QUEUE_SIZE & EVENT_QUEUE_MASK) == 0U,
               "Event queue size must be a power of two");
_Static_assert(sizeof(float) == 4U, "USB protocol requires a 32-bit float");

typedef struct {
  uint32_t first;
  uint32_t second;
} CaptureEvent;

static volatile CaptureEvent events[EVENT_QUEUE_SIZE];
static volatile uint8_t queueHead;
static volatile uint8_t queueTail;
static volatile bool acquiring;
static volatile bool firstCaptured;
static volatile bool transmitBusy;
static volatile uint8_t pendingCommand;
static uint32_t firstTimestamp;
static uint8_t transmitFrame[EVAL_EVENT_SIZE];

static void clearAcquisition(void)
{
  acquiring = false;
  firstCaptured = false;
  queueHead = 0U;
  queueTail = 0U;
}

static void writeU32(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8U);
  destination[2] = (uint8_t)(value >> 16U);
  destination[3] = (uint8_t)(value >> 24U);
}

static void writeFloat(uint8_t *destination, float value)
{
  union {
    float number;
    uint32_t bits;
  } encoded = {.number = value};
  writeU32(destination, encoded.bits);
}

void EvalBoard_Process(void)
{
  uint8_t command = 0U;
  uint32_t first;
  uint32_t second;

  if (pendingCommand != 0U) {
    __disable_irq();
    command = pendingCommand;
    pendingCommand = 0U;
    clearAcquisition();
    acquiring = (command == EVAL_CMD_START);
    __enable_irq();
    return;
  }

  if (transmitBusy || (queueTail == queueHead)) {
    return;
  }

  first = events[queueTail].first;
  second = events[queueTail].second;
  transmitFrame[0] = EVAL_EVENT_MARKER;
  writeU32(&transmitFrame[1], first);
  writeU32(&transmitFrame[5], second);
  writeFloat(&transmitFrame[9],
             (SENSOR_SPACING_METERS * TIM2_FREQUENCY_HZ) /
                 (float)(second - first));

  transmitBusy = true;
  if (CDC_Transmit_FS(transmitFrame, sizeof(transmitFrame)) == USBD_OK) {
    queueTail = (uint8_t)((queueTail + 1U) & EVENT_QUEUE_MASK);
  } else {
    transmitBusy = false;
  }
}

void EvalBoard_OnUsbReceive(const uint8_t *data, uint32_t length)
{
  uint32_t index;

  for (index = 0U; index < length; ++index) {
    if (data[index] == EVAL_CMD_START) {
      pendingCommand = EVAL_CMD_START;
    } else if (data[index] == EVAL_CMD_STOP) {
      pendingCommand = EVAL_CMD_STOP;
    }
  }
}

void EvalBoard_OnUsbConnected(void)
{
  transmitBusy = false;
}

void EvalBoard_OnUsbDisconnected(void)
{
  acquiring = false;
  firstCaptured = false;
  pendingCommand = EVAL_CMD_STOP;
  transmitBusy = false;
}

void EvalBoard_OnUsbTransmitComplete(void)
{
  transmitBusy = false;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *timer)
{
  uint32_t secondTimestamp;
  uint8_t nextHead;

  if ((timer == NULL) || (timer->Instance != TIM2) || !acquiring) {
    return;
  }

  if (timer->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
    firstTimestamp = HAL_TIM_ReadCapturedValue(timer, TIM_CHANNEL_3);
    firstCaptured = true;
    return;
  }

  if ((timer->Channel != HAL_TIM_ACTIVE_CHANNEL_4) || !firstCaptured) {
    return;
  }

  secondTimestamp = HAL_TIM_ReadCapturedValue(timer, TIM_CHANNEL_4);
  firstCaptured = false;
  if ((secondTimestamp - firstTimestamp) == 0U) {
    return;
  }

  nextHead = (uint8_t)((queueHead + 1U) & EVENT_QUEUE_MASK);
  if (nextHead == queueTail) {
    return;
  }

  events[queueHead].first = firstTimestamp;
  events[queueHead].second = secondTimestamp;
  queueHead = nextHead;
}
