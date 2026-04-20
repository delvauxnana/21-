#ifndef NAVIGATOR_H
#define NAVIGATOR_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include <stdint.h>
#include "app_types.h"

/*
 * 文件作用:
 * 1. 把离散格子路径转换为连续目标点序列。
 * 2. 为控制模块提供下一个导航目标点。
 */

#define NAV_WAYPOINT_MAX            (64U)   /* 单次导航计划允许的最大目标点数 */

typedef struct
{
    app_pose_t points[NAV_WAYPOINT_MAX];    /* 规划出的目标点数组 */
    uint16_t count;                         /* 当前有效目标点数量 */
    uint16_t index;                         /* 正在执行到的目标点索引 */
} navigator_plan_t;

void navigator_reset(navigator_plan_t *plan);
bool navigator_load_plan(navigator_plan_t *plan);
bool navigator_get_next_waypoint(navigator_plan_t *plan, app_pose_t *pose_out);

#endif /* NAVIGATOR_H */
