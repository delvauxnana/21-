#include "solver.h"

/*
 * 文件作用：
 * 1. 为三阶段任务提供路径规划入口。
 * 2. 当前阶段先保留函数框架，后续再逐步接入 BFS 和炸弹策略。
 */

void solver_init(void)
{
    /* TODO: 初始化搜索缓存和统计信息。 */
}

bool solver_plan_stage1(const grid_map_t *map, navigator_plan_t *plan_out)
{
    (void)map;
    (void)plan_out;
    /* TODO: 实现第一阶段单箱推送求解。 */
    return false;
}

bool solver_plan_stage2(const grid_map_t *map, navigator_plan_t *plan_out)
{
    (void)map;
    (void)plan_out;
    /* TODO: 实现第二阶段识别映射后的推箱求解。 */
    return false;
}

bool solver_plan_stage3(const grid_map_t *map, navigator_plan_t *plan_out)
{
    (void)map;
    (void)plan_out;
    /* TODO: 实现第三阶段带炸弹策略的路径求解。 */
    return false;
}
