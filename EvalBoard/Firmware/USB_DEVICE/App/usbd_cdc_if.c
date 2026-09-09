#include "usbd_cdc_if.h"

#include "eval_board.h"

static uint8_t receiveBuffer[APP_RX_DATA_SIZE];
static uint8_t lineCoding[7] = {0x00U, 0xC2U, 0x01U, 0x00U,
                                0x00U, 0x00U, 0x08U};

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t command, uint8_t *buffer, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *buffer, uint32_t *length);
static int8_t CDC_TransmitComplete_FS(uint8_t *buffer, uint32_t *length,
                                      uint8_t endpoint);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS, CDC_DeInit_FS, CDC_Control_FS, CDC_Receive_FS,
    CDC_TransmitComplete_FS};

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, receiveBuffer);
  EvalBoard_OnUsbConnected();
  return (int8_t)USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
  EvalBoard_OnUsbDisconnected();
  return (int8_t)USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t command, uint8_t *buffer, uint16_t length)
{
  uint8_t index;

  if ((command == CDC_SET_LINE_CODING) && (length == sizeof(lineCoding))) {
    for (index = 0U; index < sizeof(lineCoding); ++index) {
      lineCoding[index] = buffer[index];
    }
  } else if ((command == CDC_GET_LINE_CODING) &&
             (length == sizeof(lineCoding))) {
    for (index = 0U; index < sizeof(lineCoding); ++index) {
      buffer[index] = lineCoding[index];
    }
  }
  return (int8_t)USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *buffer, uint32_t *length)
{
  EvalBoard_OnUsbReceive(buffer, *length);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, buffer);
  return (int8_t)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}

uint8_t CDC_Transmit_FS(uint8_t *buffer, uint16_t length)
{
  USBD_CDC_HandleTypeDef *cdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

  if ((cdc == NULL) || (cdc->TxState != 0U)) {
    return (uint8_t)USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, buffer, length);
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

static int8_t CDC_TransmitComplete_FS(uint8_t *buffer, uint32_t *length,
                                      uint8_t endpoint)
{
  UNUSED(buffer);
  UNUSED(length);
  UNUSED(endpoint);
  EvalBoard_OnUsbTransmitComplete();
  return (int8_t)USBD_OK;
}
