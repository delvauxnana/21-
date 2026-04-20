#include "navigator.h"

/*
 * 文件作用：
 * 1. 维护路径点序列和当前执行索引。
 * 2. 后续把 solver 结果转换为控制层可消费的目标点。
 */

void navigator_reset(navigator_plan_t *plan)
{
    if (plan == 0)
    {
        return;
    }

    plan->count = 0U;
    plan->index = 0U;
}

bool navigator_load_plan(navigator_plan_t *plan)
{
    (void)plan;
    /* TODO: 后续把离散路径压缩成拐点序列并装载到 plan 中。 */
    return false;
}

bool navigator_get_next_waypoint(navigator_plan_t *plan, app_pose_t *pose_out)
{
    if ((plan == 0) || (pose_out == 0) || (plan->index >= plan->count))
    {
        return false;
    }

    *pose_out = plan->points[plan->index];
    plan->index++;
    return true;
}
