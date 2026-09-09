#pragma once

#include "usbd_cdc.h"

#define APP_RX_DATA_SIZE 64U

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
uint8_t CDC_Transmit_FS(uint8_t *buffer, uint16_t length);
