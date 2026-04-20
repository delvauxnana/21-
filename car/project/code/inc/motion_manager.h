#ifndef MOTION_MANAGER_H
#define MOTION_MANAGER_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include <stdint.h>
#include "app_types.h"
#include "control.h"

/*
 * 文件作用:
 * 1. 统一管理车体运动模式状态机，将零散的模式切换逻辑集中到一个模块。
 * 2. 在各模式下生成 app_velocity_t 目标，传递给速度控制器 (control)。
 * 3. 模式切换时自动执行安全停车过渡，避免电机突变。
 */

/* ---- 运动模式枚举 ---- */
typedef enum
{
    MOTION_MODE_IDLE = 0,     /* 停车 —— 电机零输出                          */
    MOTION_MODE_REMOTE,       /* 遥控模式 —— 使用无线下发的速度命令           */
    MOTION_MODE_AUTO,         /* 自动导航 —— 位置环 + 航向保持（预留）        */
    MOTION_MODE_TEST_WHEEL,   /* 单轮恒速测试                                */
    MOTION_MODE_TEST_MOVE,    /* 距离移动测试 —— 位置环套速度环               */
    MOTION_MODE_MAX
} motion_mode_t;

/* ---- 距离移动测试轴向枚举 ---- */
typedef enum
{
    MOTION_TEST_AXIS_X = 0,
    MOTION_TEST_AXIS_Y,
    MOTION_TEST_AXIS_Z
} motion_test_axis_t;

/* ---- 运动管理器状态快照 ---- */
typedef struct
{
    motion_mode_t mode;
    bool          move_active;
    bool          move_finished;
    float         move_target;
    float         move_current;
} motion_status_t;

/* ---- 初始化 ---- */
void motion_manager_init(void);

/* ---- 模式切换（带安全过渡） ---- */
bool motion_manager_set_mode(motion_mode_t mode);

/* ---- 遥控速度命令写入 ---- */
void motion_manager_set_remote_cmd(const app_velocity_t *cmd);

/* ---- 20ms 周期更新（PIT ISR 中调用）---- */
void motion_manager_update_20ms(void);

/* ---- 距离移动测试启动/停止 ---- */
void motion_manager_start_test_move(motion_test_axis_t axis, float target_value);
void motion_manager_stop_test_move(void);

/* ---- 单轮测试设置 ---- */
void motion_manager_set_test_wheel_speed(const app_wheel4_t *speed);

/* ---- 查询接口 ---- */
motion_mode_t   motion_manager_get_mode(void);
motion_status_t motion_manager_get_status(void);
bool            motion_manager_is_move_active(void);
bool            motion_manager_is_move_finished(void);

/* ---- 兼容接口: 获取距离移动调试信息 ---- */
float motion_manager_get_move_target_value(void);
float motion_manager_get_move_current_value(void);

#endif /* MOTION_MANAGER_H */
