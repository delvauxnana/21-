#include "filter.h"

/*
 * 文件作用:
 * 1. 提供低通、斜坡和一维卡尔曼滤波工具。
 * 2. 当前编码器速度滤波优先使用整形卡尔曼。
 */

/* 初始化一阶低通滤波器。 */
void lpf_init(lpf_t *filter, float alpha)
{
    if (filter == 0)
    {
        return;
    }

    filter->alpha = alpha;
    filter->value = 0.0f;
}

/* 输入新采样值并返回低通后的结果。 */
float lpf_update(lpf_t *filter, float input)
{
    if (filter == 0)
    {
        return input;
    }

    filter->value = filter->value + filter->alpha * (input - filter->value);
    return filter->value;
}

/* 初始化斜坡限幅器。 */
void ramp_init(ramp_t *ramp, float step)
{
    if (ramp == 0)
    {
        return;
    }

    ramp->step = step;
    ramp->value = 0.0f;
}

/* 按步长限制更新目标值。 */
float ramp_update(ramp_t *ramp, float target)
{
    if (ramp == 0)
    {
        return target;
    }

    if (ramp->value < target)
    {
        ramp->value += ramp->step;
        if (ramp->value > target)
        {
            ramp->value = target;
        }
    }
    else if (ramp->value > target)
    {
        ramp->value -= ramp->step;
        if (ramp->value < target)
        {
            ramp->value = target;
        }
    }

    return ramp->value;
}

/* 初始化一维卡尔曼滤波器。 */
void kalman1_init(kalman1_t *filter, int32_t q, int32_t r, int32_t init_value)
{
    if (filter == 0)
    {
        return;
    }

    filter->q = q;
    filter->r = r;
    filter->x = init_value;
    filter->p = r;
    filter->k = 0;
}

/* 复位一维卡尔曼滤波器状态。 */
void kalman1_reset(kalman1_t *filter, int32_t init_value)
{
    if (filter == 0)
    {
        return;
    }

    filter->x = init_value;
    filter->p = filter->r;
    filter->k = 0;
}

/* 输入一次测量值并返回滤波后的估计结果。 */
int32_t kalman1_update(kalman1_t *filter, int32_t measurement)
{
    int32_t denominator;
    int32_t error;

    if (filter == 0)
    {
        return measurement;
    }

    filter->p += filter->q;
    denominator = filter->p + filter->r;
    if (denominator <= 0)
    {
        denominator = 1;
    }

    filter->k = (filter->p * FILTER_GAIN_SCALE) / denominator;
    error = measurement - filter->x;
    filter->x = filter->x + (filter->k * error) / FILTER_GAIN_SCALE;
    filter->p = ((FILTER_GAIN_SCALE - filter->k) * filter->p) / FILTER_GAIN_SCALE;

    return filter->x;
}
