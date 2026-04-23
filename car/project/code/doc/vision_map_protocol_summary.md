# 车端视觉地图串口协议总结

本文档按当前车端实现整理视觉模块与车端之间的地图串口协议，重点对应以下代码：

- `car/project/code/inc/vision_map.h`
- `car/project/code/src/vision_map.c`
- `car/project/code/inc/grid_map.h`
- `car/project/code/inc/board_config.h`
- `car/project/user/src/main.c`
- `car/project/user/src/isr.c`

已有参考说明见 `car/project/code/doc/vision_protocol.md`。本文以当前代码实际行为为准。

## 1. 链路概览

- 物理链路：视觉模块 <-> 车端 UART4
- 主要用途：把视觉识别出的 `16 x 12` 栅格地图与车模位姿送入车端
- 接收中断入口：`LPUART4_IRQHandler()`
- 主循环处理：`vision_map_poll()`
- 在线检测节拍：`vision_map_tick()`，由 5 ms PIT 节拍驱动

当前车端已经具备：

- 接收地图帧
- 接收心跳/ACK 帧
- 主动发送地图请求帧
- 主动发送心跳帧

但当前主循环实际只持续执行“初始化 + 接收解析 + 在线超时判断”，并没有周期性主动发送请求帧或心跳帧。

## 2. 物理层参数

| 项目 | 当前值 |
| --- | --- |
| 接口 | `UART4` / `LPUART4` |
| TX 引脚 | `UART4_TX_C16` |
| RX 引脚 | `UART4_RX_C17` |
| 波特率 | `115200` |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |

代码来源：

- `BOARD_VISION_UART_INDEX`
- `BOARD_VISION_UART_TX_PIN`
- `BOARD_VISION_UART_RX_PIN`
- `BOARD_VISION_UART_BAUD`

## 3. 帧格式

```
[0xA5][0x5A][CMD][LEN][PAYLOAD...][CHECKSUM]
```

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| `HEADER0` | 1B | 固定 `0xA5` |
| `HEADER1` | 1B | 固定 `0x5A` |
| `CMD` | 1B | 命令字 |
| `LEN` | 1B | 有效载荷长度 |
| `PAYLOAD` | 0~200B | 有效载荷 |
| `CHECKSUM` | 1B | `CMD ^ LEN ^ PAYLOAD[0] ^ ... ^ PAYLOAD[n-1]` |

当前实现约束：

- `VISION_PAYLOAD_MAX = 200`
- 解析器采用逐字节状态机
- 接收 ISR 先把字节写入 `512B` 环形缓冲区
- `vision_map_poll()` 每次最多消费 `300` 字节，避免主循环阻塞过久

### 校验算法

```text
CHECKSUM = CMD ^ LEN ^ PAYLOAD[0] ^ ... ^ PAYLOAD[LEN-1]
```

## 4. 命令定义

### 4.1 `CMD = 0x01` 地图帧

方向：视觉模块 -> 车端

`LEN = 195`

载荷布局：

```text
[car_x][car_y][car_dir][map_data[192]]
```

字段说明：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `car_x` | `uint8_t` | 车模 X 坐标，范围 `0~15` |
| `car_y` | `uint8_t` | 车模 Y 坐标，范围 `0~11` |
| `car_dir` | `uint8_t` | 车模朝向，范围 `0~3` |
| `map_data` | `uint8_t[192]` | `16 x 12` 地图，按行优先 |

朝向枚举来自 `grid_map.h`：

| 值 | 枚举 | 含义 |
| --- | --- | --- |
| `0` | `CAR_DIR_UP` | 朝上 `(+Y)` |
| `1` | `CAR_DIR_RIGHT` | 朝右 `(+X)` |
| `2` | `CAR_DIR_DOWN` | 朝下 `(-Y)` |
| `3` | `CAR_DIR_LEFT` | 朝左 `(-X)` |

地图单元枚举来自 `grid_map.h`：

| 值 | 枚举 | 含义 |
| --- | --- | --- |
| `0` | `GRID_CELL_UNKNOWN` | 未知 |
| `1` | `GRID_CELL_EMPTY` | 空地 |
| `2` | `GRID_CELL_WALL` | 墙 |
| `3` | `GRID_CELL_BOX` | 箱子 |
| `4` | `GRID_CELL_GOAL` | 目标点 |
| `5` | `GRID_CELL_BOMB` | 炸弹箱 |
| `6` | `GRID_CELL_CAR` | 虚拟车模位置 |

排列方式：

- 地图宽度 `GRID_MAP_W = 16`
- 地图高度 `GRID_MAP_H = 12`
- `map_data[y * 16 + x]` 对应 `map.cells[y][x]`

