下面这份可以直接保存为：

```text
docs/ops_uart_protocol.md
```

# OPS 全方位平面定位系统通信协议说明

## 1. 协议概述

OPS 通过 **RS-232 串口**输出定位数据，并支持通过串口发送校准、清零、坐标更新等命令。手册中明确该模块使用 **5V 供电**，接口包含 `VCC`、`GND`、`RS-232 TxD`、`RS-232 RxD`；通信参数为 `115200bps, 8N1`。

> 注意：OPS 是 **RS-232 电平**，不是普通 TTL 串口。
> STM32 或 Jetson 不能直接用 3.3V UART 引脚硬接 OPS 的 RS-232 TX/RX，必须经过 RS-232 转 TTL 模块或使用开发板上的 RS-232 接口。

---

## 2. 串口通信参数

| 参数      | 数值            |
| ------- | ------------- |
| 通信方式    | UART / RS-232 |
| 波特率     | `115200 bps`  |
| 数据位     | `8 bit`       |
| 起始位     | `1 bit`       |
| 停止位     | `1 bit`       |
| 校验位     | 无             |
| 数据帧率    | `200 fps`     |
| 初始化静止时间 | 上电后约 `15 s`   |

串口配置可写为：

```text
115200, 8N1
```

---

## 3. 数据帧格式

OPS 周期性输出固定长度数据帧。

手册说明每个数据包共 **28 字节**，其中包头 2 字节、数据区 24 字节、包尾 2 字节；数据区由 6 个单精度浮点数构成，每个 float 占 4 字节，且低字节优先。

### 3.1 总体帧结构

```text
帧头        数据区                         帧尾
0D 0A       24 bytes                       0A 0D
2 bytes     6 个 float，每个 4 bytes        2 bytes
```

### 3.2 字节布局

| 偏移 | 长度 | 字段        | 类型      | 字节序           | 说明            |
| -: | -: | --------- | ------- | ------------- | ------------- |
|  0 |  1 | Header[0] | uint8_t | -             | 固定 `0x0D`     |
|  1 |  1 | Header[1] | uint8_t | -             | 固定 `0x0A`     |
|  2 |  4 | zangle    | float   | Little-Endian | Z 轴角度/航向角     |
|  6 |  4 | xangle    | float   | Little-Endian | X 轴角度          |
| 10 |  4 | yangle    | float   | Little-Endian | Y 轴角度          |
| 14 |  4 | pos_x     | float   | Little-Endian | X 坐标，单位通常为 mm |
| 18 |  4 | pos_y     | float   | Little-Endian | Y 坐标，单位通常为 mm |
| 22 |  4 | w_z       | float   | Little-Endian | Z 轴角速度         |
| 26 |  1 | Tail[0]   | uint8_t | -             | 固定 `0x0A`     |
| 27 |  1 | Tail[1]   | uint8_t | -             | 固定 `0x0D`     |

### 3.3 数据区顺序

```text
zangle     xangle      yangle      pos_x       pos_y       w_z
4 bytes    4 bytes     4 bytes     4 bytes     4 bytes     4 bytes
```

也就是：

```text
[0D 0A]
[zangle float]
[xangle float]
[yangle float]
[pos_x float]
[pos_y float]
[w_z float]
[0A 0D]
```

---

## 4. 浮点数格式

数据区使用 IEEE 754 单精度浮点数，也就是 C 语言中的 `float`。

每个 float 占 4 字节：

```text
float = 4 bytes
```

OPS 使用低字节优先：

```text
Little-Endian
```

示例：

```c
typedef struct
{
    float zangle;
    float xangle;
    float yangle;
    float pos_x;
    float pos_y;
    float w_z;
} OPS_Data_t;
```

解析时不能把数据当 ASCII 字符串处理，应直接按二进制 float 解析。
