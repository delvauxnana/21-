#include "pid.h"

/* 限制 PID 内部变量和输出范围。 */
static int32_t pid_limit(int32_t value, int32_t limit)
{
    if (limit <= 0)
    {
        return value;
    }

    return func_limit(value, limit);
}

/* 初始化 PID 状态量。 */
void pid_init(pid_t *pid)
{
    if (pid == 0)
    {
        return;
    }

    pid->integral = 0;
    pid->last_error = 0;
    pid->prev_error = 0;
    pid->output = 0;
}

/* 复位 PID 的历史误差、积分项和输出项。 */
void pid_reset(pid_t *pid)
{
    if (pid == 0)
    {
        return;
    }

    pid->integral = 0;
    pid->last_error = 0;
    pid->prev_error = 0;
    pid->output = 0;
}

/* 执行一次位置式 PID 计算并返回输出。 */
int32_t pid_update(pid_t *pid, int32_t target, int32_t feedback)
{
    int32_t error;
    int32_t derivative;
    int32_t output;
    int32_t next_integral;
    bool error_cross_zero;
    int64_t output_sum;

    if (pid == 0)
    {
        return 0;
    }

    error = target - feedback;
    error_cross_zero = (((error > 0) && (pid->last_error < 0))
                     || ((error < 0) && (pid->last_error > 0)));
    if (error_cross_zero)
    {
        pid->integral = 0;
    }

    if (0 == pid->ki)
    {
        next_integral = 0;
    }
    else
    {
        next_integral = pid->integral + error;
        next_integral = pid_limit(next_integral, pid->integral_limit);
    }

    derivative = error - pid->last_error;
    output_sum = (int64_t)pid->kp * error
               + (int64_t)pid->ki * next_integral
               + (int64_t)pid->kd * derivative;
    output = (int32_t)(output_sum / PID_GAIN_SCALE);
    if ((pid->output_limit > 0)
        && (((output > pid->output_limit) && (error > 0))
         || ((output < (-pid->output_limit)) && (error < 0))))
    {
        next_integral = (0 == pid->ki) ? 0 : pid->integral;
        output_sum = (int64_t)pid->kp * error
                   + (int64_t)pid->ki * next_integral
                   + (int64_t)pid->kd * derivative;
        output = (int32_t)(output_sum / PID_GAIN_SCALE);
    }
    output = pid_limit(output, pid->output_limit);

    pid->integral = next_integral;
    pid->prev_error = pid->last_error;
    pid->last_error = error;
    pid->output = output;

    return output;
}

/* 执行一次增量式 PID 计算并返回累计输出。 */
int32_t pid_update_incremental(pid_t *pid, int32_t target, int32_t feedback)
{
    int32_t error;
    int64_t delta_output;

    if (pid == 0)
    {
        return 0;
    }

    error = target - feedback;
    delta_output = (int64_t)pid->kp * (error - pid->last_error)
                 + (int64_t)pid->ki * error
                 + (int64_t)pid->kd * (error - 2 * pid->last_error + pid->prev_error);

    pid->output += (int32_t)(delta_output / PID_GAIN_SCALE);
    pid->output = pid_limit(pid->output, pid->output_limit);

    pid->prev_error = pid->last_error;
    pid->last_error = error;

    return pid->output;
}
