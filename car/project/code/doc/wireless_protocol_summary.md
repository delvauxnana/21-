# 车端无线串口协议总结

本文档按当前车端实现整理无线遥控/遥测协议，重点对应以下代码：

- `car/project/code/inc/protocol.h`
- `car/project/code/src/protocol.c`
- `car/project/code/inc/cmd_dispatcher.h`
- `car/project/code/src/cmd_dispatcher.c`
- `car/project/code/inc/motion_manager.h`
- `car/project/user/src/main.c`
- `car/project/user/src/isr.c`

已有参考说明见 `car/project/protocol_spec.md`。本文以当前代码实际行为为准。

## 1. 链路概览

- 物理链路：逐飞无线串口模块 `wireless_uart`
- 接收解析端：车端 RT1064
- 串口中断入口：`LPUART8_IRQHandler()`
- 主循环处理：`protocol_poll()`
- 失控保护节拍：`protocol_tick()`，由 5 ms PIT 节拍驱动

该链路既负责遥控端下发控制命令，也负责车端上报心跳与遥测。

## 2. 物理层参数

| 项目 | 当前值 |
| --- | --- |
| 接口 | LPUART8 / `wireless_uart` |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 多字节字节序 | 小端 Little-Endian |

## 3. 帧格式

```
[0x55][0xAA][CMD][LEN][PAYLOAD...][CHECKSUM]
```

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| HEADER0 | 1B | 固定 `0x55` |
| HEADER1 | 1B | 固定 `0xAA` |
| CMD | 1B | 命令字 |
| LEN | 1B | `PAYLOAD` 长度 |
| PAYLOAD | 0~16B | 有效载荷 |
| CHECKSUM | 1B | 从 `HEADER0` 到最后一个 `PAYLOAD` 字节的累加和低 8 位 |

约束：

- 最大载荷长度：`16` 字节
- 最大整帧长度：`21` 字节
- `protocol_poll()` 当前每次最多从无线 FIFO 读取 `32` 字节进入状态机

### 校验算法

```text
CHECKSUM = (Byte0 + Byte1 + Byte2 + Byte3 + ... + Byte(3+LEN)) & 0xFF
```

## 4. 命令定义

### 4.1 `CMD = 0x01` 控制帧

方向：遥控端 -> 车端

`LEN = 6`

| 字段 | 偏移 | 类型 | 说明 |
| --- | --- | --- | --- |
| `vx` | +0 | `int16_t` LE | 遥控原始前后速度命令 |
| `vy` | +2 | `int16_t` LE | 遥控原始横移速度命令 |
| `wz` | +4 | `int16_t` LE | 遥控原始转向速度命令 |

协议层收到后，业务层在 `cmd_dispatcher.c` 中做二次映射：

| 参数 | 实际映射公式 | 备注 |
| --- | --- | --- |
| `vx` | `raw_vx * 3.00f * 1.0f` | 当前代码未取反 |
| `vy` | `raw_vy * 3.00f * 1.0f` | 当前代码未取反 |
| `wz` | `raw_wz * 1.20f * -1.0f` | 当前代码取反 |

实现注意：

- 收到控制帧后，`motion_manager_set_remote_cmd()` 会写入遥控目标速度。
- 如果当前模式是 `MOTION_MODE_IDLE`，收到有效控制帧会自动切到 `MOTION_MODE_REMOTE`。
- 车体控制链路后续还会受 `control.c` 的轮速限幅影响，默认 `speed_limit = 420 mm/s`，因此遥控原始值并不会直接等同于最终轮速。

### 4.2 `CMD = 0x02` 心跳帧

方向：双向

`LEN = 1`

| 字段 | 偏移 | 类型 | 说明 |
| --- | --- | --- | --- |
| `status` | +0 | `uint8_t` | 链路状态码 |

当前车端使用的状态码：

| 值 | 含义 |
| --- | --- |
| `0x00` | 失控保护已触发 |
| `0x01` | 正常 |

实现注意：

- 车端主循环每 `200 ms` 调用 `cmd_dispatcher_send_heartbeat()` 主动回传一次心跳。
- 当前车端接收到 `0x02` 时，未注册外部回调，由 `protocol.c` 内置逻辑更新 `s_heartbeat.status` 与 `s_heartbeat.valid`。

### 4.3 `CMD = 0x03` 模式切换帧 / 应答帧

请求方向：遥控端 -> 车端  
应答方向：车端 -> 遥控端

请求帧：

- `LEN = 1`
- 载荷：`[mode]`

应答帧：

- `LEN = 2`
- 载荷：`[mode][result]`

模式枚举来自 `motion_manager.h`：

| 值 | 枚举 | 含义 |
| --- | --- | --- |
| `0` | `MOTION_MODE_IDLE` | 停车 |
| `1` | `MOTION_MODE_REMOTE` | 遥控模式 |
| `2` | `MOTION_MODE_AUTO` | 自动模式，当前为预留入口 |
| `3` | `MOTION_MODE_TEST_WHEEL` | 单轮测试 |
| `4` | `MOTION_MODE_TEST_MOVE` | 距离移动测试 |

应答结果：

| 值 | 含义 |
| --- | --- |
| `0x00` | 失败 |
| `0x01` | 成功 |

实现注意：

- `motion_manager_set_mode()` 负责模式切换。
- 模式号越界时，车端直接回 `result = 0x00`。
- 请求与应答共用同一个 `CMD = 0x03`，通过 `LEN=1` / `LEN=2` 区分语义。

### 4.4 `CMD = 0x04` 参数下发帧

方向：遥控端 -> 车端

`LEN = 5`

载荷格式：

