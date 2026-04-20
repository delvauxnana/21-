#ifndef SOLVER_H
#define SOLVER_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include "grid_map.h"
#include "navigator.h"

/*
 * 文件作用：
 * 1. 实现地图分解、BFS 求解和炸弹策略接口。
 * 2. 为 mission 层输出可执行的路径计划。
 */

void solver_init(void);
bool solver_plan_stage1(const grid_map_t *map, navigator_plan_t *plan_out);
bool solver_plan_stage2(const grid_map_t *map, navigator_plan_t *plan_out);
bool solver_plan_stage3(const grid_map_t *map, navigator_plan_t *plan_out);

#endif /* SOLVER_H */
