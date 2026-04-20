# RT1064 遥控器无线串口通信协议 V2.0

## 1. 物理层

| 参数 | 值 |
|------|------|
| 硬件 | 逐飞无线串口模块 (wireless_uart) |
| 接口 | LPUART8 |
| 波特率 | 115200 bps |
| 数据位 | 8 bit |
| 停止位 | 1 bit |
| 校验 | 无 |
| 字节序 | **小端 (Little-Endian)** |

---

## 2. 帧格式

```
┌────────┬────────┬──────┬──────┬──────────────────┬──────────┐
│ HEADER │ HEADER │ CMD  │ LEN  │    PAYLOAD       │ CHECKSUM │
│  0x55  │  0xAA  │ 1B   │ 1B   │   LEN bytes      │   1B     │
├────────┼────────┼──────┼──────┼──────────────────┼──────────┤
│ Byte 0 │ Byte 1 │ B2   │ B3   │ B4 ... B(3+LEN)  │ B(4+LEN) │
└────────┴────────┴──────┴──────┴──────────────────┴──────────┘
```

| 字段 | 长度 | 说明 |
|------|------|------|
| HEADER | 2 字节 | 固定 `0x55 0xAA` |
| CMD | 1 字节 | 命令字，见下表 |
| LEN | 1 字节 | PAYLOAD 字节数 |
| PAYLOAD | LEN 字节 | 命令负载，最大 16 字节 |
| CHECKSUM | 1 字节 | 校验和 |

**整帧最大长度**：2 + 1 + 1 + 16 + 1 = **21 字节**

---

## 3. 校验算法

**Sum Check**：从帧头第一个字节到 PAYLOAD 最后一个字节（不含 CHECKSUM 本身）的所有字节累加，取低 8 位。

```
CHECKSUM = (Byte0 + Byte1 + Byte2 + Byte3 + ... + Byte(3+LEN)) & 0xFF
```

### 示例

控制帧 `vx=100, vy=0, wz=0`：

```
55 AA 01 06  64 00  00 00  00 00
│            │      │      │
│            vx=100 vy=0   wz=0
│
Sum = 0x55+0xAA+0x01+0x06+0x64+0x00+0x00+0x00+0x00+0x00 = 0x170
CHECKSUM = 0x70

完整帧: 55 AA 01 06 64 00 00 00 00 00 70
```

---

## 4. 命令字定义

### 4.1 CMD = 0x01 — 控制帧 (遥控端 → 车端)

| 字段 | 偏移 | 类型 | 范围 | 说明 |
|------|------|------|------|------|
| LEN | — | uint8 | 固定 `6` | — |
| vx | +0 | int16_t LE | -1000 ~ +1000 | 前进/后退速度 |
| vy | +2 | int16_t LE | -1000 ~ +1000 | 横移速度 |
| wz | +4 | int16_t LE | -1000 ~ +1000 | 旋转速度 |

**摇杆映射**（遥控器端）：

```
         vy+
          ↑
          │
  vx- ←──┼──→ vx+        wz- ←──┼──→ wz+
          │
          ↓
         vy-

   [左摇杆: vx, vy]       [右摇杆: wz]
```

| 参数 | 摇杆源 | 极性说明 |
|------|--------|----------|
| `vx` | 左摇杆 X 轴 (`lx`) | 右推为正，左推为负 |
| `vy` | 左摇杆 Y 轴 (`-ly`) | 上推为正，下推为负（与屏幕显示一致） |
| `wz` | 右摇杆 X 轴 (`rx`) | 右推为正（逆时针），左推为负（顺时针） |

**车端映射增益** (在 `cmd_dispatcher.h` 中定义):

| 参数 | 增益 | 极性 | 效果 |
|------|------|------|------|
| `vx` | 0.20 | +1 | ±1000 → ±200 mm/s |
| `vy` | 0.20 | -1 | ±1000 → ±200 mm/s (取反) |
| `wz` | 0.05 | -1 | ±1000 → ±50 deg/s (取反) |

### 4.2 CMD = 0x02 — 心跳帧 (双向)

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `1` |
| status | +0 | uint8 | 状态码 |

