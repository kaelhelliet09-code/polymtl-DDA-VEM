#include "usbd_desc.h"

#include "usbd_core.h"

#define USBD_VID 0x0483U
#define USBD_PID 0x5741U
#define USBD_LANGID_STRING 0x0409U
#define USBD_MANUFACTURER_STRING "DDA"
#define USBD_PRODUCT_STRING "DDA Velocity Eval Board"
#define USBD_CONFIGURATION_STRING "CDC Config"
#define USBD_INTERFACE_STRING "CDC Interface"

static uint8_t *deviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *languageDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *manufacturerDescriptor(USBD_SpeedTypeDef speed,
                                       uint16_t *length);
static uint8_t *productDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *serialDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *configurationDescriptor(USBD_SpeedTypeDef speed,
                                        uint16_t *length);
static uint8_t *interfaceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *stringDescriptor(const char *text, uint16_t *length);
static void makeSerialNumber(void);
static void integerToUnicode(uint32_t value, uint8_t *buffer, uint8_t length);

USBD_DescriptorsTypeDef CDC_Desc = {
    deviceDescriptor,       languageDescriptor, manufacturerDescriptor,
    productDescriptor,      serialDescriptor,   configurationDescriptor,
    interfaceDescriptor};

__ALIGN_BEGIN static uint8_t deviceDescription[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12U,
    USB_DESC_TYPE_DEVICE,
    0x00U,
    0x02U,
    0x02U,
    0x02U,
    0x00U,
    USB_MAX_EP0_SIZE,
    LOBYTE(USBD_VID),
    HIBYTE(USBD_VID),
    LOBYTE(USBD_PID),
    HIBYTE(USBD_PID),
    0x00U,
    0x02U,
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION};

__ALIGN_BEGIN static uint8_t languageDescription[USB_LEN_LANGID_STR_DESC]
    __ALIGN_END = {USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING,
                   LOBYTE(USBD_LANGID_STRING), HIBYTE(USBD_LANGID_STRING)};

__ALIGN_BEGIN static uint8_t stringDescription[USBD_MAX_STR_DESC_SIZ]
    __ALIGN_END;
__ALIGN_BEGIN static uint8_t serialDescription[USB_SIZ_STRING_SERIAL]
    __ALIGN_END = {USB_SIZ_STRING_SERIAL, USB_DESC_TYPE_STRING};

static uint8_t *deviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(deviceDescription);
  return deviceDescription;
}

static uint8_t *languageDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(languageDescription);
  return languageDescription;
}

static uint8_t *stringDescriptor(const char *text, uint16_t *length)
{
  USBD_GetString((uint8_t *)text, stringDescription, length);
  return stringDescription;
}

static uint8_t *manufacturerDescriptor(USBD_SpeedTypeDef speed,
                                       uint16_t *length)
{
  UNUSED(speed);
  return stringDescriptor(USBD_MANUFACTURER_STRING, length);
}

static uint8_t *productDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  return stringDescriptor(USBD_PRODUCT_STRING, length);
}

static uint8_t *serialDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(serialDescription);
  makeSerialNumber();
  return serialDescription;
}

static uint8_t *configurationDescriptor(USBD_SpeedTypeDef speed,
                                        uint16_t *length)
{
  UNUSED(speed);
  return stringDescriptor(USBD_CONFIGURATION_STRING, length);
}

static uint8_t *interfaceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  return stringDescriptor(USBD_INTERFACE_STRING, length);
}

static void makeSerialNumber(void)
{
  uint32_t first = *(uint32_t *)DEVICE_ID1 + *(uint32_t *)DEVICE_ID3;
  uint32_t second = *(uint32_t *)DEVICE_ID2;

  if (first != 0U) {
    integerToUnicode(first, &serialDescription[2], 8U);
    integerToUnicode(second, &serialDescription[18], 4U);
  }
}

static void integerToUnicode(uint32_t value, uint8_t *buffer, uint8_t length)
{
  uint8_t index;

  for (index = 0U; index < length; ++index) {
    uint8_t digit = (uint8_t)(value >> 28U);
    buffer[index * 2U] = (digit < 10U) ? (uint8_t)('0' + digit)
                                       : (uint8_t)('A' + digit - 10U);
    buffer[index * 2U + 1U] = 0U;
    value <<= 4U;
  }
}
