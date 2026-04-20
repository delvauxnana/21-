#include "cmd_dispatcher.h"
#include "protocol.h"
#include "motion_manager.h"
#include "control.h"
#include "parameter.h"
#include "pid.h"

/*
 * 文件作用:
 * 1. 实现各协议命令帧到业务逻辑的分发。
 * 2. 摇杆 ±1000 → 物理量映射集中在此处，主循环不再包含任何速度变换代码。
 * 3. 遥测帧构建：周期性读取控制器反馈并打包发送。
 */

/* ---- 内部函数声明 ---- */
static void on_control_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);
static void on_heartbeat_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);
static void on_mode_set_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);
static void on_param_set_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);
static void on_telemetry_req_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);

/* ================================================================
 *  初始化
 * ================================================================ */

void cmd_dispatcher_init(void)
{
    protocol_register_handler(PROTO_CMD_CONTROL,       on_control_frame);
    /* 注意: 不注册 PROTO_CMD_HEARTBEAT 回调，让 protocol.c 内置处理
     * 自动更新 s_heartbeat.valid，否则 LED 状态检测不到遥控连接。 */
    protocol_register_handler(PROTO_CMD_MODE_SET,      on_mode_set_frame);
    protocol_register_handler(PROTO_CMD_PARAM_SET,     on_param_set_frame);
    protocol_register_handler(PROTO_CMD_TELEMETRY_REQ, on_telemetry_req_frame);
}

/* ================================================================
 *  遥测发送
 * ================================================================ */

void cmd_dispatcher_send_telemetry(void)
{
    proto_telemetry_data_t telem;
    app_wheel4_t  ws = control_get_wheel_speed_feedback();
    app_velocity_t vf = control_get_body_velocity_feedback();

    telem.wheel_speed_lf = (int16_t)ws.lf;
    telem.wheel_speed_rf = (int16_t)ws.rf;
    telem.wheel_speed_lb = (int16_t)ws.lb;
    telem.wheel_speed_rb = (int16_t)ws.rb;
    telem.vx_feedback    = (int16_t)vf.vx;
    telem.vy_feedback    = (int16_t)vf.vy;
    telem.wz_feedback    = (int16_t)(vf.wz * 100.0f);  /* deg/s → 0.01 deg/s */
    telem.mode           = (uint8_t)motion_manager_get_mode();
    telem.status         = PROTO_STATUS_NORMAL;

    protocol_send_telemetry(&telem);
}

void cmd_dispatcher_send_heartbeat(void)
{
    const proto_velocity_t *v = protocol_get_velocity();
    protocol_send_heartbeat(v->failsafe ?
        PROTO_STATUS_FAILSAFE : PROTO_STATUS_NORMAL);
}

/* ================================================================
 *  命令帧处理回调
 * ================================================================ */

/*
 * 0x01 控制帧：解析 vx/vy/wz (±1000) → 转换为物理量 → 写入 motion_manager
 */
static void on_control_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    int16_t raw_vx;
    int16_t raw_vy;
    int16_t raw_wz;
    app_velocity_t vel_cmd;

    (void)cmd;

    if (len != PROTO_CONTROL_LEN)
    {
        return;
    }

    raw_vx = (int16_t)((uint16_t)payload[0] | ((uint16_t)payload[1] << 8));
    raw_vy = (int16_t)((uint16_t)payload[2] | ((uint16_t)payload[3] << 8));
    raw_wz = (int16_t)((uint16_t)payload[4] | ((uint16_t)payload[5] << 8));

    /* 摇杆 ±1000 → 物理量 (mm/s, deg/s)
     * 极性修正:
     *   VY 取反: 车体前方 = 运动学 -Y
     *   WZ 取反: 用户推右期望顺时针 = 运动学逆时针
     *   VX 不取反: O-config 麦轮滚子排列
     */
    vel_cmd.vx = (float)raw_vx * CMD_VX_GAIN * CMD_VX_SIGN;
    vel_cmd.vy = (float)raw_vy * CMD_VY_GAIN * CMD_VY_SIGN;
    vel_cmd.wz = (float)raw_wz * CMD_WZ_GAIN * CMD_WZ_SIGN;

    motion_manager_set_remote_cmd(&vel_cmd);
}

/*
 * 0x02 心跳帧：更新心跳状态
 */
static void on_heartbeat_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    (void)cmd;

    /* 外部回调会抢占 protocol.c 的内置处理，
     * 所以必须在这里手动解析心跳并更新状态。 */
    if (len >= PROTO_HEARTBEAT_LEN)
    {
        /* 通过 protocol 内置接口无法直接写 s_heartbeat，
         * 这里重新调用内置逻辑：手动更新心跳字段。
         * 由于 s_heartbeat 是 protocol.c 内部 static 变量，
         * 我们改为：不注册此回调，让 protocol.c 内置处理生效。
         * → 见 cmd_dispatcher_init() 修改。 */
    }
}

/*
 * 0x03 模式切换帧：切换运动模式
 */
static void on_mode_set_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t  requested_mode;
    bool     ok;

    (void)cmd;

    if (len < PROTO_MODE_SET_LEN)
    {
        return;
    }

    requested_mode = payload[0];

    if (requested_mode >= (uint8_t)MOTION_MODE_MAX)
    {
        protocol_send_mode_ack(requested_mode, 0x00U);  /* 0x00 = 失败 */
        return;
    }

    ok = motion_manager_set_mode((motion_mode_t)requested_mode);
    protocol_send_mode_ack(requested_mode, ok ? 0x01U : 0x00U);
}

/*
 * 0x04 参数下发帧：热更新单个参数
 * payload: [param_id(1B)] [value_i32(4B, LE)]
 */
static void on_param_set_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t param_id;
    int32_t value;
    parameter_set_t *param;

    (void)cmd;

    if (len < PROTO_PARAM_SET_LEN)
    {
        return;
    }

    param_id = payload[0];
    value = (int32_t)((uint32_t)payload[1]
                    | ((uint32_t)payload[2] << 8)
                    | ((uint32_t)payload[3] << 16)
                    | ((uint32_t)payload[4] << 24));

    param = parameter_get();

    switch (param_id)
    {
        case 0x01: param->wheel_pid.kp             = value; break;
        case 0x02: param->wheel_pid.ki             = value; break;
        case 0x03: param->wheel_pid.kd             = value; break;
        case 0x04: param->wheel_pid.output_limit   = value; break;
        case 0x05: param->speed_limit              = value; break;
        case 0x06: param->position_pid.kp          = value; break;
        case 0x07: param->position_pid.ki          = value; break;
        case 0x08: param->position_pid.kd          = value; break;
        case 0x09: param->position_speed_limit     = value; break;
        default:
            /* 未知参数 ID，忽略 */
            break;
    }
}

/*
 * 0x05 遥测请求帧：立即回传一帧遥测
 */
static void on_telemetry_req_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    (void)cmd;
    (void)payload;
    (void)len;

    cmd_dispatcher_send_telemetry();
}
