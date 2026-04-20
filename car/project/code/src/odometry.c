#include "odometry.h"
#include "encoder.h"
#include "imu.h"
#include "parameter.h"
#include <math.h>

#define ODOMETRY_PI     (3.1415926f)

static app_pose_t g_pose;
static app_velocity_t g_body_velocity;

/* 初始化里程计状态。 */
void odometry_init(void)
{
    g_pose = (app_pose_t){0.0f, 0.0f, 0.0f};
    g_body_velocity = (app_velocity_t){0.0f, 0.0f, 0.0f};
}

/* 将当前位置和速度反馈重新清零。 */
void odometry_reset(void)
{
    g_pose = (app_pose_t){0.0f, 0.0f, 0.0f};
    g_body_velocity = (app_velocity_t){0.0f, 0.0f, 0.0f};
}

/* 使用编码器位移和 IMU 航向更新当前位姿。 */
void odometry_update_20ms(void)
{
    parameter_set_t *parameter = parameter_get();
    app_wheel4_t wheel_delta_x100 = encoder_get_delta();
    app_wheel4_t wheel_speed = encoder_get_speed();
    imu_state_t imu_state = imu_get_state();
    float lf_delta_mm = (float)wheel_delta_x100.lf * 0.01f;
    float rf_delta_mm = (float)wheel_delta_x100.rf * 0.01f;
    float lb_delta_mm = (float)wheel_delta_x100.lb * 0.01f;
    float rb_delta_mm = (float)wheel_delta_x100.rb * 0.01f;
    float local_dx_mm;
    float local_dy_mm;
    float yaw_rad;

    /* 仅使用编码器估计平移位移，旋转量统一采用 IMU 提供的航向。 */
    local_dx_mm = (lf_delta_mm - rf_delta_mm - lb_delta_mm + rb_delta_mm) * 0.25f;
    local_dy_mm = (lf_delta_mm + rf_delta_mm + lb_delta_mm + rb_delta_mm) * 0.25f;

    yaw_rad = ((float)imu_state.yaw_cdeg * 0.01f) * ODOMETRY_PI / 180.0f;

    g_pose.x_mm += (local_dx_mm * cosf(yaw_rad) - local_dy_mm * sinf(yaw_rad)) * parameter->odo_scale_x;
    g_pose.y_mm += (local_dx_mm * sinf(yaw_rad) + local_dy_mm * cosf(yaw_rad)) * parameter->odo_scale_y;
    g_pose.yaw_deg = ((float)imu_state.yaw_cdeg * 0.01f) * parameter->odo_scale_yaw;

    g_body_velocity.vx = ((float)wheel_speed.lf - (float)wheel_speed.rf - (float)wheel_speed.lb + (float)wheel_speed.rb) * 0.25f;
    g_body_velocity.vy = ((float)wheel_speed.lf + (float)wheel_speed.rf + (float)wheel_speed.lb + (float)wheel_speed.rb) * 0.25f;
    g_body_velocity.wz = (float)imu_state.gyro_z_cdeg_dps * 0.01f;
}

/* 返回当前位姿。 */
app_pose_t odometry_get_pose(void)
{
    return g_pose;
}

/* 返回当前车体速度反馈。 */
app_velocity_t odometry_get_body_velocity(void)
{
    return g_body_velocity;
}

/* 预留视觉校正接口。 */
void odometry_correct_by_vision(const app_pose_t *vision_pose)
{
    if (vision_pose == 0)
    {
        return;
    }

    g_pose = *vision_pose;
}
