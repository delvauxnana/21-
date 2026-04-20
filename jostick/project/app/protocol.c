/*===========================================================================
 * protocol.c
 * 无线串口通信协议解析器 + 麦轮底盘逆运动学
 *
 * 数据流：
 *   LPUART8 ISR → wireless_uart_callback() → FIFO
 *       ↓ (主循环)
 *   protocol_poll() → 逐字节状态机解析 → velocity / heartbeat
 *       ↓
 *   protocol_mecanum_inverse() → 四轮速度
 *
 * 失控保护：
 *   protocol_tick() 每 10ms 被 PIT ISR 调用，递增看门狗计数。
 *   protocol_poll() 在成功解析帧后重置计数。
 *   当计数超过 PROTO_FAILSAFE_MS → vx/vy/wz 和四轮速度强制清零。
 *===========================================================================*/

#include "zf_common_headfile.h"
#include "protocol.h"
#include "zf_device_wireless_uart.h"
#include "board_input.h"

/*===========================================================================
 * 模块内部状态（单例）
 *===========================================================================*/

static proto_ctx_t s_ctx;
static volatile uint32_t s_heartbeat_tx_cnt = 0;  /* 发送计时（ms，ISR 递增） */
static uint8_t s_heartbeat_divider = 0;              /* 心跳穿插计数器 */
#define PROTO_TX_INTERVAL  (50)                     /* 每 50ms 发送一次控制帧（20Hz） */

/*===========================================================================
 * 内部工具
 *===========================================================================*/

/* 计算 Sum Check：buf[0..len-1] 所有字节累加取低 8 位 */
static uint8 proto_calc_checksum(const uint8 *buf, uint8 len)
{
    uint16 sum = 0;
    for (uint8 i = 0; i < len; i++) {
        sum += buf[i];
    }
    return (uint8)(sum & 0xFF);
}

