#ifndef PARAMETER_H
#define PARAMETER_H

#include "zf_common_headfile.h"
#include "pid.h"

/*
 * 文件作用:
 * 1. 集中管理速度环、位置环、保向和里程计相关参数。
 * 2. 让后续在线调参与参数固化有统一入口。
 */

typedef struct
{
    pid_t wheel_pid;                 /* 四轮速度内环，输出单位为 duty */
    pid_t position_pid;              /* 固定距离移动的位置环，输出单位为 mm/s */
    int32_t speed_limit;             /* 单轮速度目标上限，单位 mm/s */
    int32_t position_speed_limit;    /* 位置环输出的前向速度上限，单位 mm/s */
    int32_t position_ramp_step_mm_s; /* 每个 20ms 控制周期允许变化的速度步进，单位 mm/s */
    float yaw_hold_kp;               /* 航向保持比例系数，单位 deg/s per deg */
    float yaw_hold_limit_dps;        /* 航向保持最大修正角速度，单位 deg/s */
    float yaw_hold_deadband_deg;     /* 小角度误差死区，避免直线行驶时频繁左右修正 */
    float yaw_hold_gyro_damp;        /* 使用陀螺仪角速度做阻尼，减小保向来回摆动 */
    float yaw_control_sign;          /* 航向控制总符号，便于匹配 IMU 正方向与底盘旋转正方向 */
    float yaw_recover_enter_deg;     /* 偏航误差超过该阈值时，先进入原地回正模式 */
    float yaw_recover_exit_deg;      /* 回正完成后退出原地回正模式的误差阈值 */
    float yaw_recover_kp;            /* 原地回正时使用的角速度比例系数 */
    float yaw_recover_limit_dps;     /* 原地回正时允许的最大角速度 */
    float odo_scale_x;               /* X 方向里程计比例 */
    float odo_scale_y;               /* Y 方向里程计比例 */
    float odo_scale_yaw;             /* 航向角比例 */
} parameter_set_t;

void parameter_load_default(void);
void parameter_save(void);
parameter_set_t *parameter_get(void);

#endif /* PARAMETER_H */
