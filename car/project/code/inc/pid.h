#ifndef PID_H
#define PID_H

#include "zf_common_headfile.h"

/*
 * 文件作用:
 * 1. 提供定点 PID 结构体和更新接口。
 * 2. 当前速度环优先使用整形增量式 PID，系数统一按 PID_GAIN_SCALE 放大保存。
 */

#define PID_GAIN_SCALE     (1000)   /* PID 系数的统一放大倍数，例如 2.2 保存为 2200 */

typedef struct
{
    int32_t kp;              /* 比例系数，已按 PID_GAIN_SCALE 放大 */
    int32_t ki;              /* 积分系数，已按 PID_GAIN_SCALE 放大 */
    int32_t kd;              /* 微分系数，已按 PID_GAIN_SCALE 放大 */
    int32_t integral;        /* 位置式 PID 的积分累计值 */
    int32_t last_error;      /* e(k-1)，上一次误差 */
    int32_t prev_error;      /* e(k-2)，上上次误差 */
    int32_t output;          /* 增量式 PID 的累计输出 */
    int32_t integral_limit;  /* 积分限幅，防止积分饱和 */
    int32_t output_limit;    /* 控制输出限幅 */
} pid_t;

/* 初始化 PID 状态量。 */
void pid_init(pid_t *pid);

/* 复位 PID 的历史误差、积分项和输出项。 */
void pid_reset(pid_t *pid);

/* 执行一次位置式 PID 计算并返回输出。 */
int32_t pid_update(pid_t *pid, int32_t target, int32_t feedback);

/* 执行一次增量式 PID 计算并返回累计输出。 */
int32_t pid_update_incremental(pid_t *pid, int32_t target, int32_t feedback);

#endif /* PID_H */
