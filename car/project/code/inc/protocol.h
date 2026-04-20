#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * 文件作用:
 * 1. 定义车端与遥控器之间的无线串口通信协议（V2.0）。
 * 2. 帧格式: [0x55][0xAA][CMD][LEN][PAYLOAD...][CHECKSUM]
 * 3. 车端作为接收端解析控制帧、心跳帧、模式切换帧和参数下发帧。
 * 4. 车端同时作为发送端回传心跳应答、遥测数据等。
 * 5. 支持通过回调函数表扩展命令处理。
 */

/* ---- 帧头 ---- */
#define PROTO_HEADER_0          (0x55U)
#define PROTO_HEADER_1          (0xAAU)

/* ---- 命令字 ---- */
#define PROTO_CMD_CONTROL       (0x01U)     /* 控制帧：vx, vy, wz               */
#define PROTO_CMD_HEARTBEAT     (0x02U)     /* 心跳帧：status                    */
#define PROTO_CMD_MODE_SET      (0x03U)     /* 模式切换：mode                    */
#define PROTO_CMD_PARAM_SET     (0x04U)     /* 参数下发：param_id + value        */
#define PROTO_CMD_TELEMETRY_REQ (0x05U)     /* 遥测请求（遥控端 → 车端）         */
#define PROTO_CMD_TELEMETRY     (0x06U)     /* 遥测数据（车端 → 遥控端）         */
#define PROTO_CMD_MAX           (0x10U)     /* 命令字上限，用于回调表大小         */

/* ---- 帧参数 ---- */
#define PROTO_PAYLOAD_MAX       (16U)       /* 单帧最大有效载荷                  */
#define PROTO_FRAME_MAX         (21U)       /* 2+1+1+16+1 = 21                   */
#define PROTO_CONTROL_LEN       (6U)        /* 控制帧 payload 长度               */
#define PROTO_HEARTBEAT_LEN     (1U)        /* 心跳帧 payload 长度               */
#define PROTO_MODE_SET_LEN      (1U)        /* 模式切换帧 payload 长度           */
#define PROTO_PARAM_SET_LEN     (5U)        /* 参数下发帧: 1B id + 4B value      */
#define PROTO_TELEMETRY_REQ_LEN (1U)        /* 遥测请求帧 payload 长度           */
#define PROTO_TELEMETRY_LEN     (16U)       /* 遥测帧 payload 长度               */

/* ---- 失控保护 ---- */
#define PROTO_FAILSAFE_MS       (500U)      /* 超时 ms                           */

/* ---- 心跳状态码 ---- */
#define PROTO_STATUS_FAILSAFE   (0x00U)     /* 失控保护已触发                    */
#define PROTO_STATUS_NORMAL     (0x01U)     /* 正常运行                          */

/* ---- 数据结构 ---- */

/* 解析后的速度命令 */
typedef struct
{
    int16_t vx;             /* 前进/后退速度  -1000~+1000 */
    int16_t vy;             /* 横移速度        -1000~+1000 */
    int16_t wz;             /* 旋转速度        -1000~+1000 */
    bool    valid;          /* 至少收到过一帧有效控制帧     */
    bool    failsafe;       /* 当前处于失控保护状态         */
} proto_velocity_t;

/* 解析后的心跳 */
typedef struct
{
    uint8_t status;         /* 状态码                       */
    bool    valid;          /* 至少收到过一帧有效心跳帧     */
} proto_heartbeat_t;

/* 协议统计上下文 */
typedef struct
{
    uint32_t rx_good;       /* 校验通过的帧数               */
    uint32_t rx_bad;        /* 校验失败的帧数               */
    uint32_t tx_count;      /* 已发送帧数                   */
    uint32_t failsafe_count;/* 失控保护触发次数             */
} proto_ctx_t;

/* 遥测帧结构体 */
typedef struct
{
    int16_t  wheel_speed_lf;   /* 左前轮速 mm/s   */
    int16_t  wheel_speed_rf;   /* 右前轮速 mm/s   */
    int16_t  wheel_speed_lb;   /* 左后轮速 mm/s   */
    int16_t  wheel_speed_rb;   /* 右后轮速 mm/s   */
    int16_t  vx_feedback;      /* 车体 vx mm/s    */
    int16_t  vy_feedback;      /* 车体 vy mm/s    */
    int16_t  wz_feedback;      /* 车体 wz cdeg/s  */
    uint8_t  mode;             /* 当前运动模式     */
    uint8_t  status;           /* 系统状态         */
} proto_telemetry_data_t;

/* ---- 命令回调函数类型 ---- */
typedef void (*proto_cmd_handler_fn)(uint8_t cmd, const uint8_t *payload, uint8_t len);

/* ---- 初始化 ---- */
void protocol_init(void);

/* ---- 注册命令处理回调 ---- */
void protocol_register_handler(uint8_t cmd, proto_cmd_handler_fn handler);

/* ---- 发送 ---- */
void protocol_send_control(int16_t vx, int16_t vy, int16_t wz);
void protocol_send_heartbeat(uint8_t status);
void protocol_send_mode_ack(uint8_t mode, uint8_t result);
void protocol_send_telemetry(const proto_telemetry_data_t *data);
void protocol_send_raw(uint8_t cmd, const uint8_t *payload, uint8_t len);

/* ---- 接收 / 主循环调用 ---- */
uint32_t protocol_poll(void);           /* 解析接收缓冲区，返回本次解析的有效帧数 */

/* ---- ISR 中调用（5ms 节拍） ---- */
void protocol_tick(void);              /* 驱动失控保护看门狗计时 */

/* ---- 查询接口 ---- */
const proto_velocity_t  *protocol_get_velocity(void);
const proto_heartbeat_t *protocol_get_heartbeat(void);
const proto_ctx_t       *protocol_get_ctx(void);

#endif /* PROTOCOL_H */
