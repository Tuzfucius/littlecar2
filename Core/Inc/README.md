# Inc 目录说明

本目录存放 `Core` 层头文件。

- `main.h`：主程序公共声明
- `stm32f4xx_hal_conf.h`：HAL 组件配置
- `stm32f4xx_it.h`：中断入口声明
- `zdt_stepper.h`：张大头步进电机协议接口
- `bus_servo.h`：总线舵机设备层接口
- `wit_imu.h`：WIT 陀螺仪协议层接口
- `ops_sensor.h`：OPS 定位系统协议层接口
- `chassis_motion.h`：基于 Emm_V5 的麦克纳姆轮底盘高层运动接口，包含电机 ID、方向修正和各动作平滑预设参数
