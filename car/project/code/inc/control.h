#ifndef CONTROL_H
#define CONTROL_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 纯速度内环控制器：编码器采样 → 四轮增量式 PID → 电机输出。
 * 2. 上层通过 control_set_velocity_target() 下发车体速度命令。
 * 3. 位置环、距离移动等高级运动逻辑已迁移到 motion_manager 模块。
 */

/* 初始化速度控制器。 */
void control_init(void);

/* 20ms 周期更新: 编码器读取 → 逆运动学 → 四轮 PID → 电机输出。 */
void control_update_20ms(void);

/* 设置车体速度目标，由运动管理器调用。 */
void control_set_velocity_target(const app_velocity_t *target);

/* 直接设置四轮速度目标（绕过逆运动学），用于单轮测试。 */
void control_set_wheel_speed_direct(const app_wheel4_t *target_speed_mm_s);

/* 急停：清零所有 PID 状态和电机输出。 */
void control_emergency_stop(void);

/* ---- 查询接口 ---- */
app_velocity_t control_get_velocity_target(void);
app_velocity_t control_get_body_velocity_feedback(void);
app_wheel4_t   control_get_wheel_speed_target(void);
app_wheel4_t   control_get_wheel_speed_feedback(void);
app_wheel4_t   control_get_wheel_output(void);

#endif /* CONTROL_H */
