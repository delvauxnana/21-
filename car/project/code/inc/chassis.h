#ifndef CHASSIS_H
#define CHASSIS_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 对外提供底盘层车体速度命令接口。
 * 2. 将车体速度统一转换成四轮线速度参考值。
 */

void chassis_init(void);
void chassis_set_velocity(const app_velocity_t *cmd);
app_velocity_t chassis_get_velocity_command(void);
app_wheel4_t chassis_get_wheel_reference(void);
void chassis_stop(void);

#endif /* CHASSIS_H */
