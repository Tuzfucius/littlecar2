# ZDT_X42S 协议帧说明

依据文件：`ZDT_X42S第二代闭环步进电机用户手册V1.0.3_251224.pdf`。

本文档说明当前工程使用的 ZDT_X42S 串口 TTL/RS485 自由协议帧。工程当前通过 `USART3` 控制 ZDT 电机，默认参数为 `115200, 8N1`，默认校验字节为固定 `0x6B`。

## 1. 通用帧格式

主机发送：

```text
Addr + Code + Data + CheckSum
```

电机返回：

```text
Addr + Code + ReturnData + CheckSum
```

字段含义：

| 字段 | 长度 | 含义 |
| --- | ---: | --- |
| `Addr` | 1 字节 | 电机地址，`0x01-0xFF` 为单机地址，`0x00` 为广播地址 |
| `Code` | 1 字节 | 功能码，决定执行的命令类型 |
| `Data` | 变长 | 命令参数，不同功能码含义不同 |
| `ReturnData` | 变长 | 电机返回数据或状态码 |
| `CheckSum` | 1 字节 | 当前工程默认固定为 `0x6B` |

多字节数值采用高字节在前，即大端序。例如 `1500RPM = 0x05DC`，发送为 `05 DC`。

## 2. 地址与校验

地址规则：

| 地址 | 含义 |
| --- | --- |
| `0x00` | 广播地址，适合急停、同步启动等动作命令 |
| `0x01-0xFF` | 单个电机地址 |

校验方式：

| 校验方式 | 当前工程状态 |
| --- | --- |
| 固定 `0x6B` | 已使用，默认方式 |
| XOR | 手册支持，当前代码未启用 |
| CRC-8 | 手册支持，当前代码未启用 |
| Modbus-RTU CRC-16 | 手册支持，当前代码未启用 |

## 3. 通用返回码

控制类命令通常返回：

```text
Addr + Code + Status + 6B
```

状态码：

| 状态码 | 含义 |
| --- | --- |
| `0x02` | 命令正确，已接收 |
| `0x12` | 触发回零时当前已在零点或限位已触发 |
| `0xE2` | 参数错误、条件不满足、低压、堵转、过热或过流等 |
| `0xEE` | 命令格式错误 |
| `0x9F` | 动作执行完成，由电机主动返回 |

示例：

```text
01 F6 02 6B
```

含义：地址 `1` 的电机收到 `F6` 速度命令，命令正确。

## 4. 通用控制帧

以下命令 Emm 固件和 X 固件通用。

### 4.1 电机使能控制

发送：

```text
Addr F3 AB Enable Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `Addr` | 电机地址 |
| `F3` | 使能控制功能码 |
| `AB` | 固定辅助码 |
| `Enable` | `00` 失能，`01` 使能 |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |
| `6B` | 固定校验 |

示例：

```text
01 F3 AB 01 00 6B
```

含义：使能地址 `1` 的电机，立即执行。

返回：

```text
01 F3 02 6B
```

### 4.2 立即停止

发送：

```text
Addr FE 98 Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `FE` | 立即停止功能码 |
| `98` | 固定辅助码 |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |

示例：

```text
01 FE 98 00 6B
```

含义：立即停止地址 `1` 的电机。

广播急停：

```text
00 FE 98 00 6B
```

### 4.3 多机同步启动

发送：

```text
Addr FF 66 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `FF` | 同步启动功能码 |
| `66` | 固定辅助码 |

推荐使用广播地址：

```text
00 FF 66 6B
```

含义：让所有已缓存同步命令的电机同时开始运动。

## 5. Emm 固件专用运动帧

当前代码中 `ZDT_SetSpeed()` 和 `ZDT_SetPosition()` 是 Emm 固件接口。

### 5.1 Emm 速度模式

发送：

```text
Addr F6 Dir VelH VelL Acc Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `F6` | 速度模式功能码 |
| `Dir` | `00` CW，`01` CCW |
| `VelH VelL` | 速度，单位 RPM，范围 `0-3000` |
| `Acc` | 加速度档位，范围 `0-255` |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |

示例：

```text
01 F6 01 05 DC 0A 00 6B
```

含义：地址 `1`，CCW，`1500RPM`，加速度档位 `10`，立即执行。

### 5.2 Emm 位置模式

发送：

```text
Addr FD Dir VelH VelL Acc Pulse3 Pulse2 Pulse1 Pulse0 Mode Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `FD` | 位置模式功能码 |
| `Dir` | `00` CW，`01` CCW |
| `VelH VelL` | 速度，单位 RPM，范围 `0-3000` |
| `Acc` | 加速度档位，范围 `0-255` |
| `Pulse3-Pulse0` | 脉冲数，4 字节大端 |
| `Mode` | `00` 相对上一输入目标位置，`01` 相对坐标零点绝对位置，`02` 相对当前实时位置 |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |

示例：

```text
01 FD 01 05 DC 00 00 00 7D 00 00 00 6B
```

含义：地址 `1`，CCW，`1500RPM`，加速度 `0`，相对运动 `32000` 个脉冲，立即执行。

默认 `1.8°` 电机、`16` 细分时，`3200` 个脉冲为一圈 `360°`。

## 6. X 固件专用运动帧

当前代码中带 `X` 后缀的接口是 X 固件接口，例如 `ZDT_SetSpeedX()`。

### 6.1 X 速度模式

发送：

```text
Addr F6 Dir AccH AccL SpeedH SpeedL Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `F6` | X 固件速度模式功能码 |
| `Dir` | `00` CW，`01` CCW |
| `AccH AccL` | 加速度，单位 RPM/S，范围 `0-65535` |
| `SpeedH SpeedL` | 速度，单位 `0.1RPM`，范围 `0-30000`，即 `0-3000.0RPM` |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |

