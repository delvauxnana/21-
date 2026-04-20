#ifndef ODOMETRY_H
#define ODOMETRY_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 使用编码器与 IMU 融合更新当前位姿。
 * 2. 给位置环提供当前距离和车体速度反馈。
 */

void odometry_init(void);
void odometry_reset(void);
void odometry_update_20ms(void);
app_pose_t odometry_get_pose(void);
app_velocity_t odometry_get_body_velocity(void);
void odometry_correct_by_vision(const app_pose_t *vision_pose);

#endif /* ODOMETRY_H */
