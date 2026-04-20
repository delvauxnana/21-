#include "parameter.h"

static parameter_set_t g_parameter_set;

/*
 * 文件作用:
 * 1. 保存整车默认参数。
 * 2. 当前阶段优先提供固定距离移动测试所需的一组稳定默认值。
 */

/* 加载一组适合“位置环套速度环”测试的默认参数。 */
void parameter_load_default(void)
{
    g_parameter_set.wheel_pid.kp = 2200;          /* 2.2 */
    g_parameter_set.wheel_pid.ki = 120;           /* 0.12 */
    g_parameter_set.wheel_pid.kd = 0;
    g_parameter_set.wheel_pid.integral = 0;
    g_parameter_set.wheel_pid.last_error = 0;
    g_parameter_set.wheel_pid.prev_error = 0;
    g_parameter_set.wheel_pid.output = 0;
    g_parameter_set.wheel_pid.integral_limit = 600;
    g_parameter_set.wheel_pid.output_limit = 5200;

    g_parameter_set.position_pid.kp = 900;        /* 0.9 */
    g_parameter_set.position_pid.ki = 0;
    g_parameter_set.position_pid.kd = 0;
    g_parameter_set.position_pid.integral = 0;
    g_parameter_set.position_pid.last_error = 0;
    g_parameter_set.position_pid.prev_error = 0;
    g_parameter_set.position_pid.output = 0;
    g_parameter_set.position_pid.integral_limit = 1200;
    g_parameter_set.position_pid.output_limit = 320;

    g_parameter_set.speed_limit = 420;
    g_parameter_set.position_speed_limit = 320;
    g_parameter_set.position_ramp_step_mm_s = 40;
    g_parameter_set.yaw_hold_kp = 2.0f;
    g_parameter_set.yaw_hold_limit_dps = 24.0f;
    g_parameter_set.yaw_hold_deadband_deg = 0.5f;
    g_parameter_set.yaw_hold_gyro_damp = 0.80f;
    g_parameter_set.yaw_control_sign = -1.0f;
    g_parameter_set.yaw_recover_enter_deg = 12.0f;
    g_parameter_set.yaw_recover_exit_deg = 4.0f;
    g_parameter_set.yaw_recover_kp = 6.0f;
    g_parameter_set.yaw_recover_limit_dps = 80.0f;

    g_parameter_set.odo_scale_x = 1.0f;
    g_parameter_set.odo_scale_y = 1.0f;
    g_parameter_set.odo_scale_yaw = 1.0f;
}

/* 预留参数保存接口，后续可接入 Flash。 */
void parameter_save(void)
{
    /* TODO: 将参数保存到片内或片外非易失存储。 */
}

/* 返回全局参数表地址。 */
parameter_set_t *parameter_get(void)
{
    return &g_parameter_set;
}
