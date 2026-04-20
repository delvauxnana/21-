#include "debug.h"

static debug_snapshot_t g_debug_snapshot;

/*
 * 文件作用：
 * 1. 汇总调试快照给屏幕和无线终端使用。
 * 2. 当前阶段先保留快照缓存结构。
 */

void debug_init_module(void)
{
    g_debug_snapshot = (debug_snapshot_t){0};
}

void debug_update_snapshot(void)
{
    /* TODO: 从 odometry、control、mission 等模块汇总调试信息。 */
}

debug_snapshot_t debug_get_snapshot(void)
{
    return g_debug_snapshot;
}