```text
[param_id][value_i32_le]
```

| `param_id` | 当前热更新目标 |
| --- | --- |
| `0x01` | `wheel_pid.kp` |
| `0x02` | `wheel_pid.ki` |
| `0x03` | `wheel_pid.kd` |
| `0x04` | `wheel_pid.output_limit` |
| `0x05` | `speed_limit` |
| `0x06` | `position_pid.kp` |
| `0x07` | `position_pid.ki` |
| `0x08` | `position_pid.kd` |
| `0x09` | `position_speed_limit` |

实现注意：

- 参数通过 `parameter_get()` 取得的全局参数表直接热更新。
- 当前没有协议级应答帧。
- 当前没有自动持久化，`parameter_save()` 仍是预留接口。

### 4.5 `CMD = 0x05` 遥测请求帧

方向：遥控端 -> 车端

`LEN = 1`

| 字段 | 偏移 | 类型 | 说明 |
| --- | --- | --- | --- |
| `reserved` | +0 | `uint8_t` | 当前未使用 |

实现注意：

- 当前处理函数忽略载荷内容。
- 收到该帧后，车端立即调用 `cmd_dispatcher_send_telemetry()` 回传一帧 `0x06`。

### 4.6 `CMD = 0x06` 遥测数据帧

方向：车端 -> 遥控端

`LEN = 16`

| 字段 | 偏移 | 类型 | 说明 |
| --- | --- | --- | --- |
| `wheel_speed_lf` | +0 | `int16_t` LE | 左前轮反馈速度，mm/s |
| `wheel_speed_rf` | +2 | `int16_t` LE | 右前轮反馈速度，mm/s |
| `wheel_speed_lb` | +4 | `int16_t` LE | 左后轮反馈速度，mm/s |
| `wheel_speed_rb` | +6 | `int16_t` LE | 右后轮反馈速度，mm/s |
| `vx_feedback` | +8 | `int16_t` LE | 车体速度反馈，mm/s |
| `vy_feedback` | +10 | `int16_t` LE | 车体速度反馈，mm/s |
| `wz_feedback` | +12 | `int16_t` LE | 角速度反馈，单位 `0.01 deg/s` |
| `mode` | +14 | `uint8_t` | 当前运动模式 |
| `status` | +15 | `uint8_t` | 当前状态码 |

实现注意：

- 车端主循环每 `200 ms` 主动发送一次遥测。
- 收到 `0x05` 请求时也会立即补发一帧。
- `status` 当前由 `cmd_dispatcher_send_telemetry()` 固定写成 `PROTO_STATUS_NORMAL`，不会随 failsafe 自动变化；当前 failsafe 状态主要通过心跳帧体现。

## 5. 当前车端实现行为

### 5.1 初始化与调用链

主流程在 `main.c`：

1. `wireless_uart_init()`
2. `protocol_init()`
3. `cmd_dispatcher_init()`
4. 主循环持续调用 `protocol_poll()`
5. 每 `200 ms` 调用：
   - `cmd_dispatcher_send_heartbeat()`
   - `cmd_dispatcher_send_telemetry()`

失控保护节拍在 `isr.c`：

- `protocol_tick()` 由 `BOARD_IMU_PERIOD_MS = 5 ms` 的 PIT 节拍驱动

### 5.2 失控保护

当前规则：

- 任意一帧“校验通过的有效协议帧”都会重置 failsafe 看门狗
- 连续 `500 ms` 没有收到有效帧时：
  - `s_velocity.failsafe = true`
  - `s_velocity.valid = false`
  - `s_heartbeat.valid = false`
  - `vx/vy/wz` 清零
  - `failsafe_count` 计数加一

注意：

- 看门狗并不只盯控制帧，`0x02/0x03/0x04/0x05` 等有效帧同样能保活。

### 5.3 回调分发关系

`cmd_dispatcher_init()` 当前注册的命令：

- `0x01` 控制帧
- `0x03` 模式切换
- `0x04` 参数下发
- `0x05` 遥测请求

未注册、由 `protocol.c` 内置处理的命令：

- `0x02` 心跳

实现上的一个重要细节：

- `dispatch_frame()` 里如果某个命令注册了外部回调，就不会再执行 `protocol.c` 的内置默认处理。
- 因此在当前实现中，`0x01` 控制帧主要通过 `cmd_dispatcher` 写入 `motion_manager`，`protocol_get_velocity()` 更可靠的字段是 `valid` / `failsafe`，而不是原始 `vx/vy/wz` 数值本身。

## 6. 与业务模块的对应关系

| 协议环节 | 代码入口 | 当前作用 |
| --- | --- | --- |
| 收发底层 | `wireless_uart_*` | 无线串口收发 |
| 协议解析 | `protocol.c` | 帧组包、状态机、校验、failsafe |
| 命令分发 | `cmd_dispatcher.c` | 命令到业务逻辑的映射 |
| 模式管理 | `motion_manager.c` | 遥控模式切换、控制目标生成 |
| 控制闭环 | `control.c` | 轮速限幅、PID、输出到电机 |

## 7. 联调注意事项

- 如果遥控端只发控制帧不发心跳，也能维持链路在线，因为任意有效帧都会喂狗。
- 如果只发参数下发或模式切换帧，也会让 failsafe 保持未触发。
- 若上位机想准确识别“车端是否进入 failsafe”，优先看车端回传的 `0x02` 心跳状态，不要只看 `0x06` 遥测状态位。
- 旧说明里关于摇杆增益和极性的文字与当前宏定义并非完全一致，联调时应以 `cmd_dispatcher.h` 和 `cmd_dispatcher.c` 的实际公式为准。