**状态码定义**：

| 值 | 含义 |
|----|------|
| `0x00` | 失控保护已触发 |
| `0x01` | 正常运行 |

### 4.3 CMD = 0x03 — 模式切换帧

**请求** (遥控端 → 车端):

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `1` |
| mode | +0 | uint8 | 目标模式 |

**应答** (车端 → 遥控端):

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `2` |
| mode | +0 | uint8 | 请求的模式 |
| result | +1 | uint8 | 0x00=失败, 0x01=成功 |

**模式定义**：

| 值 | 枚举名 | 说明 |
|----|--------|------|
| `0` | `MOTION_MODE_IDLE` | 停车 |
| `1` | `MOTION_MODE_REMOTE` | 遥控模式 |
| `2` | `MOTION_MODE_AUTO` | 自动导航 (预留) |
| `3` | `MOTION_MODE_TEST_WHEEL` | 单轮恒速测试 |
| `4` | `MOTION_MODE_TEST_MOVE` | 距离移动测试 |

### 4.4 CMD = 0x04 — 参数下发帧 (遥控端 → 车端)

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `5` |
| param_id | +0 | uint8 | 参数编号 |
| value | +1 | int32_t LE | 参数值 |

**参数编号**：

| ID | 参数 | 单位 |
|----|------|------|
| 0x01 | wheel_pid.kp | ×1000 |
| 0x02 | wheel_pid.ki | ×1000 |
| 0x03 | wheel_pid.kd | ×1000 |
| 0x04 | wheel_pid.output_limit | duty |
| 0x05 | speed_limit | mm/s |
| 0x06 | position_pid.kp | ×1000 |
| 0x07 | position_pid.ki | ×1000 |
| 0x08 | position_pid.kd | ×1000 |
| 0x09 | position_speed_limit | mm/s |

### 4.5 CMD = 0x05 — 遥测请求帧 (遥控端 → 车端)

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `1` |
| reserved | +0 | uint8 | 预留，填 0 |

车端收到后立即回传一帧 CMD=0x06 遥测数据。

### 4.6 CMD = 0x06 — 遥测数据帧 (车端 → 遥控端)

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `16` |
| wheel_speed_lf | +0 | int16_t LE | 左前轮速 mm/s |
| wheel_speed_rf | +2 | int16_t LE | 右前轮速 mm/s |
| wheel_speed_lb | +4 | int16_t LE | 左后轮速 mm/s |
| wheel_speed_rb | +6 | int16_t LE | 右后轮速 mm/s |
| vx_feedback | +8 | int16_t LE | 车体 vx mm/s |
| vy_feedback | +10 | int16_t LE | 车体 vy mm/s |
| wz_feedback | +12 | int16_t LE | 车体 wz 0.01 deg/s |
| mode | +14 | uint8 | 当前运动模式 |
| status | +15 | uint8 | 系统状态 |

车端按 200ms 周期主动发送，或在收到 CMD=0x05 时立即回传。

---

## 5. 当前实际行为

### 5.1 遥控器端（RT1064 → 无线串口发出）

| 行为 | 参数 |
|------|------|
| 发送内容 | CMD=0x01 控制帧 |
| 发送周期 | 每 **50ms**（20 Hz） |
| 数据来源 | ADC 采集双摇杆，EMA 滤波后归一化到 ±1000 |

### 5.2 接收端（无线串口收入 → RT1064 解析）

| 行为 | 参数 |
|------|------|
| 解析方式 | 非阻塞状态机，逐字节解析 |
| 支持命令 | 0x01~0x06 (通过回调表扩展) |
| 失控保护 | **500ms** 未收到任何有效帧 → vx/vy/wz 强制清零 |
| 命令分发 | 通过 `cmd_dispatcher` 回调注册，协议层与业务逻辑解耦 |
| 遥测回传 | 每 200ms 主动发送 CMD=0x06 + 心跳 CMD=0x02 |

---

## 6. 状态机解析流程

