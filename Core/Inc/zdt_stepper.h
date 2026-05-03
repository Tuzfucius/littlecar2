#ifndef __ZDT_STEPPER_H__
#define __ZDT_STEPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define ZDT_CHECKSUM_FIXED ((uint8_t)0x6B)
#define DATOU_CHECKSUM ZDT_CHECKSUM_FIXED
#define ZDT_BROADCAST_ADDR ((uint8_t)0x00)
#define ZDT_MAX_EMM_RPM ((uint16_t)3000U)
#define ZDT_MAX_X_SPEED_0P1RPM ((uint16_t)30000U)
#define ZDT_MAX_X_CURRENT_MA ((uint16_t)5000U)

typedef enum
{
  ZDT_DIR_CW = 0x00,
  ZDT_DIR_CCW = 0x01
} ZDT_Dir;

typedef enum
{
  ZDT_SYNC_DISABLE = 0x00,
  ZDT_SYNC_ENABLE = 0x01
} ZDT_Sync;

typedef enum
{
  ZDT_POS_RELATIVE_LAST_TARGET = 0x00,
  ZDT_POS_ABSOLUTE_ZERO = 0x01,
  ZDT_POS_RELATIVE_CURRENT = 0x02
} ZDT_PosMode;

typedef enum
{
  ZDT_ACK_OK = 0x02,
  ZDT_ACK_HOME_ALREADY = 0x12,
  ZDT_ACK_PARAM_ERROR = 0xE2,
  ZDT_ACK_FORMAT_ERROR = 0xEE,
  ZDT_ACK_ACTION_DONE = 0x9F
} ZDT_AckStatus;

void Datou_En_Control(UART_HandleTypeDef *huart, uint8_t addr, bool state, bool snF);
void Datou_Vel_Control(UART_HandleTypeDef *huart, uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF);
void Datou_Pos_Control(
    UART_HandleTypeDef *huart, uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t pulse, ZDT_PosMode mode, bool snF);
void Datou_Stop_Now(UART_HandleTypeDef *huart, uint8_t addr, bool snF);
void Datou_Synchronous_motion(UART_HandleTypeDef *huart, uint8_t addr);
void Datou_setpos(UART_HandleTypeDef *huart, uint8_t addr, int32_t pos);

void ZDT_Enable(UART_HandleTypeDef *huart, uint8_t addr, bool enable, ZDT_Sync sync);
void ZDT_SetSpeed(UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t rpm, uint8_t acc, ZDT_Sync sync);
void ZDT_SetPosition(
    UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t rpm, uint8_t acc, uint32_t pulse, ZDT_PosMode mode, ZDT_Sync sync);
void ZDT_Stop(UART_HandleTypeDef *huart, uint8_t addr, ZDT_Sync sync);
void ZDT_SyncStart(UART_HandleTypeDef *huart, uint8_t addr);
void ZDT_ReadVersion(UART_HandleTypeDef *huart, uint8_t addr);
void ZDT_ReadSpeed(UART_HandleTypeDef *huart, uint8_t addr);
void ZDT_ReadPosition(UART_HandleTypeDef *huart, uint8_t addr);
void ZDT_ReadStatus(UART_HandleTypeDef *huart, uint8_t addr);
void ZDT_SetID(UART_HandleTypeDef *huart, uint8_t old_addr, uint8_t new_addr, bool save);
bool ZDT_IsAckOK(const uint8_t *rx, uint16_t len, uint8_t expected_addr, uint8_t expected_func);
bool ZDT_ParseSpeedEmm(const uint8_t *rx, uint16_t len, int16_t *rpm);
bool ZDT_ParsePositionEmm(const uint8_t *rx, uint16_t len, int32_t *raw_position);
float ZDT_EmmRawPositionToDegree(int32_t raw_position);

void ZDT_SetSpeedX(UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t acc_rpm_s, uint16_t speed_0p1rpm, ZDT_Sync sync);
void ZDT_SetSpeedCurrentX(
    UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t acc_rpm_s, uint16_t speed_0p1rpm, ZDT_Sync sync, uint16_t max_current_ma);
void ZDT_SetDirectPositionX(
    UART_HandleTypeDef *huart, uint8_t addr, ZDT_Dir dir, uint16_t speed_0p1rpm, uint32_t angle_0p1deg, ZDT_PosMode mode, ZDT_Sync sync);
void ZDT_SetDirectPositionCurrentX(
    UART_HandleTypeDef *huart,
    uint8_t addr,
    ZDT_Dir dir,
    uint16_t speed_0p1rpm,
    uint32_t angle_0p1deg,
    ZDT_PosMode mode,
    ZDT_Sync sync,
    uint16_t max_current_ma);
void ZDT_SetTrapezoidPositionX(
    UART_HandleTypeDef *huart,
    uint8_t addr,
    ZDT_Dir dir,
    uint16_t acc_rpm_s,
    uint16_t dec_rpm_s,
    uint16_t max_speed_0p1rpm,
    uint32_t angle_0p1deg,
    ZDT_PosMode mode,
    ZDT_Sync sync);
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
    uint16_t max_current_ma);
bool ZDT_ParseSpeedX(const uint8_t *rx, uint16_t len, int16_t *speed_0p1rpm);
bool ZDT_ParsePositionX(const uint8_t *rx, uint16_t len, int32_t *angle_0p1deg);
float ZDT_XSpeedToRpm(int16_t speed_0p1rpm);
float ZDT_XPositionToDegree(int32_t angle_0p1deg);

#ifdef __cplusplus
}
#endif

#endif

/*
 * Emm 固件基础测试示例：
 *   ZDT_Enable(&huart3, 1, true, ZDT_SYNC_DISABLE);
 *   HAL_Delay(100);
 *   ZDT_SetSpeed(&huart3, 1, ZDT_DIR_CW, 100, 10, ZDT_SYNC_DISABLE);
 *   HAL_Delay(2000);
 *   ZDT_Stop(&huart3, 1, ZDT_SYNC_DISABLE);
 */
