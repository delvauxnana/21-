#ifndef FILTER_H
#define FILTER_H

#include "zf_common_headfile.h"

/*
 * 文件作用:
 * 1. 存放常用的低通、斜坡、一维卡尔曼滤波工具。
 * 2. 当前速度环主链路优先使用整形卡尔曼，减少高频路径中的浮点运算。
 */

#define FILTER_GAIN_SCALE    (1024)   /* 滤波器内部增益的统一放大倍数 */

typedef struct
{
    float alpha;    /* 一阶低通滤波系数，暂保留浮点版本供非关键路径使用 */
    float value;    /* 滤波器当前输出值 */
} lpf_t;

typedef struct
{
    float step;     /* 每次更新允许变化的最大步长 */
    float value;    /* 斜坡输出的当前值 */
} ramp_t;

typedef struct
{
    int32_t q;      /* 过程噪声协方差，整形表示 */
    int32_t r;      /* 测量噪声协方差，整形表示 */
    int32_t x;      /* 当前状态估计值，单位与输入一致 */
    int32_t p;      /* 当前估计协方差，整形表示 */
    int32_t k;      /* 当前卡尔曼增益，按 FILTER_GAIN_SCALE 放大 */
} kalman1_t;

/* 初始化一阶低通滤波器。 */
void lpf_init(lpf_t *filter, float alpha);

/* 输入新采样值并返回低通后的结果。 */
float lpf_update(lpf_t *filter, float input);

/* 初始化斜坡限幅器。 */
void ramp_init(ramp_t *ramp, float step);

/* 按步长限制更新目标值。 */
float ramp_update(ramp_t *ramp, float target);

/* 初始化一维卡尔曼滤波器。 */
void kalman1_init(kalman1_t *filter, int32_t q, int32_t r, int32_t init_value);

/* 复位一维卡尔曼滤波器状态。 */
void kalman1_reset(kalman1_t *filter, int32_t init_value);

/* 输入一次测量值并返回滤波后的估计结果。 */
int32_t kalman1_update(kalman1_t *filter, int32_t measurement);

#endif /* FILTER_H */