```
                    ┌──────────────────────────────────┐
                    │                                  │
                    ▼                                  │
              ┌──────────┐                             │
          ┌──▶│   IDLE   │  等待 0x55                  │
          │   └────┬─────┘                             │
          │        │ 收到 0x55                         │
          │        ▼                                   │
          │   ┌──────────┐                             │
          │   │  HEADER  │  等待 0xAA                  │
          │   └────┬─────┘                             │
          │        │ 收到 0xAA                         │
          │        ▼                                   │
          │   ┌──────────┐                             │
          │   │   CMD    │  存储命令字                  │
          │   └────┬─────┘                             │
          │        │                                   │
          │        ▼                                   │
          │   ┌──────────┐                             │
          │   │   LEN    │  存储长度，校验合法性         │
          │   └────┬─────┘                             │
          │        │ LEN > 0                           │
          │        ▼                                   │
          │   ┌──────────┐                             │
          │   │ PAYLOAD  │  累积 LEN 字节              │
          │   └────┬─────┘                             │
          │        │ 收满 LEN 字节                     │
          │        ▼                                   │
          │   ┌──────────┐  校验通过: 处理帧           │
          └───┤ CHECKSUM │──────────────────────────────┘
              └──────────┘  校验失败: 计错误数
```

---

## 7. 软件架构

### 7.1 命令分发链路

```
wireless FIFO → protocol_poll() → parser 状态机 → dispatch_frame()
                                                        │
                                     ┌──────────────────┼──────────────────┐
                                     ▼                  ▼                  ▼
                              回调表查找         内置 0x01 处理      内置 0x02 处理
                              s_handlers[cmd]   (fallback)          (fallback)
                                     │
                    ┌────────────────┼────────────────────┐
                    ▼                ▼                    ▼
             cmd_dispatcher    cmd_dispatcher       cmd_dispatcher
             on_control()      on_mode_set()        on_param_set()
                    │                │                    │
                    ▼                ▼                    ▼
             motion_manager    motion_manager        parameter_get()
             set_remote_cmd()  set_mode()            → 热更新
```

### 7.2 运动控制链路

```
motion_manager_update_20ms()  ← PIT ISR 20ms
         │
         ├─ REMOTE: g_remote_cmd → control_set_velocity_target()
         ├─ TEST_MOVE: position_pid → velocity target
         ├─ TEST_WHEEL: control_set_wheel_speed_direct()
         └─ IDLE: zero velocity
                           │
                           ▼
              control_update_20ms()  ← PIT ISR 20ms
                           │
         ┌─────────────────┼──────────────────┐
         ▼                 ▼                  ▼
    encoder_update   chassis_set_velocity   motor_set_output_all
    odometry_update  kinematics_inverse     (4x wheel PID)
```

---

## 8. API 速查

### 协议层

```c
/* 初始化 */
void protocol_init(void);

/* 注册命令处理回调 */
void protocol_register_handler(uint8_t cmd, proto_cmd_handler_fn handler);

/* 发送 */
void protocol_send_control(int16_t vx, int16_t vy, int16_t wz);
void protocol_send_heartbeat(uint8_t status);
void protocol_send_mode_ack(uint8_t mode, uint8_t result);
void protocol_send_telemetry(const proto_telemetry_data_t *data);
void protocol_send_raw(uint8_t cmd, const uint8_t *payload, uint8_t len);

/* 接收 / 查询 */
uint32_t protocol_poll(void);
void protocol_tick(void);
const proto_velocity_t    *protocol_get_velocity(void);
const proto_heartbeat_t   *protocol_get_heartbeat(void);
const proto_ctx_t         *protocol_get_ctx(void);
```

### 命令分发器

```c
void cmd_dispatcher_init(void);
void cmd_dispatcher_send_telemetry(void);
void cmd_dispatcher_send_heartbeat(void);
```

### 运动管理器

```c
void motion_manager_init(void);
bool motion_manager_set_mode(motion_mode_t mode);
void motion_manager_set_remote_cmd(const app_velocity_t *cmd);
void motion_manager_update_20ms(void);
void motion_manager_start_test_move(motion_test_axis_t axis, float target);
void motion_manager_stop_test_move(void);
void motion_manager_set_test_wheel_speed(const app_wheel4_t *speed);
motion_mode_t   motion_manager_get_mode(void);
motion_status_t motion_manager_get_status(void);
```

