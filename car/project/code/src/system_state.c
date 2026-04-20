#include "system_state.h"

static system_state_t g_system_state;

/*
 * 文件作用：
 * 1. 保存系统运行模式和故障标志。
 * 2. 给所有模块提供统一的全局状态入口。
 */

void system_state_init(void)
{
    g_system_state.run_mode = APP_MODE_MONITOR;
    g_system_state.mission_stage = MISSION_STAGE_IDLE;
    g_system_state.mission_state = MISSION_STATE_IDLE;
    g_system_state.fault_flags = 0U;
}

void system_state_set_mode(app_run_mode_t mode)
{
    g_system_state.run_mode = mode;
}

app_run_mode_t system_state_get_mode(void)
{
    return g_system_state.run_mode;
}

system_state_t system_state_get_snapshot(void)
{
    return g_system_state;
}