/* 从 2 字节小端数据中提取 int16_t */
static int16_t proto_read_i16_le(const uint8 *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* 将 velocity 清零（失控保护或初始化） */
static void proto_clear_velocity(void)
{
    s_ctx.velocity.vx = 0;
    s_ctx.velocity.vy = 0;
    s_ctx.velocity.wz = 0;
    s_ctx.wheels.fl  = 0;
    s_ctx.wheels.fr  = 0;
    s_ctx.wheels.rl  = 0;
    s_ctx.wheels.rr  = 0;
}

/* 重置看门狗（收到有效帧时调用） */
static void proto_feed_watchdog(void)
{
    s_ctx.watchdog_cnt   = 0;
    s_ctx.failsafe_active = false;
    s_ctx.heartbeat.connected = true;
}

/* 状态机复位到 IDLE（帧出错或处理完毕后调用） */
static void proto_reset_parser(void)
{
    s_ctx.state       = PROTO_STATE_IDLE;
    s_ctx.frame_idx   = 0;
    s_ctx.payload_cnt = 0;
    s_ctx.expected_payload_len = 0;
}

/*===========================================================================
 * 帧处理（已通过校验的完整帧）
 *===========================================================================*/

static void proto_on_frame_received(void)
{
    /* frame_buf 布局：[H0 H1 CMD LEN PAYLOAD... CHECKSUM]
     * frame_idx 指向最后一个字节（checksum）之后 */
    uint8 cmd = s_ctx.frame_buf[2];
    uint8 len = s_ctx.frame_buf[3];
    const uint8 *payload = &s_ctx.frame_buf[4];

    /* 任何校验通过的帧都应重置看门狗（包括车端发来的遥测帧 CMD=0x06） */
    proto_feed_watchdog();
    s_ctx.rx_frame_cnt++;

    switch (cmd) {
    case PROTO_CMD_CONTROL:
        if (len == PROTO_CONTROL_LEN) {
            s_ctx.velocity.vx = proto_read_i16_le(&payload[0]);
            s_ctx.velocity.vy = proto_read_i16_le(&payload[2]);
            s_ctx.velocity.wz = proto_read_i16_le(&payload[4]);

            /* 立即计算四轮速度 */
            protocol_mecanum_inverse(s_ctx.velocity.vx,
                                     s_ctx.velocity.vy,
                                     s_ctx.velocity.wz,
                                     &s_ctx.wheels);
        } else {
            s_ctx.rx_err_cnt++;
        }
        break;

    case PROTO_CMD_HEARTBEAT:
        if (len == PROTO_HEARTBEAT_LEN) {
            s_ctx.heartbeat.remote_status = payload[0];
        } else {
            s_ctx.rx_err_cnt++;
        }
        break;

    default:
        /* 未知命令字（如车端遥测 0x06）：静默忽略，不计入错误 */
        break;
    }
}

/*===========================================================================
 * 状态机：逐字节非阻塞解析
 *===========================================================================*/

static void proto_parse_byte(uint8 byte)
{
    switch (s_ctx.state) {

    /*--- IDLE：等待帧头第 1 字节 ---*/
    case PROTO_STATE_IDLE:
        if (byte == PROTO_HEADER_0) {
            s_ctx.frame_buf[0] = byte;
            s_ctx.frame_idx    = 1;
            s_ctx.state        = PROTO_STATE_HEADER;
        }
        /* 非 0x55 → 丢弃，继续等 */
        break;

    /*--- HEADER：等待帧头第 2 字节 ---*/
    case PROTO_STATE_HEADER:
        if (byte == PROTO_HEADER_1) {
            s_ctx.frame_buf[1] = byte;
            s_ctx.frame_idx    = 2;
            s_ctx.state        = PROTO_STATE_CMD;
        } else if (byte == PROTO_HEADER_0) {
            /* 连续 0x55，保持在 HEADER 状态重新匹配 */
            s_ctx.frame_buf[0] = byte;
            s_ctx.frame_idx    = 1;
        } else {
            /* 不匹配，回到 IDLE */
            proto_reset_parser();
        }
        break;

    /*--- CMD：接收命令字，根据命令字确定负载长度 ---*/
    case PROTO_STATE_CMD:
        s_ctx.frame_buf[2] = byte;
        s_ctx.frame_idx    = 3;
        s_ctx.state        = PROTO_STATE_LEN;
        break;

    /*--- LEN：接收负载长度字节 ---*/
    case PROTO_STATE_LEN:
        s_ctx.frame_buf[3] = byte;
        s_ctx.frame_idx    = 4;
        s_ctx.expected_payload_len = byte;
        s_ctx.payload_cnt  = 0;

        /* 长度合法性检查：避免缓冲区溢出 */
        if (byte == 0) {
            /* 无负载，直接等校验和 */
            s_ctx.state = PROTO_STATE_CHECKSUM;
        } else if (byte > (PROTO_FRAME_MAX_SIZE - 5)) {
            /* 负载过长（帧头2 + cmd1 + len1 + payload + checksum1） */
            s_ctx.rx_err_cnt++;
            proto_reset_parser();
        } else {
            s_ctx.state = PROTO_STATE_PAYLOAD;
        }
        break;

    /*--- PAYLOAD：累积接收负载字节 ---*/
    case PROTO_STATE_PAYLOAD:
        s_ctx.frame_buf[s_ctx.frame_idx] = byte;
        s_ctx.frame_idx++;
        s_ctx.payload_cnt++;

        if (s_ctx.payload_cnt >= s_ctx.expected_payload_len) {
            /* 负载接收完毕，等最后的校验字节 */
            s_ctx.state = PROTO_STATE_CHECKSUM;
        }
        break;

    /*--- CHECKSUM：校验和验证 ---*/
    case PROTO_STATE_CHECKSUM:
    {
        /* 校验范围：frame_buf[0 .. frame_idx-1]（帧头到最后一个负载字节） */
        uint8 expected = proto_calc_checksum(s_ctx.frame_buf, s_ctx.frame_idx);

        if (byte == expected) {
            /* 校验通过，处理帧 */
            s_ctx.frame_buf[s_ctx.frame_idx] = byte;
            s_ctx.frame_idx++;
            proto_on_frame_received();
        } else {
            s_ctx.rx_err_cnt++;
        }

        /* 无论成功/失败，解析器回到 IDLE */
        proto_reset_parser();
        break;
    }

    default:
        proto_reset_parser();
        break;
    }
}

/*===========================================================================
 * 麦轮逆运动学
 *
 * 标准 X 型麦克纳姆轮布局（俯视图，滚子方向 X 交叉）：
 *
 *     FL ╲   ╱ FR         坐标系：x = 前进方向
 *         ╲ ╱                      y = 左方
 *         ╱ ╲                      wz = 逆时针为正
 *     RL ╱   ╲ RR
 *
 * 逆运动学公式（简化版，wz 与 vx/vy 同量纲直接混合）：
 *   FL = vx - vy - wz
 *   FR = vx + vy + wz
 *   RL = vx + vy - wz
 *   RR = vx - vy + wz
 *===========================================================================*/

void protocol_mecanum_inverse(int16_t vx, int16_t vy, int16_t wz,
                              proto_wheel_speed_t *out)
{
    if (out == NULL) return;

    int32_t fl = (int32_t)vx - (int32_t)vy - (int32_t)wz;
    int32_t fr = (int32_t)vx + (int32_t)vy + (int32_t)wz;
    int32_t rl = (int32_t)vx + (int32_t)vy - (int32_t)wz;
    int32_t rr = (int32_t)vx - (int32_t)vy + (int32_t)wz;

    /* 钳位到 int16_t 范围，防止溢出 */
    if (fl >  32767) fl =  32767;
    if (fl < -32768) fl = -32768;
    if (fr >  32767) fr =  32767;
    if (fr < -32768) fr = -32768;
    if (rl >  32767) rl =  32767;
    if (rl < -32768) rl = -32768;
    if (rr >  32767) rr =  32767;
    if (rr < -32768) rr = -32768;

    out->fl = (int16_t)fl;
    out->fr = (int16_t)fr;
    out->rl = (int16_t)rl;
    out->rr = (int16_t)rr;
}

/*===========================================================================
 * 公共接口实现
 *===========================================================================*/

void protocol_init(void)
{
    /* 清空上下文 */
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.heartbeat.connected = false;
    s_ctx.failsafe_active     = true;    /* 初始即为失控保护状态 */

    /* 初始化无线串口硬件已在 main.c 中完成（wireless_uart_init()）。
     * 此处不再重复调用，避免多次初始化导致 FIFO 被清空。 */
}

uint32_t protocol_poll(void)
{
    uint32_t parsed_frames = 0;
    uint8    rx_buf[32];    /* 每次最多取 32 字节，避免一次取太多阻塞主循环 */
    uint32_t rx_len;

    /*--- 1. 失控保护检查 ---*/
    if (s_ctx.watchdog_cnt >= PROTO_FAILSAFE_MS) {
        if (!s_ctx.failsafe_active) {
            s_ctx.failsafe_active     = true;
            s_ctx.heartbeat.connected = false;
            proto_clear_velocity();
        }
    }

    /*--- 2. 从 wireless_uart FIFO 取出可用字节 ---*/
    rx_len = wireless_uart_read_buffer(rx_buf, sizeof(rx_buf));

    /*--- 3. 逐字节送入状态机（有数据时才解析） ---*/
    if (rx_len > 0) {
        uint32_t old_cnt = s_ctx.rx_frame_cnt;
        for (uint32_t i = 0; i < rx_len; i++) {
            proto_parse_byte(rx_buf[i]);
        }
        parsed_frames = s_ctx.rx_frame_cnt - old_cnt;
    }

    /*--- 4. 周期性发送控制帧（摇杆数据 → vx/vy/wz） ---*/
    if (s_heartbeat_tx_cnt >= PROTO_TX_INTERVAL) {
        s_heartbeat_tx_cnt = 0;

        /* 仅在摇杆模式页面发送真实摇杆数据，其他页面发送零速 */
        if (board_input_get_active_page() == PAGE_JOYSTICK_MODE) {
            int16_t lx, ly, rx, ry;
            board_input_get_stick_data(&lx, &ly, &rx, &ry);

            /* 极性与屏幕显示一致（屏幕显示 lx, -ly）：
             *   vx = lx     左摇杆 X→前进后退
             *   vy = -ly    左摇杆 Y（取反）→横移
             *   wz = rx     右摇杆 X→旋转 */
            protocol_send_control(lx, (int16_t)(-ly), rx);
        } else {
            /* 非摇杆页面：发送零速保持链路活跃，车端停车 */
            protocol_send_control(0, 0, 0);
        }

        /* 每 4 次控制帧（约 200ms）穿插发送 1 次心跳帧 */
        s_heartbeat_divider++;
        if (s_heartbeat_divider >= 4) {
            s_heartbeat_divider = 0;
            protocol_send_heartbeat(0x01);  /* PROTO_STATUS_NORMAL */
        }
    }

    return parsed_frames;
}

void protocol_tick(void)
{
    /* 每次调用递增 10ms（由 PIT 10ms ISR 调用）。
     * 使用饱和加法，防止溢出后看门狗意外复位。 */
    if (s_ctx.watchdog_cnt < UINT32_MAX) {
        s_ctx.watchdog_cnt += 10;
    }
    /* 心跳发送计时 */
    if (s_heartbeat_tx_cnt < UINT32_MAX) {
        s_heartbeat_tx_cnt += 10;
    }
}

const proto_wheel_speed_t *protocol_get_wheel_speeds(void)
{
    return &s_ctx.wheels;
}

const proto_velocity_t *protocol_get_velocity(void)
{
    return &s_ctx.velocity;
}

const proto_heartbeat_t *protocol_get_heartbeat(void)
{
    return &s_ctx.heartbeat;
}

const proto_ctx_t *protocol_get_ctx(void)
{
    return &s_ctx;
}

/*===========================================================================
 * 发送接口实现
 *===========================================================================*/

/* 将 int16_t 写入 2 字节小端缓冲 */
static void proto_write_i16_le(uint8 *p, int16_t val)
{
    uint16_t u = (uint16_t)val;
    p[0] = (uint8)(u & 0xFF);
    p[1] = (uint8)(u >> 8);
}

static void proto_write_i32_le(uint8 *p, int32_t val)
{
    uint32_t u = (uint32_t)val;
    p[0] = (uint8)(u & 0xFF);
    p[1] = (uint8)((u >> 8) & 0xFF);
    p[2] = (uint8)((u >> 16) & 0xFF);
    p[3] = (uint8)((u >> 24) & 0xFF);
}

void protocol_send_raw(uint8 cmd, const uint8 *payload, uint8 len)
{
    uint8 tx_buf[PROTO_FRAME_MAX_SIZE];
    uint8 idx = 0;

    /* 帧头 */
    tx_buf[idx++] = PROTO_HEADER_0;     /* 0x55 */
    tx_buf[idx++] = PROTO_HEADER_1;     /* 0xAA */

    /* 命令字 + 长度 */
    tx_buf[idx++] = cmd;
    tx_buf[idx++] = len;

    /* 负载 */
    for (uint8 i = 0; i < len && idx < (PROTO_FRAME_MAX_SIZE - 1); i++) {
        tx_buf[idx++] = payload[i];
    }

    /* 校验和：从 tx_buf[0] 到 tx_buf[idx-1] 的累加 */
    tx_buf[idx] = proto_calc_checksum(tx_buf, idx);
    idx++;

    /* 通过无线串口发送完整帧 */
    wireless_uart_send_buffer(tx_buf, idx);
}

void protocol_send_heartbeat(uint8 status)
{
    protocol_send_raw(PROTO_CMD_HEARTBEAT, &status, 1);
}

void protocol_send_control(int16_t vx, int16_t vy, int16_t wz)
{
    uint8 payload[6];
    proto_write_i16_le(&payload[0], vx);
    proto_write_i16_le(&payload[2], vy);
    proto_write_i16_le(&payload[4], wz);
    protocol_send_raw(PROTO_CMD_CONTROL, payload, 6);
}

void protocol_send_param_set(uint8 param_id, int32_t value)
{
    uint8 payload[PROTO_PARAM_SET_LEN];

    payload[0] = param_id;
    proto_write_i32_le(&payload[1], value);
    protocol_send_raw(PROTO_CMD_PARAM_SET, payload, PROTO_PARAM_SET_LEN);
}
