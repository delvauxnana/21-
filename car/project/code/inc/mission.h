#ifndef MISSION_H
#define MISSION_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用：
 * 1. 统一管理第一、第二、第三阶段任务状态机。
 * 2. 协调感知、规划与控制模块。
 */

void mission_init(void);
void mission_update_20ms(void);
mission_stage_t mission_get_stage(void);
mission_state_t mission_get_state(void);

#endif /* MISSION_H */