当前实现对地图帧的处理：

- `car_x >= 16`、`car_y >= 12`、`car_dir > 3` 时整帧判坏
- `map_data` 中若单元值大于 `GRID_CELL_CAR`，则自动降级为 `GRID_CELL_UNKNOWN`
- 成功后更新：
  - `s_data.map`
  - `s_data.car_x`
  - `s_data.car_y`
  - `s_data.car_dir`
  - `s_data.valid = true`
  - `s_data.connected = true`
  - `s_timeout_cnt = 0`

### 4.2 `CMD = 0x02` 地图请求帧

方向：车端 -> 视觉模块

`LEN = 1`

| 字段 | 类型 | 当前值 | 说明 |
| --- | --- | --- | --- |
| `request_type` | `uint8_t` | `0x01` | 请求全量地图 |

实现入口：

- `vision_map_request()`

当前状态：

- 接口已实现
- 当前主循环没有自动调用它

### 4.3 `CMD = 0x03` 心跳 / ACK 帧

方向：双向

`LEN = 1`

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `status` | `uint8_t` | 状态码 |

已有文档 `vision_protocol.md` 中给出的状态码语义：

| 值 | 含义 |
| --- | --- |
| `0x00` | 故障 |
| `0x01` | 正常 |

但当前 `vision_map.c` 的实际行为是：

- 收到 `0x03` 时，仅将其当作在线保活帧
- 当前不解析 `status` 语义，也不据此分支处理

发送入口：

- `vision_map_send_heartbeat(uint8_t status)`

当前状态：

- 接口已实现
- 当前主循环没有自动周期发送

## 5. 当前车端实现行为

### 5.1 初始化与调用链

主流程在 `main.c`：

1. `vision_map_init()`
2. 主循环持续调用 `vision_map_poll()`

中断与节拍在 `isr.c`：

- `LPUART4_IRQHandler()` -> `vision_map_uart_isr()`
- `vision_map_tick()` 由 5 ms PIT 节拍驱动

### 5.2 接收缓冲与状态机

接收路径如下：

```text
LPUART4_IRQHandler
  -> vision_map_uart_isr()
  -> 环形缓冲区 s_rx_buf[512]
  -> vision_map_poll()
  -> parse_byte()
  -> on_frame_received()
```

状态机阶段：

- 等待 `0xA5`
- 等待 `0x5A`
- 接收 `CMD`
- 接收 `LEN`
- 接收 `PAYLOAD`
- 接收 `CHECKSUM`

### 5.3 在线检测

超时规则：

- `vision_map_tick()` 每次按 `BOARD_IMU_PERIOD_MS = 5 ms` 累加计时
- 超过 `VISION_TIMEOUT_MS = 1000 ms` 后：
  - `s_data.connected = false`
  - 超时计数饱和在 `1000 ms`

注意：

- 超时只会把 `connected` 置为 `false`
- `valid` 不会因此清零
- 最后一帧已成功解析的地图内容也会保留

### 5.4 对外查询语义

当前对外接口：

- `vision_map_get_data()`：返回最近一次解析结果
- `vision_map_is_online()`：只返回 `s_data.connected`

实现细节：

- `vision_map_is_online()` 不检查 `valid`
- `vision_map_data_t.update_tick` 字段当前没有被真正维护为系统时间戳，成功收帧时只是被写成 `0`

## 6. 与业务模块的对应关系

| 协议环节 | 代码入口 | 当前作用 |
| --- | --- | --- |
| 串口初始化 | `vision_map_init()` | 初始化 UART4 与接收中断 |
| ISR 入缓冲 | `vision_map_uart_isr()` | 避免硬件 FIFO 溢出 |
| 主循环解析 | `vision_map_poll()` | 逐字节状态机解帧 |
| 地图落地 | `on_frame_received()` | 写入 `grid_map_t` 与车模位姿 |
| 在线检测 | `vision_map_tick()` | 1 s 超时掉线判定 |

## 7. 联调注意事项

- 视觉模块如果只周期发送地图帧，不发 `0x03` 心跳，车端同样会认为链路在线，因为有效地图帧会重置超时计数。
- 车端当前不会主动周期请求地图，也不会主动周期发送视觉心跳；如果联调需要主从握手，需明确由哪一端先发。
- 如果视觉模块发来的地图单元值超出 `0~6`，车端会静默降级为 `UNKNOWN`，不会报错中止。
- 如果上层既关心“是否曾经收到过地图”又关心“当前是否在线”，需要同时参考：
  - `vision_map_get_data()->valid`
  - `vision_map_is_online()`