示例：

```text
01 F6 01 03 E8 4E 20 00 6B
```

含义：地址 `1`，CCW，加速度 `1000RPM/S`，速度 `2000.0RPM`，立即执行。

### 6.2 X 速度模式限电流

发送：

```text
Addr C6 Dir AccH AccL SpeedH SpeedL Sync CurH CurL 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `C6` | X 固件速度模式限电流功能码 |
| `CurH CurL` | 最大电流，单位 mA，范围 `0-5000` |

示例：

```text
01 C6 01 03 E8 4E 20 00 07 D0 6B
```

含义：地址 `1`，CCW，加速度 `1000RPM/S`，速度 `2000.0RPM`，最大电流 `2000mA`。

### 6.3 X 直通限速位置模式

发送：

```text
Addr FB Dir SpeedH SpeedL Angle3 Angle2 Angle1 Angle0 Mode Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `FB` | 直通限速位置模式功能码 |
| `SpeedH SpeedL` | 最大速度，单位 `0.1RPM` |
| `Angle3-Angle0` | 位置角度，单位 `0.1°` |
| `Mode` | `00` 相对上一输入目标位置，`01` 相对坐标零点绝对位置，`02` 相对当前实时位置 |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |

示例：

```text
01 FB 00 03 E8 00 00 0E 10 02 00 6B
```

含义：地址 `1`，CW，最大速度 `100.0RPM`，相对当前位置运动 `360.0°`，立即执行。

### 6.4 X 直通限速位置模式限电流

发送：

```text
Addr CB Dir SpeedH SpeedL Angle3 Angle2 Angle1 Angle0 Mode Sync CurH CurL 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `CB` | 直通限速位置模式限电流功能码 |
| `CurH CurL` | 最大电流，单位 mA，范围 `0-5000` |

示例：

```text
01 CB 01 4E 20 00 00 8C A0 00 00 07 D0 6B
```

含义：地址 `1`，CCW，最大速度 `2000.0RPM`，相对上一输入目标位置运动 `3600.0°`，最大电流 `2000mA`。

### 6.5 X 梯形曲线位置模式

发送：

```text
Addr FD Dir AccH AccL DecH DecL SpeedH SpeedL Angle3 Angle2 Angle1 Angle0 Mode Sync 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `FD` | X 固件梯形曲线位置模式功能码 |
| `AccH AccL` | 加速加速度，单位 RPM/S |
| `DecH DecL` | 减速加速度，单位 RPM/S |
| `SpeedH SpeedL` | 最大速度，单位 `0.1RPM` |
| `Angle3-Angle0` | 位置角度，单位 `0.1°` |
| `Mode` | `00` 相对上一输入目标位置，`01` 相对坐标零点绝对位置，`02` 相对当前实时位置 |
| `Sync` | `00` 立即执行，`01` 缓存等待同步启动 |

示例：

```text
01 FD 00 01 F4 01 F4 03 E8 00 00 0E 10 02 00 6B
```

含义：地址 `1`，CW，加速/减速加速度均为 `500RPM/S`，最大速度 `100.0RPM`，相对当前位置运动 `360.0°`。

注意：X 固件和 Emm 固件的位置模式都使用功能码 `FD`，但帧长度和字段含义不同，必须按固件类型选择对应接口。

### 6.6 X 梯形曲线位置模式限电流

发送：

```text
Addr CD Dir AccH AccL DecH DecL SpeedH SpeedL Angle3 Angle2 Angle1 Angle0 Mode Sync CurH CurL 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `CD` | X 固件梯形曲线位置模式限电流功能码 |
| `CurH CurL` | 最大电流，单位 mA，范围 `0-5000` |

示例：

```text
01 CD 01 01 FF 01 FA 27 10 00 00 8C A0 00 00 07 D0 6B
```

含义：地址 `1`，CCW，加速加速度 `511RPM/S`，减速加速度 `506RPM/S`，最大速度 `1000.0RPM`，相对上一输入目标位置运动 `3600.0°`，最大电流 `2000mA`。

## 7. 读取类帧

读取类命令通常为：

```text
Addr Code 6B
```

### 7.1 读取固件和硬件版本

发送：

```text
01 1F 6B
```

返回：

```text
Addr 1F FW_VerH FW_VerL HW_Info 6B
```

固件版本和硬件信息按手册字段解析。

### 7.2 读取实时速度

发送：

```text
01 35 6B
```

返回：

```text
Addr 35 Sign SpeedH SpeedL 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `Sign` | `00` 正，`01` 负 |
| `SpeedH SpeedL` | 实时速度 |

