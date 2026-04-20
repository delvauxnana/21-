#ifndef IMU_H
#define IMU_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 完成 IMU660RC 的初始化、零偏校准和周期更新。
 * 2. 使用 IMU660RC 内置四元数解算直接获取偏航角（无需手动积分）。
 * 3. 同时保留陀螺仪原始角速度供控制环使用。
 * 4. 四元数 (w, x, y, z) 中仅 w 和 z 用于提取偏航角。
 *
 * 偏航角约定: 从上方俯视，逆时针为正。
 */

typedef struct
{
    /* 四元数（IMU660RC 内置 SFLP 解算输出） */
    float quat_w;                /* 四元数 w 分量 */
    float quat_x;                /* 四元数 x 分量 */
    float quat_y;                /* 四元数 y 分量 */
    float quat_z;                /* 四元数 z 分量 */

    /* 偏航角（从四元数提取） */
    int32_t yaw_cdeg;            /* 当前偏航角，单位 0.01 deg */
    float yaw_deg;               /* 兼容显示用偏航角，单位 deg */

    /* Z 轴角速度（陀螺仪原始值） */
    int32_t gyro_z_cdeg_dps;     /* Z 轴角速度，单位 0.01 deg/s */
    float gyro_z_dps;            /* 兼容显示用角速度，单位 deg/s */

    float yaw_offset_deg;        /* 偏航角零点偏移（reset_yaw 时记录） */
    bool ready;                  /* IMU 是否初始化成功 */
    bool calibrated;             /* IMU 是否完成零偏校准 */
} imu_state_t;

void imu_init(void);
void imu_calibrate_zero(void);
void imu_reset_yaw(void);
void imu_update_5ms(void);
imu_state_t imu_get_state(void);

#endif /* IMU_H */
