#include "zdt_stepper.h"

#define DATOU_UART_TIMEOUT_MS ((uint32_t)100)

static void Datou_SendBytes(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
  if ((huart == NULL) || (data == NULL) || (len == 0U))
  {
    return;
  }

  (void)HAL_UART_Transmit(huart, data, len, DATOU_UART_TIMEOUT_MS);
}

static void ZDT_PutU16BE(uint8_t *buf, uint16_t value)
{
  buf[0] = (uint8_t)(value >> 8);
  buf[1] = (uint8_t)(value & 0xFFU);
}

static void ZDT_PutU32BE(uint8_t *buf, uint32_t value)
{
  buf[0] = (uint8_t)(value >> 24);
  buf[1] = (uint8_t)(value >> 16);
  buf[2] = (uint8_t)(value >> 8);
  buf[3] = (uint8_t)(value & 0xFFU);
}

static uint16_t ZDT_ClampEmmRpm(uint16_t rpm)
{
  return (rpm > ZDT_MAX_EMM_RPM) ? ZDT_MAX_EMM_RPM : rpm;
}

static uint16_t ZDT_ClampXSpeed(uint16_t speed_0p1rpm)
{
  return (speed_0p1rpm > ZDT_MAX_X_SPEED_0P1RPM) ? ZDT_MAX_X_SPEED_0P1RPM : speed_0p1rpm;
}

static uint16_t ZDT_ClampXCurrent(uint16_t current_ma)
{
  return (current_ma > ZDT_MAX_X_CURRENT_MA) ? ZDT_MAX_X_CURRENT_MA : current_ma;
}

void Datou_En_Control(UART_HandleTypeDef *huart, uint8_t addr, bool state, bool snF)
{
  ZDT_Enable(huart, addr, state, snF ? ZDT_SYNC_ENABLE : ZDT_SYNC_DISABLE);
}

void Datou_Vel_Control(UART_HandleTypeDef *huart, uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF)
{
  ZDT_SetSpeed(huart, addr, (ZDT_Dir)dir, vel, acc, snF ? ZDT_SYNC_ENABLE : ZDT_SYNC_DISABLE);
}

void Datou_Pos_Control(
    UART_HandleTypeDef *huart,
    uint8_t addr,
    uint8_t dir,
    uint16_t vel,
    uint8_t acc,
    uint32_t pulse,
    ZDT_PosMode mode,
    bool snF
) {
    ZDT_SetPosition(huart, addr, (ZDT_Dir)dir, vel, acc, pulse, mode, snF ? ZDT_SYNC_ENABLE : ZDT_SYNC_DISABLE);
}

void Datou_Stop_Now(UART_HandleTypeDef *huart, uint8_t addr, bool snF)
{
  ZDT_Stop(huart, addr, snF ? ZDT_SYNC_ENABLE : ZDT_SYNC_DISABLE);
}

void Datou_Synchronous_motion(UART_HandleTypeDef *huart, uint8_t addr)
{
  ZDT_SyncStart(huart, addr);
}

void Datou_setpos(UART_HandleTypeDef *huart, uint8_t addr, int32_t pos)
{
  Datou_Pos_Control(huart, addr, 0U, 0U, 0U, (uint32_t)pos, ZDT_POS_ABSOLUTE_ZERO, false);
}

