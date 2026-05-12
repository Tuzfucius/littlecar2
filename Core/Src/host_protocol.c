#include "host_protocol.h"
#include "chassis_motion.h"
#include <stdbool.h>
#include <string.h>

#define HOST_PROTOCOL_HEADER1          ((uint8_t)0x5AU)
#define HOST_PROTOCOL_HEADER2          ((uint8_t)0xA5U)
#define HOST_PROTOCOL_VERSION          ((uint8_t)0x01U)
#define HOST_PROTOCOL_HEADER_LEN       ((uint16_t)9U)
#define HOST_PROTOCOL_CRC_LEN          ((uint16_t)2U)
#define HOST_PROTOCOL_MAX_PAYLOAD      ((uint16_t)255U)
#define HOST_PROTOCOL_MAX_FRAME_LEN    (HOST_PROTOCOL_HEADER_LEN + HOST_PROTOCOL_MAX_PAYLOAD + HOST_PROTOCOL_CRC_LEN)
#define HOST_PROTOCOL_QUEUE_SIZE       ((uint8_t)4U)
#define HOST_PROTOCOL_TX_TIMEOUT_MS    ((uint32_t)20U)
#define HOST_PROTOCOL_HEARTBEAT_MS     ((uint32_t)300U)

typedef enum
{
  MSG_CMD = 0x01,
  MSG_ACK = 0x02
} HostProtocol_MsgType_t;

typedef enum
{
  CMDSET_SYSTEM = 0x01,
  CMDSET_SAFETY = 0x02,
  CMDSET_CHASSIS = 0x03
} HostProtocol_CmdSet_t;

typedef enum
{
  ACK_OK = 0x00,
  ACK_BAD_CRC = 0x01,
  ACK_BAD_LENGTH = 0x02,
  ACK_BAD_PARAM = 0x03,
  ACK_BUSY = 0x04,
  ACK_DENIED = 0x05,
  ACK_UNKNOWN_CMD = 0x06,
  ACK_FAULT = 0x07
} HostProtocol_AckResult_t;

typedef struct
{
  uint8_t frame[HOST_PROTOCOL_MAX_FRAME_LEN];
  uint16_t pos;
  uint16_t expected_len;
} HostProtocol_Parser_t;

typedef struct
{
  HostProtocol_Source_t source;
  uint8_t msg_type;
  uint8_t cmd_set;
  uint8_t cmd_id;
  uint16_t seq;
  uint8_t payload_len;
  uint8_t payload[HOST_PROTOCOL_MAX_PAYLOAD];
} HostProtocol_Frame_t;

static UART_HandleTypeDef *g_host_protocol_uart[HOST_PROTOCOL_SOURCE_COUNT] = {0};
static HostProtocol_Parser_t g_host_protocol_parser[HOST_PROTOCOL_SOURCE_COUNT] = {0};
static volatile uint8_t g_queue_head = 0U;
static volatile uint8_t g_queue_tail = 0U;
static volatile uint8_t g_queue_count = 0U;
static HostProtocol_Frame_t g_queue[HOST_PROTOCOL_QUEUE_SIZE] = {0};
static HostProtocol_Status_t g_last_status = HOST_PROTOCOL_STATUS_OK;
static uint32_t g_last_heartbeat_tick = 0U;
static uint8_t g_heartbeat_seen = 0U;
static uint8_t g_heartbeat_online = 0U;
static uint8_t g_safety_latched = 0U;
static uint8_t g_control_mode = 0U;

static uint16_t HostProtocol_ReadU16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static int16_t HostProtocol_ReadI16(const uint8_t *data)
{
  return (int16_t)HostProtocol_ReadU16(data);
}

