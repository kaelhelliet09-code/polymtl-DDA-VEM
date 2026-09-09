#pragma once

#include "usbd_def.h"

#define DEVICE_ID1 UID_BASE
#define DEVICE_ID2 (UID_BASE + 0x4U)
#define DEVICE_ID3 (UID_BASE + 0x8U)
#define USB_SIZ_STRING_SERIAL 0x1AU

extern USBD_DescriptorsTypeDef CDC_Desc;
