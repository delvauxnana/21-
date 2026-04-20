#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 集中保存全局模式、故障、任务阶段与运行快照。
 * 2. 避免工程里到处散落全局变量。
 */

typedef struct
{
    app_run_mode_t run_mode;         /* 当前运行模式 */
    mission_stage_t mission_stage;   /* 当前比赛阶段 */
    mission_state_t mission_state;   /* 当前任务状态 */
    uint16_t fault_flags;            /* 故障标志位 */
} system_state_t;

void system_state_init(void);
void system_state_set_mode(app_run_mode_t mode);
app_run_mode_t system_state_get_mode(void);
system_state_t system_state_get_snapshot(void);

#endif /* SYSTEM_STATE_H */