static uint32_t HostProtocol_ReadU32(const uint8_t *data)
{
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

static void HostProtocol_WriteU16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static uint16_t HostProtocol_Crc16Modbus(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;
  uint16_t index;
  uint8_t bit;

  for (index = 0U; index < length; ++index)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; ++bit)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc = (uint16_t)((crc >> 1) ^ 0xA001U);
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

static void HostProtocol_ResetParser(HostProtocol_Parser_t *parser)
{
  parser->pos = 0U;
  parser->expected_len = 0U;
}

static uint8_t HostProtocol_AbsRpmTooLarge(int16_t rpm)
{
  int32_t value = rpm;

  if (value < 0)
  {
    value = -value;
  }

  return (value > (int32_t)CHASSIS_MAX_RPM) ? 1U : 0U;
}

static uint8_t HostProtocol_IsControlAllowed(void)
{
  if (g_safety_latched != 0U)
  {
    return 0U;
  }

  if ((g_heartbeat_seen != 0U) && (g_heartbeat_online == 0U))
  {
    return 0U;
  }

  return 1U;
}

static void HostProtocol_CheckHeartbeatTimeout(void)
{
  if ((g_heartbeat_seen != 0U) &&
      (g_heartbeat_online != 0U) &&
      ((HAL_GetTick() - g_last_heartbeat_tick) > HOST_PROTOCOL_HEARTBEAT_MS))
  {
    g_heartbeat_online = 0U;
    Chassis_Stop();
  }
}

static uint8_t HostProtocol_EnqueueFrame(HostProtocol_Source_t source, const uint8_t *frame, uint16_t frame_len)
{
  HostProtocol_Frame_t *item;
  uint8_t payload_len;

  if ((source >= HOST_PROTOCOL_SOURCE_COUNT) || (frame_len < (HOST_PROTOCOL_HEADER_LEN + HOST_PROTOCOL_CRC_LEN)))
  {
    g_last_status = HOST_PROTOCOL_STATUS_INVALID_PARAM;
    return 0U;
  }

  if (g_queue_count >= HOST_PROTOCOL_QUEUE_SIZE)
  {
    g_last_status = HOST_PROTOCOL_STATUS_OVERFLOW;
    return 0U;
  }

  payload_len = frame[8];
  item = &g_queue[g_queue_head];
  item->source = source;
  item->msg_type = frame[3];
  item->cmd_set = frame[4];
  item->cmd_id = frame[5];
  item->seq = HostProtocol_ReadU16(&frame[6]);
  item->payload_len = payload_len;
  if (payload_len > 0U)
  {
    memcpy(item->payload, &frame[9], payload_len);
  }

  g_queue_head = (uint8_t)((g_queue_head + 1U) % HOST_PROTOCOL_QUEUE_SIZE);
  ++g_queue_count;
  g_last_status = HOST_PROTOCOL_STATUS_OK;
  return 1U;
}

/* 主循环取命令时短暂关中断，避免和接收回调同时改队列索引。 */
static uint8_t HostProtocol_DequeueFrame(HostProtocol_Frame_t *frame)
{
  if (frame == NULL)
  {
    return 0U;
  }

  __disable_irq();
  if (g_queue_count == 0U)
  {
    __enable_irq();
    return 0U;
  }

  memcpy(frame, &g_queue[g_queue_tail], sizeof(HostProtocol_Frame_t));
  g_queue_tail = (uint8_t)((g_queue_tail + 1U) % HOST_PROTOCOL_QUEUE_SIZE);
  --g_queue_count;
  __enable_irq();
  return 1U;
}

/* ACK 使用原命令 Seq，方便上位机按请求序号匹配结果。 */
static void HostProtocol_SendAck(const HostProtocol_Frame_t *frame, HostProtocol_AckResult_t result, uint8_t detail)
{
  UART_HandleTypeDef *huart;
  uint8_t tx[HOST_PROTOCOL_HEADER_LEN + 4U + HOST_PROTOCOL_CRC_LEN];
  uint16_t crc;

  if ((frame == NULL) || (frame->source >= HOST_PROTOCOL_SOURCE_COUNT))
  {
    return;
  }

  huart = g_host_protocol_uart[frame->source];
  if (huart == NULL)
  {
    return;
  }

  tx[0] = HOST_PROTOCOL_HEADER1;
  tx[1] = HOST_PROTOCOL_HEADER2;
  tx[2] = HOST_PROTOCOL_VERSION;
  tx[3] = MSG_ACK;
  tx[4] = frame->cmd_set;
  tx[5] = frame->cmd_id;
  HostProtocol_WriteU16(&tx[6], frame->seq);
  tx[8] = 4U;
  HostProtocol_WriteU16(&tx[9], frame->seq);
  tx[11] = (uint8_t)result;
  tx[12] = detail;
  crc = HostProtocol_Crc16Modbus(&tx[2], 7U + tx[8]);
  HostProtocol_WriteU16(&tx[13], crc);

  (void)HAL_UART_Transmit(huart, tx, (uint16_t)sizeof(tx), HOST_PROTOCOL_TX_TIMEOUT_MS);
}

/* 系统命令只维护通信状态，不直接控制外设。 */
static HostProtocol_AckResult_t HostProtocol_HandleSystem(const HostProtocol_Frame_t *frame)
{
  switch (frame->cmd_id)
  {
    case 0x01U:
      return (frame->payload_len == 0U) ? ACK_OK : ACK_BAD_LENGTH;

    case 0x02U:
      if (frame->payload_len != 4U)
      {
        return ACK_BAD_LENGTH;
      }
      (void)HostProtocol_ReadU32(frame->payload);
      g_last_heartbeat_tick = HAL_GetTick();
      g_heartbeat_seen = 1U;
      g_heartbeat_online = 1U;
      return ACK_OK;

    case 0x03U:
      if (frame->payload_len != 1U)
      {
        return ACK_BAD_LENGTH;
      }
      g_control_mode = frame->payload[0];
      return ACK_OK;

    default:
      return ACK_UNKNOWN_CMD;
  }
}

/* 安全命令允许打断普通底盘运动。 */
static HostProtocol_AckResult_t HostProtocol_HandleSafety(const HostProtocol_Frame_t *frame)
{
  switch (frame->cmd_id)
  {
    case 0x01U:
      if (frame->payload_len != 1U)
      {
        return ACK_BAD_LENGTH;
      }
      (void)frame->payload[0];
      g_safety_latched = 1U;
      Chassis_Stop();
      return ACK_OK;

    case 0x02U:
      if (frame->payload_len != 1U)
      {
        return ACK_BAD_LENGTH;
      }
      if (frame->payload[0] == 0U)
      {
        Chassis_SmoothStop(CHASSIS_DEFAULT_ACC);
      }
      else if (frame->payload[0] == 1U)
      {
        Chassis_Stop();
      }
      else
      {
        return ACK_BAD_PARAM;
      }
      return ACK_OK;

    case 0x03U:
      if (frame->payload_len != 0U)
      {
        return ACK_BAD_LENGTH;
      }
      g_safety_latched = 0U;
      return ACK_OK;

    default:
      return ACK_UNKNOWN_CMD;
  }
}

/* 底盘命令只调用 chassis_motion 高层接口，不暴露底层 Emm_V5 帧。 */
static HostProtocol_AckResult_t HostProtocol_HandleChassis(const HostProtocol_Frame_t *frame)
{
  int16_t lf_rpm;
  int16_t rf_rpm;
  int16_t lr_rpm;
  int16_t rr_rpm;
  int16_t forward_rpm;
  int16_t strafe_rpm;
  int16_t rotate_rpm;

  if (HostProtocol_IsControlAllowed() == 0U)
  {
    return ACK_DENIED;
  }

  switch (frame->cmd_id)
  {
    case 0x01U:
      if (frame->payload_len != 1U)
      {
        return ACK_BAD_LENGTH;
      }
      if (frame->payload[0] > 1U)
      {
        return ACK_BAD_PARAM;
      }
      Chassis_Enable(frame->payload[0] != 0U);
      return ACK_OK;

    case 0x02U:
      if (frame->payload_len != 1U)
      {
        return ACK_BAD_LENGTH;
      }
      if (frame->payload[0] == 0U)
      {
        Chassis_SmoothStop(CHASSIS_DEFAULT_ACC);
      }
      else if (frame->payload[0] == 1U)
      {
        Chassis_Stop();
      }
      else
      {
        return ACK_BAD_PARAM;
      }
      return ACK_OK;

    case 0x03U:
      if (frame->payload_len != 9U)
      {
        return ACK_BAD_LENGTH;
      }
      lf_rpm = HostProtocol_ReadI16(&frame->payload[0]);
      rf_rpm = HostProtocol_ReadI16(&frame->payload[2]);
      lr_rpm = HostProtocol_ReadI16(&frame->payload[4]);
      rr_rpm = HostProtocol_ReadI16(&frame->payload[6]);
      if ((HostProtocol_AbsRpmTooLarge(lf_rpm) != 0U) ||
          (HostProtocol_AbsRpmTooLarge(rf_rpm) != 0U) ||
          (HostProtocol_AbsRpmTooLarge(lr_rpm) != 0U) ||
          (HostProtocol_AbsRpmTooLarge(rr_rpm) != 0U))
      {
        return ACK_BAD_PARAM;
      }
      Chassis_SetMotorRPMEx(lf_rpm, rf_rpm, lr_rpm, rr_rpm, frame->payload[8]);
      return ACK_OK;

    case 0x04U:
      if (frame->payload_len != 7U)
      {
        return ACK_BAD_LENGTH;
      }
      forward_rpm = HostProtocol_ReadI16(&frame->payload[0]);
      strafe_rpm = HostProtocol_ReadI16(&frame->payload[2]);
      rotate_rpm = HostProtocol_ReadI16(&frame->payload[4]);
      if ((HostProtocol_AbsRpmTooLarge(forward_rpm) != 0U) ||
          (HostProtocol_AbsRpmTooLarge(strafe_rpm) != 0U) ||
          (HostProtocol_AbsRpmTooLarge(rotate_rpm) != 0U))
      {
        return ACK_BAD_PARAM;
      }
      Chassis_MoveMecanumEx(forward_rpm, strafe_rpm, rotate_rpm, frame->payload[6]);
      return ACK_OK;

    default:
      return ACK_UNKNOWN_CMD;
  }
}

static HostProtocol_AckResult_t HostProtocol_HandleCommand(const HostProtocol_Frame_t *frame)
{
  if (frame->msg_type != MSG_CMD)
  {
    return ACK_UNKNOWN_CMD;
  }

  switch (frame->cmd_set)
  {
    case CMDSET_SYSTEM:
      return HostProtocol_HandleSystem(frame);

    case CMDSET_SAFETY:
      return HostProtocol_HandleSafety(frame);

    case CMDSET_CHASSIS:
      return HostProtocol_HandleChassis(frame);

    default:
      return ACK_UNKNOWN_CMD;
  }
}

static void HostProtocol_FeedByte(HostProtocol_Source_t source, uint8_t byte)
{
  HostProtocol_Parser_t *parser = &g_host_protocol_parser[source];
  uint16_t crc_calc;
  uint16_t crc_recv;

  if (parser->pos == 0U)
  {
    if (byte == HOST_PROTOCOL_HEADER1)
    {
      parser->frame[0] = byte;
      parser->pos = 1U;
    }
    return;
  }

  if (parser->pos == 1U)
  {
    if (byte == HOST_PROTOCOL_HEADER2)
    {
      parser->frame[1] = byte;
      parser->pos = 2U;
    }
    else
    {
      HostProtocol_ResetParser(parser);
      if (byte == HOST_PROTOCOL_HEADER1)
      {
        parser->frame[0] = byte;
        parser->pos = 1U;
      }
    }
    return;
  }

  if (parser->pos >= HOST_PROTOCOL_MAX_FRAME_LEN)
  {
    HostProtocol_ResetParser(parser);
    return;
  }

  parser->frame[parser->pos++] = byte;

  if (parser->pos == HOST_PROTOCOL_HEADER_LEN)
  {
    if (parser->frame[2] != HOST_PROTOCOL_VERSION)
    {
      HostProtocol_ResetParser(parser);
      return;
    }
    parser->expected_len = (uint16_t)(HOST_PROTOCOL_HEADER_LEN + parser->frame[8] + HOST_PROTOCOL_CRC_LEN);
  }

  if ((parser->expected_len > 0U) && (parser->pos >= parser->expected_len))
  {
    crc_recv = HostProtocol_ReadU16(&parser->frame[parser->expected_len - HOST_PROTOCOL_CRC_LEN]);
    crc_calc = HostProtocol_Crc16Modbus(&parser->frame[2], (uint16_t)(7U + parser->frame[8]));
    if (crc_recv == crc_calc)
    {
      (void)HostProtocol_EnqueueFrame(source, parser->frame, parser->expected_len);
    }
    HostProtocol_ResetParser(parser);
  }
}

void HostProtocol_RegisterSource(HostProtocol_Source_t source, UART_HandleTypeDef *huart)
{
  if (source >= HOST_PROTOCOL_SOURCE_COUNT)
  {
    g_last_status = HOST_PROTOCOL_STATUS_INVALID_PARAM;
    return;
  }

  g_host_protocol_uart[source] = huart;
  HostProtocol_ResetParser(&g_host_protocol_parser[source]);
  g_last_status = HOST_PROTOCOL_STATUS_OK;
}

void HostProtocol_OnBytes(HostProtocol_Source_t source, const uint8_t *data, uint16_t length)
{
  uint16_t index;

  if ((source >= HOST_PROTOCOL_SOURCE_COUNT) || (data == NULL))
  {
    g_last_status = HOST_PROTOCOL_STATUS_INVALID_PARAM;
    return;
  }

  for (index = 0U; index < length; ++index)
  {
    HostProtocol_FeedByte(source, data[index]);
  }
}

void HostProtocol_Poll(void)
{
  HostProtocol_Frame_t frame;
  HostProtocol_AckResult_t result;

  HostProtocol_CheckHeartbeatTimeout();

  while (HostProtocol_DequeueFrame(&frame) != 0U)
  {
    result = HostProtocol_HandleCommand(&frame);
    HostProtocol_SendAck(&frame, result, 0U);
  }

  (void)g_control_mode;
}

HostProtocol_Status_t HostProtocol_GetLastStatus(void)
{
  return g_last_status;
}
