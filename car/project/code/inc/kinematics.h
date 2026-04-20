#ifndef KINEMATICS_H
#define KINEMATICS_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 保存麦克纳姆底盘的正逆运动学。
 * 2. 负责在车体速度与四轮线速度之间做统一换算。
 */

void kinematics_inverse(const app_velocity_t *body_ref, app_wheel4_t *wheel_ref);
void kinematics_forward(const app_wheel4_t *wheel_meas, app_velocity_t *body_meas);

#endif /* KINEMATICS_H */
