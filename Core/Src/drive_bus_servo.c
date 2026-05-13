#include "drive_bus_servo.h"

#include <string.h>

#define drive_bus_servo_FRAME_HEADER             ((uint8_t)0xFF)
#define drive_bus_servo_FRAME_WRITE_INSTRUCTION  ((uint8_t)0x03)
#define drive_bus_servo_FRAME_START_ADDRESS      ((uint8_t)0x29)
#define drive_bus_servo_FRAME_DATA_LENGTH        ((uint8_t)0x0A)
#define drive_bus_servo_FRAME_TOTAL_LENGTH       ((uint16_t)14U)
#define drive_bus_servo_UART_TIMEOUT_MS          ((uint32_t)100U)

static UART_HandleTypeDef *g_drive_bus_servo_uart = NULL;
static volatile uint8_t g_drive_bus_servo_rx_byte = 0U;
static volatile uint32_t g_drive_bus_servo_rx_count = 0U;
static BusServo_Status_t g_drive_bus_servo_last_status = drive_bus_servo_STATUS_NOT_READY;
static uint8_t g_drive_bus_servo_last_frame[drive_bus_servo_FRAME_TOTAL_LENGTH] = {0};

/**
  * @brief  判断舵机 ID 是否落在当前协议允许的地址范围内�?  * @param  id: 待检查舵�?ID�?  * @retval `1` 表示有效，`0` 表示无效�?  */
static uint8_t BusServo_IsValidId(uint8_t id)
{
  return (id > drive_bus_servo_MIN_ID) && (id < drive_bus_servo_MAX_ID);
}

/**
  * @brief  判断位置参数是否落在当前设备层允许的编码范围内�?  * @param  position: 待检查位置值�?  * @retval `1` 表示有效，`0` 表示无效�?  */
static uint8_t BusServo_IsValidPosition(int32_t position)
{
  return (position > drive_bus_servo_MIN_POSITION) && (position < drive_bus_servo_MAX_POSITION);
}

/**
  * @brief  按旧项目的规则将有符号位置值编码成协议中的 16 位字段�?  * @param  position: 上层传入的位置参数�?  * @retval 协议帧中使用的编码值�?  */
static uint16_t BusServo_EncodePosition(int32_t position)
{
  if (position < 0)
  {
    return (uint16_t)(32768 - position);
  }

  return (uint16_t)position;
}

/**
  * @brief  对舵机控制帧的校验区间求和后按位取反，得到最终校验字节�?  * @param  frame: 完整待发送帧缓冲区�?  * @retval 计算得到的校验字节�?  */
static uint8_t BusServo_CalcChecksum(const uint8_t *frame)
{
  uint8_t sum = 0U;
  uint16_t index = 0U;

  for (index = 2U; index < (drive_bus_servo_FRAME_TOTAL_LENGTH - 1U); ++index)
  {
    sum = (uint8_t)(sum + frame[index]);
  }

  return (uint8_t)(~sum);
}

/**
  * @brief  根据旧项�?`Servo_SendPositionAccelSpeedCommand()` 的格式组装一帧位置命令�?  * @param  command: 结构化命令输入�?  * @param  frame: 输出帧缓冲区，长度固定为 14 字节�?  * @retval `drive_bus_servo_STATUS_OK` 表示组帧成功�?  */
static BusServo_Status_t BusServo_BuildPositionFrame(const BusServo_Command_t *command,
                                                     uint8_t frame[drive_bus_servo_FRAME_TOTAL_LENGTH])
{
  uint16_t encoded_position = 0U;

  if ((command == NULL) || (frame == NULL))
  {
    return drive_bus_servo_STATUS_INVALID_PARAM;
  }

  if (!BusServo_IsValidId(command->id) ||
      !BusServo_IsValidPosition(command->position))
  {
    return drive_bus_servo_STATUS_INVALID_PARAM;
  }

  encoded_position = BusServo_EncodePosition(command->position);
  memset(frame, 0, drive_bus_servo_FRAME_TOTAL_LENGTH);

  frame[0] = drive_bus_servo_FRAME_HEADER;
  frame[1] = drive_bus_servo_FRAME_HEADER;
  frame[2] = command->id;
  frame[3] = drive_bus_servo_FRAME_DATA_LENGTH;
  frame[4] = drive_bus_servo_FRAME_WRITE_INSTRUCTION;
  frame[5] = drive_bus_servo_FRAME_START_ADDRESS;
  frame[6] = (uint8_t)(command->acceleration & 0x00FFU);
  frame[7] = (uint8_t)(encoded_position & 0x00FFU);
  frame[8] = (uint8_t)((encoded_position >> 8) & 0x00FFU);
  frame[9] = 0x00U;
  frame[10] = 0x00U;
  frame[11] = (uint8_t)(command->speed & 0x00FFU);
  frame[12] = (uint8_t)((command->speed >> 8) & 0x00FFU);
  frame[13] = BusServo_CalcChecksum(frame);

  return drive_bus_servo_STATUS_OK;
}

