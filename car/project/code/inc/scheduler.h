#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * 文件作用:
 * 1. 提供系统 1ms 基准节拍与周期任务标志。
 * 2. 让主循环按固定节拍分发任务。
 */

typedef enum
{
    SCHED_TASK_5MS = 0,    /* 高频控制任务 */
    SCHED_TASK_20MS,       /* 主要控制与状态机任务 */
    SCHED_TASK_50MS,       /* 遥测与中频调试任务 */
    SCHED_TASK_100MS,      /* 屏幕和低频信息刷新任务 */
    SCHED_TASK_MAX         /* 任务枚举上限 */
} scheduler_task_t;

void scheduler_init(void);
void scheduler_tick_1ms(void);
bool scheduler_is_task_ready(scheduler_task_t task);
uint32_t scheduler_get_tick_ms(void);

#endif /* SCHEDULER_H */
