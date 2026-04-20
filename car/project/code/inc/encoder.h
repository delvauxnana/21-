#ifndef ENCODER_H
#define ENCODER_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用:
 * 1. 统一管理四路编码器初始化与周期采样。
 * 2. 对上层提供整数速度和整数位移增量，减少高频路径中的浮点运算。
 */

/* 初始化四路方向编码器并清空内部缓存。 */
void encoder_init(void);

/* 清空编码器计数、速度滤波器和本周期位移缓存，用于任务启动前重新对齐零点。 */
void encoder_reset(void);

/* 按 20ms 周期读取编码器并更新轮速。 */
void encoder_update_20ms(void);

/* 获取四个轮子的滤波后线速度，单位 mm/s。 */
app_wheel4_t encoder_get_speed(void);

/* 获取四个轮子本周期的位移增量，单位 0.01mm。 */
app_wheel4_t encoder_get_delta(void);

#endif /* ENCODER_H */
