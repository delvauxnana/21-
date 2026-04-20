#include "mission.h"

static mission_stage_t g_stage = MISSION_STAGE_IDLE;
static mission_state_t g_state = MISSION_STATE_IDLE;

/*
 * 文件作用：
 * 1. 管理三阶段任务流程。
 * 2. 后续在这里串起感知、规划和控制逻辑。
 */

void mission_init(void)
{
    g_stage = MISSION_STAGE_IDLE;
    g_state = MISSION_STATE_IDLE;
}

void mission_update_20ms(void)
{
    /* TODO: 按当前阶段完成建图、识别、规划、执行和收尾状态切换。 */
}

mission_stage_t mission_get_stage(void)
{
    return g_stage;
}

mission_state_t mission_get_state(void)
{
    return g_state;
}
