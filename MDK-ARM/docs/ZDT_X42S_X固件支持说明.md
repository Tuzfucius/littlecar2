# ZDT_X42S X 固件协议支持说明

依据文件：`ZDT_X42S第二代闭环步进电机用户手册V1.0.3_251224.pdf`。

## 与 Emm 固件的关键差异

- X 固件速度模式和位置模式命令格式与 Emm 固件不同，不能混用。
- X 固件速度值以 `0.1RPM` 为单位，`1000` 表示 `100.0RPM`。
- X 固件位置角度以 `0.1°` 为单位，`3600` 表示 `360.0°`。
- X 固件读取实时转速返回值也以 `0.1RPM` 为单位。
- X 固件读取实时位置返回值以 `0.1°` 为单位。

## 新增接口

速度模式：

```c
ZDT_SetSpeedX(&huart3, 1, ZDT_DIR_CW, 1000, 1000, ZDT_SYNC_DISABLE);
```

含义：地址 `1`，CW，速度加速度 `1000RPM/S`，目标速度 `100.0RPM`。

速度模式限电流：

```c
ZDT_SetSpeedCurrentX(&huart3, 1, ZDT_DIR_CCW, 1000, 20000, ZDT_SYNC_DISABLE, 2000);
```

直通限速位置模式：

```c
ZDT_SetDirectPositionX(
    &huart3,
    1,
    ZDT_DIR_CW,
    1000,
    3600,
    ZDT_POS_RELATIVE_LAST_TARGET,
    ZDT_SYNC_DISABLE);
```

含义：最大速度 `100.0RPM`，相对上一输入目标位置运动 `360.0°`。

梯形曲线位置模式：

```c
ZDT_SetTrapezoidPositionX(
    &huart3,
    1,
    ZDT_DIR_CW,
    500,
    500,
    1000,
    3600,
    ZDT_POS_RELATIVE_LAST_TARGET,
    ZDT_SYNC_DISABLE);
```

含义：加速加速度 `500RPM/S`，减速加速度 `500RPM/S`，最大速度 `100.0RPM`，相对运动 `360.0°`。

## 解析接口

读取速度仍使用通用读取命令：

```c
ZDT_ReadSpeed(&huart3, 1);
```

收到返回后：

```c
int16_t speed_0p1rpm;
if (ZDT_ParseSpeedX(rx, len, &speed_0p1rpm))
{
  float rpm = ZDT_XSpeedToRpm(speed_0p1rpm);
}
```

读取位置：

```c
ZDT_ReadPosition(&huart3, 1);
```

收到返回后：

```c
int32_t angle_0p1deg;
if (ZDT_ParsePositionX(rx, len, &angle_0p1deg))
{
  float degree = ZDT_XPositionToDegree(angle_0p1deg);
}
```

## 注意事项

- `ZDT_SetSpeed()` 和 `ZDT_SetPosition()` 仍然是 Emm 固件接口。
- X 固件必须调用带 `X` 后缀的接口，例如 `ZDT_SetSpeedX()`。
- `ZDT_SetTrapezoidPositionX()` 与 Emm 的 `ZDT_SetPosition()` 都使用功能码 `0xFD`，但帧长度和字段含义完全不同。
- 速度参数会被限制到 `0-3000.0RPM`，电流参数会被限制到 `0-5000mA`。
