#include "protocol.h"

/*
 * 文件作用:
 * 1. 实现车端无线串口通信协议 V2.0。
 * 2. 帧格式: [0x55][0xAA][CMD][LEN][PAYLOAD...][CHECKSUM]
 * 3. 非阻塞状态机逐字节解析，内置处理 0x01/0x02，其余通过回调表扩展。
 * 4. 失控保护: 500ms 内未收到有效帧则 vx/vy/wz 强制清零。
 */

/* ---- 解析状态机枚举 ---- */
typedef enum
{
    PARSE_IDLE = 0,     /* 等待第一个帧头 0x55                 */
    PARSE_HEADER,       /* 等待第二个帧头 0xAA                 */
    PARSE_CMD,          /* 接收命令字                          */
    PARSE_LEN,          /* 接收载荷长度                        */
    PARSE_PAYLOAD,      /* 累积 LEN 字节的载荷                 */
    PARSE_CHECKSUM      /* 校验字节                            */
} parse_state_t;

/* ---- 解析器上下文 ---- */
typedef struct
{
    parse_state_t state;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[PROTO_PAYLOAD_MAX];
    uint8_t payload_idx;
    uint8_t checksum_acc;               /* 累加校验值                    */
} parser_t;

/* ---- 模块静态变量 ---- */
static proto_velocity_t  s_velocity;
static proto_heartbeat_t s_heartbeat;
static proto_ctx_t       s_ctx;
static parser_t          s_parser;

static volatile uint32_t s_failsafe_tick;       /* ISR 中递增的 5ms 计数     */
static volatile bool     s_failsafe_active;     /* 是否已触发失控保护         */

/* ---- 命令处理回调表 ---- */
static proto_cmd_handler_fn s_handlers[PROTO_CMD_MAX];

/* ---- 内部函数声明 ---- */
static void  parser_reset(parser_t *p);
static bool  parser_feed(parser_t *p, uint8_t byte);
static void  dispatch_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);

/* ================================================================
 *  初始化
 * ================================================================ */
void protocol_init(void)
{
    uint8_t i;

    s_velocity.vx       = 0;
    s_velocity.vy       = 0;
    s_velocity.wz       = 0;
    s_velocity.valid    = false;
    s_velocity.failsafe = true;         /* 开机即处于失控保护             */

    s_heartbeat.status  = 0U;
    s_heartbeat.valid   = false;

    s_ctx.rx_good       = 0U;
    s_ctx.rx_bad        = 0U;
    s_ctx.tx_count      = 0U;
    s_ctx.failsafe_count = 0U;

    parser_reset(&s_parser);

    s_failsafe_tick     = 0U;
    s_failsafe_active   = true;

    for (i = 0U; i < PROTO_CMD_MAX; ++i)
    {
        s_handlers[i] = (proto_cmd_handler_fn)0;
    }
}

/* ================================================================
 *  命令回调注册
 * ================================================================ */
void protocol_register_handler(uint8_t cmd, proto_cmd_handler_fn handler)
{
    if (cmd < PROTO_CMD_MAX)
    {
        s_handlers[cmd] = handler;
    }
}

/* ================================================================
 *  发送
 * ================================================================ */

/*
 * protocol_send_raw
 * 组帧并通过无线串口发送:
 *   [0x55][0xAA][cmd][len][payload...][checksum]
 */
void protocol_send_raw(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t frame[PROTO_FRAME_MAX];
    uint8_t i;
    uint8_t sum;
    uint8_t frame_len;

    if (len > PROTO_PAYLOAD_MAX)
    {
        return;
    }

    frame[0] = PROTO_HEADER_0;
    frame[1] = PROTO_HEADER_1;
    frame[2] = cmd;
    frame[3] = len;

    for (i = 0U; i < len; ++i)
    {
        frame[4U + i] = payload[i];
    }

    /* 校验: Byte0 到 Byte(3+len) 累加取低 8 位 */
    sum = 0U;
    frame_len = 4U + len;
    for (i = 0U; i < frame_len; ++i)
    {
        sum += frame[i];
    }
    frame[frame_len] = sum;             /* CHECKSUM */
    frame_len += 1U;

    wireless_uart_send_buffer(frame, frame_len);
    s_ctx.tx_count++;
}

/*
 * protocol_send_control
 * 发送控制帧 (CMD=0x01), payload = [vx_lo, vx_hi, vy_lo, vy_hi, wz_lo, wz_hi]
 */
