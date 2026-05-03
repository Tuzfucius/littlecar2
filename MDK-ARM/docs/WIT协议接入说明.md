# WIT 协议接入说明

## 1. 本轮实现范围

本轮在 `USART2` 上接入 HWT905/WIT 标准串口协议的基础解析层，代码位于：

- `Core/Inc/wit_imu.h`
- `Core/Src/wit_imu.c`

当前实现范围如下：

- 接收并解析三类 11 字节实时输出帧
  - `0x51`：加速度
  - `0x52`：角速度
  - `0x53`：姿态角
- 维护三类三轴缓存数据
  - `accel_g`
  - `gyro_dps`
  - `angle_deg`
- 每类缓存都带有：
  - `valid`
  - `updated_tick`
- 对外统一通过 `WIT_GetData()` 暴露结构体，供其他模块读取

本轮未实现：

- `0x50` 时间
- `0x54` 磁场
- `0x56` 气压高度
- `0x57` 经纬度
- `0x59` 四元数
- 串口写配置命令封装

## 2. 接收链路

接收链路复用当前工程已有的 `USART2 + DMA + UART 空闲中断` 资源。

- `WIT_Init()` 调用 `HAL_UARTEx_ReceiveToIdle_DMA(&huart2, ...)`
- 在 `main.c` 的统一 `HAL_UARTEx_RxEventCallback()` 中分发到 `WIT_OnUartRxEvent()`
- 在 `main.c` 的统一 `HAL_UART_ErrorCallback()` 中分发到 `WIT_OnUartError()`
- 在主循环调用 `WIT_Poll()`，超时后将对应缓存标记为无效

## 3. 帧格式

WIT 标准实时输出帧固定为 11 字节：

```text
Byte0   0x55
Byte1   类型
Byte2   Data1_L
Byte3   Data1_H
Byte4   Data2_L
Byte5   Data2_H
Byte6   Data3_L
Byte7   Data3_H
Byte8   Data4_L
Byte9   Data4_H
Byte10  CheckSum
```

当前实现仅消费 `Byte1` 为 `0x51`、`0x52`、`0x53` 的帧。

校验规则：

- 将 `Byte0` 到 `Byte9` 累加
- 取低 8 位
- 与 `Byte10` 比较

## 4. 数值换算

三轴数据按小端有符号 16 位整数解析。

### 4.1 加速度 `0x51`

```text
ax = raw_ax / 32768 * 16
ay = raw_ay / 32768 * 16
az = raw_az / 32768 * 16
```

单位为 `g`。

### 4.2 角速度 `0x52`

```text
wx = raw_wx / 32768 * 2000
wy = raw_wy / 32768 * 2000
wz = raw_wz / 32768 * 2000
```

单位为 `deg/s`。

### 4.3 姿态角 `0x53`

```text
roll  = raw_roll  / 32768 * 180
pitch = raw_pitch / 32768 * 180
yaw   = raw_yaw   / 32768 * 180
```

单位为 `deg`。

注意：

- `0x51` 和 `0x52` 的 `Byte8/Byte9` 可按手册解释为温度原始值
- `0x53` 的 `Byte8/Byte9` 是版本号，不是温度
- 当前代码不对温度和版本号做缓存

## 5. 解析策略

协议层按字节流滑窗解析，不假设每次 DMA 回调都刚好对齐 11 字节帧。

- 先拼接上一次回调残留字节
- 从字节流中搜索帧头 `0x55`
- 当剩余长度不足 11 字节时，保留残留字节等待下一次回调
- 校验通过后按帧类型分发到对应缓存
- 校验失败时仅跳过当前字节，继续向后搜索下一帧头

## 6. 对外接口

当前开放的接口如下：

- `WIT_Init()`
- `WIT_Poll()`
- `WIT_OnUartRxEvent()`
- `WIT_OnUartError()`
- `WIT_GetData()`

`WIT_Data_t` 对外字段如下：

```c
typedef struct
{
  WIT_Vector3f_t accel_g;
  WIT_Vector3f_t gyro_dps;
  WIT_Vector3f_t angle_deg;
} WIT_Data_t;
```

## 7. 当前边界

本轮实现目标是把 WIT 设备稳定接入当前下位机通信框架，重点在：

- 串口接收
- 帧解析
- 三类数据缓存
- 对外结构体读取

本轮不负责：

- 磁场、气压、四元数等扩展帧
- 写寄存器配置设备
- 姿态控制算法
- 与 Jetson 的联动协议
