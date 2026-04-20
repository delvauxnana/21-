#ifndef DEBUG_H
#define DEBUG_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 汇总车端需要输出的调试快照。
 * 2. 供屏幕显示和无线调试模块复用。
 */

typedef struct
{
    app_pose_t pose;             /* 当前整车位姿快照 */
    app_velocity_t velocity;     /* 当前车体速度快照 */
    app_wheel4_t wheel_ref;      /* 四轮目标值 */
    app_wheel4_t wheel_meas;     /* 四轮实际反馈值 */
    mission_stage_t stage;       /* 当前比赛阶段 */
    mission_state_t state;       /* 当前任务状态 */
    uint16_t fault_flags;        /* 故障标志位集合 */
} debug_snapshot_t;

void debug_init_module(void);
void debug_update_snapshot(void);
debug_snapshot_t debug_get_snapshot(void);

#endif /* DEBUG_H */