void protocol_send_control(int16_t vx, int16_t vy, int16_t wz)
{
    uint8_t payload[PROTO_CONTROL_LEN];

    payload[0] = (uint8_t)(vx & 0xFF);
    payload[1] = (uint8_t)((vx >> 8) & 0xFF);
    payload[2] = (uint8_t)(vy & 0xFF);
    payload[3] = (uint8_t)((vy >> 8) & 0xFF);
    payload[4] = (uint8_t)(wz & 0xFF);
    payload[5] = (uint8_t)((wz >> 8) & 0xFF);

    protocol_send_raw(PROTO_CMD_CONTROL, payload, PROTO_CONTROL_LEN);
}

/*
 * protocol_send_heartbeat
 * 发送心跳帧 (CMD=0x02), payload = [status]
 */
void protocol_send_heartbeat(uint8_t status)
{
    protocol_send_raw(PROTO_CMD_HEARTBEAT, &status, PROTO_HEARTBEAT_LEN);
}

/*
 * protocol_send_mode_ack
 * 发送模式切换应答帧 (CMD=0x03), payload = [mode, result]
 */
void protocol_send_mode_ack(uint8_t mode, uint8_t result)
{
    uint8_t payload[2];
    payload[0] = mode;
    payload[1] = result;
    protocol_send_raw(PROTO_CMD_MODE_SET, payload, 2U);
}

/*
 * protocol_send_telemetry
 * 发送遥测帧 (CMD=0x06), 将遥测结构体序列化为小端字节流。
 */
void protocol_send_telemetry(const proto_telemetry_data_t *data)
{
    uint8_t payload[PROTO_TELEMETRY_LEN];

    if (data == 0)
    {
        return;
    }

    payload[0]  = (uint8_t)(data->wheel_speed_lf & 0xFF);
    payload[1]  = (uint8_t)((data->wheel_speed_lf >> 8) & 0xFF);
    payload[2]  = (uint8_t)(data->wheel_speed_rf & 0xFF);
    payload[3]  = (uint8_t)((data->wheel_speed_rf >> 8) & 0xFF);
    payload[4]  = (uint8_t)(data->wheel_speed_lb & 0xFF);
    payload[5]  = (uint8_t)((data->wheel_speed_lb >> 8) & 0xFF);
    payload[6]  = (uint8_t)(data->wheel_speed_rb & 0xFF);
    payload[7]  = (uint8_t)((data->wheel_speed_rb >> 8) & 0xFF);
    payload[8]  = (uint8_t)(data->vx_feedback & 0xFF);
    payload[9]  = (uint8_t)((data->vx_feedback >> 8) & 0xFF);
    payload[10] = (uint8_t)(data->vy_feedback & 0xFF);
    payload[11] = (uint8_t)((data->vy_feedback >> 8) & 0xFF);
    payload[12] = (uint8_t)(data->wz_feedback & 0xFF);
    payload[13] = (uint8_t)((data->wz_feedback >> 8) & 0xFF);
    payload[14] = data->mode;
    payload[15] = data->status;

    protocol_send_raw(PROTO_CMD_TELEMETRY, payload, PROTO_TELEMETRY_LEN);
}

/* ================================================================
 *  接收 / 解析
 * ================================================================ */

/*
 * protocol_poll
 * 主循环调用: 从无线串口 FIFO 中读出所有可用字节，逐字节喂入状态机。
 * 返回本次解析出的有效帧数。
 */
uint32_t protocol_poll(void)
{
    uint8_t  buf[32];
    uint32_t read_len;
    uint32_t i;
    uint32_t frame_count = 0U;

    /* 从无线串口 FIFO 批量读取 */
    read_len = wireless_uart_read_buffer(buf, sizeof(buf));

    for (i = 0U; i < read_len; ++i)
    {
        if (parser_feed(&s_parser, buf[i]))
        {
            frame_count++;
        }
    }

    return frame_count;
}

/*
 * protocol_tick
 * 在 PIT ISR (5ms) 中调用。
 * 1. 累加失控保护计数器。
 * 2. 若超过 PROTO_FAILSAFE_MS 则触发失控保护。
 */
void protocol_tick(void)
{
    /* 饱和累加，防止溢出后看门狗意外复位 */
    if (s_failsafe_tick < UINT32_MAX)
    {
        s_failsafe_tick += 5U;          /* 每次调用累加 5ms（与 PIT 周期一致） */
    }

    if (s_failsafe_tick >= PROTO_FAILSAFE_MS)
    {
        if (!s_failsafe_active)
        {
            s_failsafe_active = true;
            s_velocity.failsafe = true;
            s_velocity.valid    = false;    /* 断连后清除，便于显示判断 */
            s_heartbeat.valid   = false;
            s_velocity.vx = 0;
            s_velocity.vy = 0;
            s_velocity.wz = 0;
            s_ctx.failsafe_count++;
        }
    }
}

/* ================================================================
 *  查询接口
 * ================================================================ */

const proto_velocity_t *protocol_get_velocity(void)
{
    return &s_velocity;
}