/**
  * @brief  通过当前绑定串口发送一帧舵机命令�?  * @param  frame: 已经组装完成的完整协议帧�?  * @retval 发送结果状态�?  */
static BusServo_Status_t BusServo_TransmitFrame(const uint8_t *frame)
{
  if ((g_drive_bus_servo_uart == NULL) || (frame == NULL))
  {
    return drive_bus_servo_STATUS_NOT_READY;
  }

  if (HAL_UART_Transmit(g_drive_bus_servo_uart, (uint8_t *)frame, drive_bus_servo_FRAME_TOTAL_LENGTH, drive_bus_servo_UART_TIMEOUT_MS) != HAL_OK)
  {
    return drive_bus_servo_STATUS_TX_ERROR;
  }

  memcpy(g_drive_bus_servo_last_frame, frame, drive_bus_servo_FRAME_TOTAL_LENGTH);
  return drive_bus_servo_STATUS_OK;
}

BusServo_Status_t BusServo_Init(UART_HandleTypeDef *huart)
{
  if (huart == NULL)
  {
    g_drive_bus_servo_last_status = drive_bus_servo_STATUS_INVALID_PARAM;
    return g_drive_bus_servo_last_status;
  }

  g_drive_bus_servo_uart = huart;
  g_drive_bus_servo_rx_byte = 0U;
  g_drive_bus_servo_rx_count = 0U;
  memset(g_drive_bus_servo_last_frame, 0, sizeof(g_drive_bus_servo_last_frame));

  g_drive_bus_servo_last_status = BusServo_StartReceive();
  return g_drive_bus_servo_last_status;
}

BusServo_Status_t BusServo_StartReceive(void)
{
  if (g_drive_bus_servo_uart == NULL)
  {
    g_drive_bus_servo_last_status = drive_bus_servo_STATUS_NOT_READY;
    return g_drive_bus_servo_last_status;
  }

  if (HAL_UART_Receive_IT(g_drive_bus_servo_uart, (uint8_t *)&g_drive_bus_servo_rx_byte, 1U) != HAL_OK)
  {
    g_drive_bus_servo_last_status = drive_bus_servo_STATUS_RX_ERROR;
    return g_drive_bus_servo_last_status;
  }

  g_drive_bus_servo_last_status = drive_bus_servo_STATUS_OK;
  return g_drive_bus_servo_last_status;
}

void BusServo_OnByteReceived(void)
{
  ++g_drive_bus_servo_rx_count;
  (void)BusServo_StartReceive();
}

void BusServo_OnUartError(void)
{
  g_drive_bus_servo_last_status = drive_bus_servo_STATUS_RX_ERROR;
  (void)BusServo_StartReceive();
}

void BusServo_Poll(void)
{
}

BusServo_Status_t BusServo_SetPosition(uint8_t id, int32_t position)
{
  return BusServo_SetPositionEx(id, drive_bus_servo_DEFAULT_ACCELERATION, position, drive_bus_servo_DEFAULT_SPEED);
}

BusServo_Status_t BusServo_SetPositionEx(uint8_t id, uint16_t acceleration, int32_t position, uint16_t speed)
{
  BusServo_Command_t command = {0};

  command.id = id;
  command.acceleration = acceleration;
  command.position = position;
  command.speed = speed;

  return BusServo_SendCommand(&command);
}

BusServo_Status_t BusServo_SendCommand(const BusServo_Command_t *command)
{
  uint8_t frame[drive_bus_servo_FRAME_TOTAL_LENGTH] = {0};
  BusServo_Status_t status = drive_bus_servo_STATUS_OK;

  status = BusServo_BuildPositionFrame(command, frame);
  if (status != drive_bus_servo_STATUS_OK)
  {
    g_drive_bus_servo_last_status = status;
    return status;
  }

  status = BusServo_TransmitFrame(frame);
  g_drive_bus_servo_last_status = status;
  return status;
}

BusServo_Status_t BusServo_SendGroup(const BusServo_Command_t *commands, uint8_t count)
{
  BusServo_Status_t status = drive_bus_servo_STATUS_OK;
  uint8_t index = 0U;

  if ((commands == NULL) || (count == 0U))
  {
    g_drive_bus_servo_last_status = drive_bus_servo_STATUS_INVALID_PARAM;
    return g_drive_bus_servo_last_status;
  }

  for (index = 0U; index < count; ++index)
  {
    status = BusServo_SendCommand(&commands[index]);
    if (status != drive_bus_servo_STATUS_OK)
    {
      return status;
    }
  }

  return drive_bus_servo_STATUS_OK;
}

BusServo_Status_t BusServo_GetLastStatus(void)
{
  return g_drive_bus_servo_last_status;
}
