#pragma once

#include "stm32g0xx_hal.h"
#include <stdint.h>

#define EVAL_CMD_START 0x01U
#define EVAL_CMD_STOP 0x02U
#define EVAL_EVENT_MARKER 0x56U
#define EVAL_EVENT_SIZE 13U

void EvalBoard_Process(void);
void EvalBoard_OnUsbReceive(const uint8_t *data, uint32_t length);
void EvalBoard_OnUsbConnected(void);
void EvalBoard_OnUsbDisconnected(void);
void EvalBoard_OnUsbTransmitComplete(void);