void ZDT_Enable(UART_HandleTypeDef *huart, uint8_t addr, bool enable, ZDT_Sync sync)
{
  uint8_t cmd[6] = {0};

  cmd[0] = addr;
  cmd[1] = 0xF3;
  cmd[2] = 0xAB;
  cmd[3] = enable ? 0x01U : 0x00U;
  cmd[4] = (uint8_t)sync;
  cmd[5] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetSpeed(UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t rpm, uint8_t acc, ZDT_Sync sync)
{
  uint8_t cmd[8] = {0};
  uint16_t safe_rpm = ZDT_ClampEmmRpm(rpm);

  cmd[0] = addr;
  cmd[1] = 0xF6;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], safe_rpm);
  cmd[5] = acc;
  cmd[6] = (uint8_t)sync;
  cmd[7] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetPosition(
    UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t rpm, uint8_t acc, uint32_t pulse, ZDT_PosMode mode, ZDT_Sync sync)
{
  uint8_t cmd[13] = {0};
  uint16_t safe_rpm = ZDT_ClampEmmRpm(rpm);

  cmd[0] = addr;
  cmd[1] = 0xFD;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], safe_rpm);
  cmd[5] = acc;
  ZDT_PutU32BE(&cmd[6], pulse);
  cmd[10] = (uint8_t)mode;
  cmd[11] = (uint8_t)sync;
  cmd[12] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_Stop(UART_HandleTypeDef *huart, uint8_t addr, ZDT_Sync sync)
{
  uint8_t cmd[5] = {0};

  cmd[0] = addr;
  cmd[1] = 0xFE;
  cmd[2] = 0x98;
  cmd[3] = (uint8_t)sync;
  cmd[4] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SyncStart(UART_HandleTypeDef *huart, uint8_t addr)
{
  uint8_t cmd[4] = {0};

  cmd[0] = addr;
  cmd[1] = 0xFF;
  cmd[2] = 0x66;
  cmd[3] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_ReadVersion(UART_HandleTypeDef *huart, uint8_t addr)
{
  uint8_t cmd[3] = {addr, 0x1F, ZDT_CHECKSUM_FIXED};
  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_ReadSpeed(UART_HandleTypeDef *huart, uint8_t addr)
{
  uint8_t cmd[3] = {addr, 0x35, ZDT_CHECKSUM_FIXED};
  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_ReadPosition(UART_HandleTypeDef *huart, uint8_t addr)
{
  uint8_t cmd[3] = {addr, 0x36, ZDT_CHECKSUM_FIXED};
  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_ReadStatus(UART_HandleTypeDef *huart, uint8_t addr)
{
  uint8_t cmd[3] = {addr, 0x3A, ZDT_CHECKSUM_FIXED};
  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetID(UART_HandleTypeDef *huart, uint8_t old_addr, uint8_t new_addr, bool save)
{
  uint8_t cmd[6] = {0};

  cmd[0] = old_addr;
  cmd[1] = 0xAE;
  cmd[2] = 0x4B;
  cmd[3] = save ? 0x01U : 0x00U;
  cmd[4] = new_addr;
  cmd[5] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

bool ZDT_IsAckOK(const uint8_t *rx, uint16_t len, uint8_t expected_addr, uint8_t expected_func)
{
  if ((rx == NULL) || (len < 4U))
  {
    return false;
  }

  if ((rx[0] != expected_addr) || (rx[1] != expected_func) || (rx[len - 1U] != ZDT_CHECKSUM_FIXED))
  {
    return false;
  }

  return rx[2] == ZDT_ACK_OK;
}

bool ZDT_ParseSpeedEmm(const uint8_t *rx, uint16_t len, int16_t *rpm)
{
  uint16_t raw;

  if ((rx == NULL) || (rpm == NULL) || (len < 6U))
  {
    return false;
  }

  if ((rx[1] != 0x35U) || (rx[5] != ZDT_CHECKSUM_FIXED))
  {
    return false;
  }

  raw = ((uint16_t)rx[3] << 8) | rx[4];
  *rpm = (rx[2] == 0x01U) ? -(int16_t)raw : (int16_t)raw;
  return true;
}

bool ZDT_ParsePositionEmm(const uint8_t *rx, uint16_t len, int32_t *raw_position)
{
  uint32_t raw;

  if ((rx == NULL) || (raw_position == NULL) || (len < 8U))
  {
    return false;
  }

  if ((rx[1] != 0x36U) || (rx[7] != ZDT_CHECKSUM_FIXED))
  {
    return false;
  }

  raw = ((uint32_t)rx[3] << 24) | ((uint32_t)rx[4] << 16) | ((uint32_t)rx[5] << 8) | rx[6];
  *raw_position = (rx[2] == 0x01U) ? -(int32_t)raw : (int32_t)raw;
  return true;
}

float ZDT_EmmRawPositionToDegree(int32_t raw_position)
{
  return ((float)raw_position * 360.0f) / 65536.0f;
}

void ZDT_SetSpeedX(UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t acc_rpm_s, uint16_t speed_0p1rpm, ZDT_Sync sync)
{
  uint8_t cmd[9] = {0};
  uint16_t safe_speed = ZDT_ClampXSpeed(speed_0p1rpm);

  cmd[0] = addr;
  cmd[1] = 0xF6;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], acc_rpm_s);
  ZDT_PutU16BE(&cmd[5], safe_speed);
  cmd[7] = (uint8_t)sync;
  cmd[8] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetSpeedCurrentX(
    UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t acc_rpm_s, uint16_t speed_0p1rpm, ZDT_Sync sync, uint16_t max_current_ma)
{
  uint8_t cmd[11] = {0};
  uint16_t safe_speed = ZDT_ClampXSpeed(speed_0p1rpm);
  uint16_t safe_current = ZDT_ClampXCurrent(max_current_ma);

  cmd[0] = addr;
  cmd[1] = 0xC6;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], acc_rpm_s);
  ZDT_PutU16BE(&cmd[5], safe_speed);
  cmd[7] = (uint8_t)sync;
  ZDT_PutU16BE(&cmd[8], safe_current);
  cmd[10] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetDirectPositionX(
    UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t speed_0p1rpm, uint32_t angle_0p1deg, ZDT_PosMode mode, ZDT_Sync sync)
{
  uint8_t cmd[12] = {0};
  uint16_t safe_speed = ZDT_ClampXSpeed(speed_0p1rpm);

  cmd[0] = addr;
  cmd[1] = 0xFB;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], safe_speed);
  ZDT_PutU32BE(&cmd[5], angle_0p1deg);
  cmd[9] = (uint8_t)mode;
  cmd[10] = (uint8_t)sync;
  cmd[11] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetDirectPositionCurrentX(
    UART_HandleTypeDef *huart,
    uint8_t addr,
    ZDT_Dir dir,
    uint16_t speed_0p1rpm,
    uint32_t angle_0p1deg,
    ZDT_PosMode mode,
    ZDT_Sync sync,
    uint16_t max_current_ma)
{
  uint8_t cmd[14] = {0};
  uint16_t safe_speed = ZDT_ClampXSpeed(speed_0p1rpm);
  uint16_t safe_current = ZDT_ClampXCurrent(max_current_ma);

  cmd[0] = addr;
  cmd[1] = 0xCB;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], safe_speed);
  ZDT_PutU32BE(&cmd[5], angle_0p1deg);
  cmd[9] = (uint8_t)mode;
  cmd[10] = (uint8_t)sync;
  ZDT_PutU16BE(&cmd[11], safe_current);
  cmd[13] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetTrapezoidPositionX(
    UART_HandleTypeDef *huart,
    uint8_t addr,
    ZDT_Dir dir,
    uint16_t acc_rpm_s,
    uint16_t dec_rpm_s,
    uint16_t max_speed_0p1rpm,
    uint32_t angle_0p1deg,
    ZDT_PosMode mode,
    ZDT_Sync sync)
{
  uint8_t cmd[16] = {0};
  uint16_t safe_speed = ZDT_ClampXSpeed(max_speed_0p1rpm);

  cmd[0] = addr;
  cmd[1] = 0xFD;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], acc_rpm_s);
  ZDT_PutU16BE(&cmd[5], dec_rpm_s);
  ZDT_PutU16BE(&cmd[7], safe_speed);
  ZDT_PutU32BE(&cmd[9], angle_0p1deg);
  cmd[13] = (uint8_t)mode;
  cmd[14] = (uint8_t)sync;
  cmd[15] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

void ZDT_SetTrapezoidPositionCurrentX(
    UART_HandleTypeDef *huart,
    uint8_t addr,
    ZDT_Dir dir,
    uint16_t acc_rpm_s,
    uint16_t dec_rpm_s,
    uint16_t max_speed_0p1rpm,
    uint32_t angle_0p1deg,
    ZDT_PosMode mode,
    ZDT_Sync sync,
    uint16_t max_current_ma)
{
  uint8_t cmd[18] = {0};
  uint16_t safe_speed = ZDT_ClampXSpeed(max_speed_0p1rpm);
  uint16_t safe_current = ZDT_ClampXCurrent(max_current_ma);

  cmd[0] = addr;
  cmd[1] = 0xCD;
  cmd[2] = (uint8_t)dir;
  ZDT_PutU16BE(&cmd[3], acc_rpm_s);
  ZDT_PutU16BE(&cmd[5], dec_rpm_s);
  ZDT_PutU16BE(&cmd[7], safe_speed);
  ZDT_PutU32BE(&cmd[9], angle_0p1deg);
  cmd[13] = (uint8_t)mode;
  cmd[14] = (uint8_t)sync;
  ZDT_PutU16BE(&cmd[15], safe_current);
  cmd[17] = ZDT_CHECKSUM_FIXED;

  Datou_SendBytes(huart, cmd, sizeof(cmd));
}

bool ZDT_ParseSpeedX(const uint8_t *rx, uint16_t len, int16_t *speed_0p1rpm)
{
  uint16_t raw;

  if ((rx == NULL) || (speed_0p1rpm == NULL) || (len < 6U))
  {
    return false;
  }

  if ((rx[1] != 0x35U) || (rx[5] != ZDT_CHECKSUM_FIXED))
  {
    return false;
  }

  raw = ((uint16_t)rx[3] << 8) | rx[4];
  *speed_0p1rpm = (rx[2] == 0x01U) ? -(int16_t)raw : (int16_t)raw;
  return true;
}

bool ZDT_ParsePositionX(const uint8_t *rx, uint16_t len, int32_t *angle_0p1deg)
{
  uint32_t raw;

  if ((rx == NULL) || (angle_0p1deg == NULL) || (len < 8U))
  {
    return false;
  }

  if ((rx[1] != 0x36U) || (rx[7] != ZDT_CHECKSUM_FIXED))
  {
    return false;
  }

  raw = ((uint32_t)rx[3] << 24) | ((uint32_t)rx[4] << 16) | ((uint32_t)rx[5] << 8) | rx[6];
  *angle_0p1deg = (rx[2] == 0x01U) ? -(int32_t)raw : (int32_t)raw;
  return true;
}

float ZDT_XSpeedToRpm(int16_t speed_0p1rpm)
{
  return ((float)speed_0p1rpm) / 10.0f;
}

float ZDT_XPositionToDegree(int32_t angle_0p1deg)
{
  return ((float)angle_0p1deg) / 10.0f;
}
