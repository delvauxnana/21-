#include "scheduler.h"

static volatile uint32_t g_tick_ms;
static volatile bool g_task_ready[SCHED_TASK_MAX];

/*
 * 文件作用：
 * 1. 维护系统节拍与任务就绪标志。
 * 2. 后续由 PIT 1ms 中断驱动。
 */

void scheduler_init(void)
{
    uint8_t i;

    g_tick_ms = 0U;
    for (i = 0U; i < (uint8_t)SCHED_TASK_MAX; ++i)
    {
        g_task_ready[i] = false;
    }
}

void scheduler_tick_1ms(void)
{
    g_tick_ms++;

    if ((g_tick_ms % 5U) == 0U)
    {
        g_task_ready[SCHED_TASK_5MS] = true;
    }
    if ((g_tick_ms % 20U) == 0U)
    {
        g_task_ready[SCHED_TASK_20MS] = true;
    }
    if ((g_tick_ms % 50U) == 0U)
    {
        g_task_ready[SCHED_TASK_50MS] = true;
    }
    if ((g_tick_ms % 100U) == 0U)
    {
        g_task_ready[SCHED_TASK_100MS] = true;
    }
}

bool scheduler_is_task_ready(scheduler_task_t task)
{
    bool ready;

    if (task >= SCHED_TASK_MAX)
    {
        return false;
    }

    ready = g_task_ready[task];
    g_task_ready[task] = false;
    return ready;
}

uint32_t scheduler_get_tick_ms(void)
{
    return g_tick_ms;
}
