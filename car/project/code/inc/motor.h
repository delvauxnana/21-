#ifndef MOTOR_H
#define MOTOR_H

#include "zf_common_headfile.h"
#include "app_types.h"

typedef enum
{
    MOTOR_LF = 0,
    MOTOR_RF,
    MOTOR_LB,
    MOTOR_RB,
    MOTOR_MAX
} motor_index_t;

/* 初始化四路电机输出。 */
void motor_init(void);

/* 设置单个电机的输出，占空比范围约为 -10000~10000。 */
void motor_set_output(motor_index_t motor_id, int32_t duty);

/* 一次性下发四个轮子的输出命令。 */
void motor_set_output_all(const app_wheel4_t *duty_out);

/* 正常停止四个轮子的输出。 */
void motor_stop_all(void);

/* 急停四个轮子的输出，供故障或测试停止使用。 */
void motor_emergency_stop(void);

#endif /* MOTOR_H */