### 速度控制器

```c
void control_init(void);
void control_update_20ms(void);
void control_set_velocity_target(const app_velocity_t *target);
void control_set_wheel_speed_direct(const app_wheel4_t *speed);
void control_emergency_stop(void);
app_velocity_t control_get_velocity_target(void);
app_velocity_t control_get_body_velocity_feedback(void);
app_wheel4_t   control_get_wheel_speed_target(void);
app_wheel4_t   control_get_wheel_speed_feedback(void);
app_wheel4_t   control_get_wheel_output(void);
```

---

## 9. Python 对接示例

```python
import serial
import struct
import time

ser = serial.Serial('COM3', 115200, timeout=0.1)

def parse_frame(data: bytes):
    """从字节流中解析帧"""
    while len(data) >= 5:  # 最小帧: header(2)+cmd(1)+len(1)+checksum(1)
        # 找帧头
        idx = data.find(b'\x55\xAA')
        if idx < 0:
            break
        data = data[idx:]

        if len(data) < 4:
            break

        cmd = data[2]
        plen = data[3]
        frame_len = 4 + plen + 1  # header+cmd+len + payload + checksum

        if len(data) < frame_len:
            break

        # 校验
        checksum = sum(data[:4 + plen]) & 0xFF
        if checksum != data[4 + plen]:
            data = data[2:]  # 跳过错误帧头，继续搜索
            continue

        payload = data[4:4 + plen]

        if cmd == 0x01 and plen == 6:
            vx, vy, wz = struct.unpack('<hhh', payload)
            print(f"控制帧: vx={vx}, vy={vy}, wz={wz}")

        elif cmd == 0x02 and plen == 1:
            status = payload[0]
            print(f"心跳帧: status=0x{status:02X}")

        elif cmd == 0x03 and plen == 2:
            mode, result = payload[0], payload[1]
            print(f"模式应答: mode={mode}, result={'OK' if result else 'FAIL'}")

        elif cmd == 0x06 and plen == 16:
            ws = struct.unpack('<hhhh', payload[0:8])
            vx_fb, vy_fb, wz_fb = struct.unpack('<hhh', payload[8:14])
            mode, status = payload[14], payload[15]
            print(f"遥测: wheels={ws}, vx={vx_fb} vy={vy_fb} wz={wz_fb/100:.1f} mode={mode} st={status}")

        data = data[frame_len:]

    return data

def send_control(vx, vy, wz):
    """发送控制帧"""
    payload = struct.pack('<hhh', vx, vy, wz)
    frame = bytes([0x55, 0xAA, 0x01, 0x06]) + payload
    checksum = sum(frame) & 0xFF
    ser.write(frame + bytes([checksum]))

def send_heartbeat(status=0x01):
    """发送心跳帧"""
    payload = bytes([0x55, 0xAA, 0x02, 0x01, status])
    checksum = sum(payload) & 0xFF
    ser.write(payload + bytes([checksum]))

def send_mode_set(mode):
    """发送模式切换帧"""
    payload = bytes([0x55, 0xAA, 0x03, 0x01, mode])
    checksum = sum(payload) & 0xFF
    ser.write(payload + bytes([checksum]))

def send_param_set(param_id, value):
    """发送参数下发帧"""
    val_bytes = struct.pack('<i', value)
    frame = bytes([0x55, 0xAA, 0x04, 0x05, param_id]) + val_bytes
    checksum = sum(frame) & 0xFF
    ser.write(frame + bytes([checksum]))

def send_telemetry_req():
    """请求遥测数据"""
    payload = bytes([0x55, 0xAA, 0x05, 0x01, 0x00])
    checksum = sum(payload) & 0xFF
    ser.write(payload + bytes([checksum]))

# 主循环
buf = b''
while True:
    buf += ser.read(64)
    if buf:
        buf = parse_frame(buf)
    time.sleep(0.01)
```