const proto_heartbeat_t *protocol_get_heartbeat(void)
{
    return &s_heartbeat;
}

const proto_ctx_t *protocol_get_ctx(void)
{
    return &s_ctx;
}

/* ================================================================
 *  内部: 状态机解析器
 * ================================================================ */

static void parser_reset(parser_t *p)
{
    p->state        = PARSE_IDLE;
    p->cmd          = 0U;
    p->len          = 0U;
    p->payload_idx  = 0U;
    p->checksum_acc = 0U;
}

/*
 * parser_feed
 * 逐字节驱动状态机。解析成功返回 true，否则返回 false。
 *
 * 状态机流程:
 *   IDLE  -- 0x55 --> HEADER
 *   HEADER-- 0xAA --> CMD
 *   CMD   ----------> LEN
 *   LEN   ----------> PAYLOAD (若 len>0) 或 CHECKSUM (若 len==0)
 *   PAYLOAD --------> CHECKSUM (收满 len 字节)
 *   CHECKSUM -------> 校验比对 -> dispatch -> IDLE
 */
static bool parser_feed(parser_t *p, uint8_t byte)
{
    bool frame_ok = false;

    switch (p->state)
    {
    case PARSE_IDLE:
        if (byte == PROTO_HEADER_0)
        {
            p->checksum_acc = byte;
            p->state = PARSE_HEADER;
        }
        break;

    case PARSE_HEADER:
        if (byte == PROTO_HEADER_1)
        {
            p->checksum_acc += byte;
            p->state = PARSE_CMD;
        }
        else if (byte == PROTO_HEADER_0)
        {
            /* 连续 0x55，重新对齐 */
            p->checksum_acc = byte;
            /* 保持 PARSE_HEADER */
        }
        else
        {
            parser_reset(p);
        }
        break;

    case PARSE_CMD:
        p->cmd = byte;
        p->checksum_acc += byte;
        p->state = PARSE_LEN;
        break;

    case PARSE_LEN:
        p->len = byte;
        p->checksum_acc += byte;
        p->payload_idx = 0U;

        if (p->len > PROTO_PAYLOAD_MAX)
        {
            /* 非法长度，回退 */
            parser_reset(p);
        }
        else if (p->len == 0U)
        {
            p->state = PARSE_CHECKSUM;
        }
        else
        {
            p->state = PARSE_PAYLOAD;
        }
        break;

    case PARSE_PAYLOAD:
        p->payload[p->payload_idx] = byte;
        p->checksum_acc += byte;
        p->payload_idx++;

        if (p->payload_idx >= p->len)
        {
            p->state = PARSE_CHECKSUM;
        }
        break;

    case PARSE_CHECKSUM:
        if (byte == p->checksum_acc)
        {
            /* 校验通过 */
            dispatch_frame(p->cmd, p->payload, p->len);
            s_ctx.rx_good++;
            frame_ok = true;
        }
        else
        {
            s_ctx.rx_bad++;
        }
        parser_reset(p);
        break;

    default:
        parser_reset(p);
        break;
    }

    return frame_ok;
}

/* ================================================================
 *  内部: 帧分发
 * ================================================================ */

/*
 * dispatch_frame
 * 根据 CMD 将载荷数据写入对应结构体，并重置失控保护计时。
 * 如果有注册的外部回调，优先调用回调；否则使用内置默认处理。
 */
static void dispatch_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    /* 任何有效帧都重置失控看门狗和 failsafe 状态 */
    s_failsafe_tick   = 0U;
    s_failsafe_active = false;
    s_velocity.failsafe = false;
    s_velocity.valid    = true;

    /* 优先检查外部注册的回调处理函数 */
    if ((cmd < PROTO_CMD_MAX) && (s_handlers[cmd] != (proto_cmd_handler_fn)0))
    {
        s_handlers[cmd](cmd, payload, len);
        return;
    }

    /* 内置默认处理：仅处理 0x01 和 0x02 */
    switch (cmd)
    {
    case PROTO_CMD_CONTROL:
        if (len == PROTO_CONTROL_LEN)
        {
            s_velocity.vx = (int16_t)((uint16_t)payload[0] | ((uint16_t)payload[1] << 8));
            s_velocity.vy = (int16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8));
            s_velocity.wz = (int16_t)((uint16_t)payload[4] | ((uint16_t)payload[5] << 8));
            s_velocity.valid    = true;
            s_velocity.failsafe = false;
        }
        break;

    case PROTO_CMD_HEARTBEAT:
        if (len == PROTO_HEARTBEAT_LEN)
        {
            s_heartbeat.status = payload[0];
            s_heartbeat.valid  = true;
        }
        break;

    default:
        /* 未知命令字，忽略 */
        break;
    }
}
