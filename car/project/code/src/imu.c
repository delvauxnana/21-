#include "imu.h"
#include "board_config.h"
#include <math.h>

/*
 * 文件作用：
 * 1. 使用 IMU660RC 内置 SFLP 四元数解算获取偏航角。
 * 2. 四元数和陀螺仪数据由 imu660rc_callback()（INT2 EXTI 触发）自动采集，
 *    本模块只需读取全局变量 imu660rc_quarternion[] 和 imu660rc_gyro_z。
 * 3. imu_update_5ms() 在 PIT ISR 中调用，用于从全局变量提取偏航角和角速度。
 *
 * 偏航角约定: 从上方俯视，逆时针为正方向。
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static imu_state_t g_imu_state;
static int32_t g_gyro_z_bias_cdeg_dps = 0;

/* 将浮点角速度换算为 0.01deg/s */
static int32_t imu_float_to_cdeg(float value)
{
    if (value >= 0.0f)
    {
        return (int32_t)(value * 100.0f + 0.5f);
    }

    return (int32_t)(value * 100.0f - 0.5f);
}

/* 将角度约束到 [-180.00, 180.00) 度 */
static int32_t imu_wrap_angle_cdeg(int32_t angle_cdeg)
{
    while (angle_cdeg >= 18000)
    {
        angle_cdeg -= 36000;
    }

    while (angle_cdeg < -18000)
    {
        angle_cdeg += 36000;
    }

    return angle_cdeg;
}

/* 将角度约束到 [-180, 180) 度（float） */
static float imu_wrap_angle_deg(float angle)
{
    while (angle >= 180.0f)
    {
        angle -= 360.0f;
    }

    while (angle < -180.0f)
    {
        angle += 360.0f;
    }

    return angle;
}

/* 从四元数提取偏航角（Z 轴旋转角度），返回 deg
 * yaw = atan2(2*(w*z + x*y), 1 - 2*(y^2 + z^2))
 * 取反: IMU660RC 硬件顺时针正，我们约定逆时针正 */
static float quaternion_to_yaw_deg(float w, float x, float y, float z)
{
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    float yaw_rad   = atan2f(siny_cosp, cosy_cosp);

    return -(yaw_rad * 180.0f / M_PI);
}

/* 根据主链路同步浮点显示量 */
static void imu_sync_float_state(void)
{
    g_imu_state.yaw_deg    = (float)g_imu_state.yaw_cdeg * 0.01f;
    g_imu_state.gyro_z_dps = (float)g_imu_state.gyro_z_cdeg_dps * 0.01f;
}

/* 初始化 IMU660RC，启用四元数输出 */
void imu_init(void)
{
    g_imu_state = (imu_state_t){0};
    g_gyro_z_bias_cdeg_dps = 0;

    if (0U == imu660rc_init(IMU660RC_QUARTERNION_240HZ))
    {
        g_imu_state.ready = true;
    }
}

/* 静止采样估计 Z 轴陀螺仪零偏 */
void imu_calibrate_zero(void)
{
    uint16_t i;
    int64_t gyro_sum_cdeg_dps = 0;

    if (!g_imu_state.ready)
    {
        return;
    }

    g_gyro_z_bias_cdeg_dps = 0;
    g_imu_state.yaw_cdeg = 0;
    g_imu_state.gyro_z_cdeg_dps = 0;
    g_imu_state.yaw_offset_deg = 0.0f;
    imu_sync_float_state();
    g_imu_state.calibrated = false;

    for (i = 0U; i < BOARD_IMU_CALIBRATE_SAMPLES; ++i)
    {
        imu660rc_get_gyro();
        gyro_sum_cdeg_dps += imu_float_to_cdeg(
            imu660rc_gyro_transition(imu660rc_gyro_z));
        system_delay_ms(BOARD_IMU_PERIOD_MS);
    }

    g_gyro_z_bias_cdeg_dps = (int32_t)(gyro_sum_cdeg_dps / BOARD_IMU_CALIBRATE_SAMPLES);

    imu660rc_get_quarternion();
    g_imu_state.yaw_offset_deg = quaternion_to_yaw_deg(
        imu660rc_quarternion[0],
        imu660rc_quarternion[1],
        imu660rc_quarternion[2],
        imu660rc_quarternion[3]);

    g_imu_state.calibrated = true;
}

/* 将当前偏航角重新清零 */
void imu_reset_yaw(void)
{
    imu660rc_get_quarternion();
    g_imu_state.yaw_offset_deg = quaternion_to_yaw_deg(
        imu660rc_quarternion[0],
        imu660rc_quarternion[1],
        imu660rc_quarternion[2],
        imu660rc_quarternion[3]);

    g_imu_state.yaw_cdeg = 0;
    imu_sync_float_state();
}

/* 5ms 周期更新: 读取四元数和陀螺仪 */
void imu_update_5ms(void)
{
    int32_t gyro_z_cdeg_dps;
    int32_t deadband_cdeg_dps;
    float raw_yaw_deg;
    float rel_yaw_deg;

    if (!g_imu_state.ready)
    {
        return;
    }

    /* 直接读取 imu660rc_callback() 在 INT2 中断中已更新的全局变量
     * 取反: 与偏航角方向一致（逆时针为正） */
    gyro_z_cdeg_dps = -(imu_float_to_cdeg(
        imu660rc_gyro_transition(imu660rc_gyro_z))
        - g_gyro_z_bias_cdeg_dps);

    deadband_cdeg_dps = imu_float_to_cdeg(BOARD_IMU_GYRO_DEADBAND_DPS);
    if (func_abs(gyro_z_cdeg_dps) < deadband_cdeg_dps)
    {
        gyro_z_cdeg_dps = 0;
    }

    g_imu_state.gyro_z_cdeg_dps = gyro_z_cdeg_dps;

    /* 读取四元数（已由 imu660rc_callback 在 INT2 中断中更新） */
    g_imu_state.quat_w = imu660rc_quarternion[0];
    g_imu_state.quat_x = imu660rc_quarternion[1];
    g_imu_state.quat_y = imu660rc_quarternion[2];
    g_imu_state.quat_z = imu660rc_quarternion[3];

    raw_yaw_deg = quaternion_to_yaw_deg(
        g_imu_state.quat_w,
        g_imu_state.quat_x,
        g_imu_state.quat_y,
        g_imu_state.quat_z);

    /* 减去零点偏移，得到相对偏航角 */
    rel_yaw_deg = imu_wrap_angle_deg(raw_yaw_deg - g_imu_state.yaw_offset_deg);

    g_imu_state.yaw_cdeg = imu_wrap_angle_cdeg(imu_float_to_cdeg(rel_yaw_deg));
    imu_sync_float_state();
}

/* 返回当前 IMU 状态快照 */
imu_state_t imu_get_state(void)
{
    return g_imu_state;
}