Emm 固件中速度单位为 `RPM`。

X 固件中速度单位为 `0.1RPM`，需要除以 `10`。

示例：

```text
01 35 01 05 DC 6B
```

Emm 固件含义：`-1500RPM`。

X 固件含义：`-150.0RPM`。

### 7.3 读取实时位置

发送：

```text
01 36 6B
```

返回：

```text
Addr 36 Sign Pos3 Pos2 Pos1 Pos0 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `Sign` | `00` 正，`01` 负 |
| `Pos3-Pos0` | 实时位置 |

Emm 固件中 `0-65535` 表示一圈 `0-360°`，角度计算：

```text
degree = raw * 360 / 65536
```

X 固件中位置单位为 `0.1°`，角度计算：

```text
degree = raw / 10
```

### 7.4 读取电机状态标志

发送：

```text
01 3A 6B
```

返回：

```text
Addr 3A Status 6B
```

状态位：

| 位 | 掩码 | 含义 |
| --- | --- | --- |
| bit0 | `0x01` | 电机使能状态 |
| bit1 | `0x02` | 位置到达状态 |
| bit2 | `0x04` | 堵转状态 |
| bit3 | `0x08` | 堵转保护状态 |
| bit4 | `0x10` | 左限位输入状态 |
| bit5 | `0x20` | 右限位输入状态 |
| bit7 | `0x80` | 掉电标志 |

不同固件和配置下部分状态位含义可能受选项参数影响，调试时应结合手册和当前菜单配置确认。

## 8. 修改 ID 帧

发送：

```text
Addr AE 4B Save NewAddr 6B
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `AE` | 修改 ID 功能码 |
| `4B` | 固定辅助码 |
| `Save` | `00` 不存储，`01` 存储到参数 |
| `NewAddr` | 新地址，范围 `01-FF` |

示例：

```text
01 AE 4B 01 02 6B
```

含义：将当前地址 `1` 的电机改为地址 `2`，并保存。

注意：修改 ID 时总线上只能连接一个待修改电机，禁止使用广播地址批量修改 ID。

## 9. 当前代码接口对应关系

| 协议帧 | 代码接口 | 固件 |
| --- | --- | --- |
| `F3 AB` 使能 | `ZDT_Enable()` | 通用 |
| `FE 98` 停止 | `ZDT_Stop()` | 通用 |
| `FF 66` 同步启动 | `ZDT_SyncStart()` | 通用 |
| `F6` Emm 速度 | `ZDT_SetSpeed()` | Emm |
| `FD` Emm 位置 | `ZDT_SetPosition()` | Emm |
| `F6` X 速度 | `ZDT_SetSpeedX()` | X |
| `C6` X 限电流速度 | `ZDT_SetSpeedCurrentX()` | X |
| `FB` X 直通位置 | `ZDT_SetDirectPositionX()` | X |
| `CB` X 直通位置限电流 | `ZDT_SetDirectPositionCurrentX()` | X |
| `FD` X 梯形位置 | `ZDT_SetTrapezoidPositionX()` | X |
| `CD` X 梯形位置限电流 | `ZDT_SetTrapezoidPositionCurrentX()` | X |
| `1F` 读取版本 | `ZDT_ReadVersion()` | 通用 |
| `35` 读取速度 | `ZDT_ReadSpeed()` | 通用 |
| `36` 读取位置 | `ZDT_ReadPosition()` | 通用 |
| `3A` 读取状态 | `ZDT_ReadStatus()` | 通用 |
| `AE 4B` 修改 ID | `ZDT_SetID()` | 通用 |

## 10. 使用建议

- 调试前先确认电机当前固件类型。X42S 小屏幕或指示灯状态可用于判断固件类型。
- Emm 固件只调用不带 `X` 后缀的运动接口。
- X 固件只调用带 `X` 后缀的运动接口。
- 广播地址适合急停和同步启动，不适合读取参数或修改 ID。
- 先单电机低速测试，再接入多电机同步控制。

## 11. 底盘运动层说明

当前工程已经在协议层之上新增 `chassis_motion` 模块：

- `Core/Inc/chassis_motion.h`
- `Core/Src/chassis_motion.c`
- `docs/底盘运动控制说明.md`

该模块不新增底层协议帧，而是调用 `Emm_V5` 的多电机命令接口，将麦克纳姆轮车体动作转换为四个电机的速度命令。

主要接口包括：

```c
Chassis_Forward(rpm);
Chassis_Backward(rpm);
Chassis_StrafeLeft(rpm);
Chassis_StrafeRight(rpm);
Chassis_RotateLeft(rpm);
Chassis_RotateRight(rpm);
Chassis_DifferentialTurn(left_rpm, right_rpm);
Chassis_MoveMecanum(forward_rpm, strafe_rpm, rotate_rpm);
```

每类运动还提供 `Ex` 接口用于指定加速度，提供 `Preset` 接口用于使用头文件内的平滑移动预设参数。
