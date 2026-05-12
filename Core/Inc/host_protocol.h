#ifndef __HOST_PROTOCOL_H__
#define __HOST_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum
{
  HOST_PROTOCOL_SOURCE_PC = 0,
  HOST_PROTOCOL_SOURCE_JETSON,
  HOST_PROTOCOL_SOURCE_COUNT
} HostProtocol_Source_t;

typedef enum
{
  HOST_PROTOCOL_STATUS_OK = 0,
  HOST_PROTOCOL_STATUS_INVALID_PARAM,
  HOST_PROTOCOL_STATUS_OVERFLOW
} HostProtocol_Status_t;

/* 绑定某一路接收源的串口句柄，用于协议 ACK 回发。 */
void HostProtocol_RegisterSource(HostProtocol_Source_t source, UART_HandleTypeDef *huart);

/* 向协议解析器输入原始字节流，可在 DMA/IDLE 回调中调用。 */
void HostProtocol_OnBytes(HostProtocol_Source_t source, const uint8_t *data, uint16_t length);

/* 在主循环中轮询，负责执行已解析出的命令。 */
void HostProtocol_Poll(void);

HostProtocol_Status_t HostProtocol_GetLastStatus(void);

#ifdef __cplusplus
}
#endif

#endif
