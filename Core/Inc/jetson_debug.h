#ifndef __JETSON_DEBUG_H__
#define __JETSON_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
  JETSON_DEBUG_STATUS_OK = 0,
  JETSON_DEBUG_STATUS_INVALID_PARAM,
  JETSON_DEBUG_STATUS_NOT_READY,
  JETSON_DEBUG_STATUS_RX_ERROR,
  JETSON_DEBUG_STATUS_OVERFLOW
} JetsonDebug_Status_t;

JetsonDebug_Status_t JetsonDebug_Init(UART_HandleTypeDef *huart);
void JetsonDebug_Poll(void);
void JetsonDebug_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t size);
void JetsonDebug_OnUartError(UART_HandleTypeDef *huart);
JetsonDebug_Status_t JetsonDebug_GetLastStatus(void);

#ifdef __cplusplus
}
#endif

#endif
