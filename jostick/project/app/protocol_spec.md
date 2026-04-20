# RT1064 遥控器无线串口通信协议 V1.0

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
| PAYLOAD | LEN 字节 | 命令负载，最大 6 字节 |
| CHECKSUM | 1 字节 | 校验和 |

**整帧最大长度**：2 + 1 + 1 + 6 + 1 = **11 字节**

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

### 4.1 CMD = 0x01 — 控制帧

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

### 4.2 CMD = 0x02 — 心跳帧

| 字段 | 偏移 | 类型 | 说明 |
|------|------|------|------|
| LEN | — | uint8 | 固定 `1` |
| status | +0 | uint8 | 状态码 |

**状态码定义**：

| 值 | 含义 |
|----|------|
| `0x00` | 失控保护已触发 |
| `0x01` | 正常运行 |

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
| 支持命令 | 0x01 控制帧、0x02 心跳帧 |
| 失控保护 | **500ms** 未收到任何有效帧 → vx/vy/wz 强制清零 |

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

## 7. API 速查

### 发送

```c
/* 发送控制帧 */
protocol_send_control(int16_t vx, int16_t vy, int16_t wz);

/* 发送心跳帧 */
protocol_send_heartbeat(uint8 status);

/* 发送自定义帧（自动加帧头+校验） */
protocol_send_raw(uint8 cmd, const uint8 *payload, uint8 len);
```

### 接收 / 查询

```c
/* 主循环调用：解析 + 发送 */
uint32_t protocol_poll(void);

/* ISR 中调用：驱动看门狗 + 发送定时 */
void protocol_tick(void);

/* 获取解析结果 */
const proto_velocity_t    *protocol_get_velocity(void);
const proto_wheel_speed_t *protocol_get_wheel_speeds(void);
const proto_heartbeat_t   *protocol_get_heartbeat(void);
const proto_ctx_t         *protocol_get_ctx(void);
```

---

## 8. Python 对接示例

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

        data = data[frame_len:]

    return data

def send_heartbeat(status=0x01):
    """向遥控器发送心跳"""
    payload = bytes([0x55, 0xAA, 0x02, 0x01, status])
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
