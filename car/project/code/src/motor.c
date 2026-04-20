#include "motor.h"
#include "board_config.h"

static app_wheel4_t g_motor_output;

/* 返回整数的绝对值。 */
static int32_t motor_abs(int32_t value)
{
    return (value >= 0) ? value : (-value);
}

/* 限制电机输出占空比范围。 */
static int32_t motor_limit_output(int32_t duty)
{
    if (duty > BOARD_MOTOR_OUTPUT_LIMIT)
    {
        return BOARD_MOTOR_OUTPUT_LIMIT;
    }
    if (duty < (-BOARD_MOTOR_OUTPUT_LIMIT))
    {
        return -BOARD_MOTOR_OUTPUT_LIMIT;
    }
    return duty;
}

/* 按方向引脚和 PWM 引脚输出单个电机。 */
static void motor_apply_single(pwm_channel_enum pwm_pin,
                               gpio_pin_enum dir_pin,
                               gpio_level_enum forward_level,
                               int32_t duty)
{
    int32_t limited_duty = motor_limit_output(duty);
    uint32 duty_abs = (uint32)motor_abs(limited_duty);

    if (limited_duty >= 0)
    {
        gpio_set_level(dir_pin, (uint8)forward_level);
    }
    else
    {
        gpio_set_level(dir_pin, (uint8)!forward_level);
    }

    pwm_set_duty(pwm_pin, duty_abs);
}

/* 初始化四路电机对应的 PWM 和方向引脚。 */
void motor_init(void)
{
    g_motor_output = (app_wheel4_t){0};

    gpio_init(BOARD_MOTOR_LF_DIR_PIN, GPO, BOARD_MOTOR_LF_FORWARD_LEVEL, GPO_PUSH_PULL);
    gpio_init(BOARD_MOTOR_RF_DIR_PIN, GPO, BOARD_MOTOR_RF_FORWARD_LEVEL, GPO_PUSH_PULL);
    gpio_init(BOARD_MOTOR_LB_DIR_PIN, GPO, BOARD_MOTOR_LB_FORWARD_LEVEL, GPO_PUSH_PULL);
    gpio_init(BOARD_MOTOR_RB_DIR_PIN, GPO, BOARD_MOTOR_RB_FORWARD_LEVEL, GPO_PUSH_PULL);

    pwm_init(BOARD_MOTOR_LF_PWM_PIN, BOARD_MOTOR_PWM_FREQ_HZ, 0U);
    pwm_init(BOARD_MOTOR_RF_PWM_PIN, BOARD_MOTOR_PWM_FREQ_HZ, 0U);
    pwm_init(BOARD_MOTOR_LB_PWM_PIN, BOARD_MOTOR_PWM_FREQ_HZ, 0U);
    pwm_init(BOARD_MOTOR_RB_PWM_PIN, BOARD_MOTOR_PWM_FREQ_HZ, 0U);
}

/* 设置单个电机的输出，占空比范围约为 -10000~10000。 */
void motor_set_output(motor_index_t motor_id, int32_t duty)
{
    switch (motor_id)
    {
        case MOTOR_LF:
            g_motor_output.lf = motor_limit_output(duty);
            motor_apply_single(BOARD_MOTOR_LF_PWM_PIN, BOARD_MOTOR_LF_DIR_PIN, BOARD_MOTOR_LF_FORWARD_LEVEL, g_motor_output.lf);
            break;

        case MOTOR_RF:
            g_motor_output.rf = motor_limit_output(duty);
            motor_apply_single(BOARD_MOTOR_RF_PWM_PIN, BOARD_MOTOR_RF_DIR_PIN, BOARD_MOTOR_RF_FORWARD_LEVEL, g_motor_output.rf);
            break;

        case MOTOR_LB:
            g_motor_output.lb = motor_limit_output(duty);
            motor_apply_single(BOARD_MOTOR_LB_PWM_PIN, BOARD_MOTOR_LB_DIR_PIN, BOARD_MOTOR_LB_FORWARD_LEVEL, g_motor_output.lb);
            break;

        case MOTOR_RB:
            g_motor_output.rb = motor_limit_output(duty);
            motor_apply_single(BOARD_MOTOR_RB_PWM_PIN, BOARD_MOTOR_RB_DIR_PIN, BOARD_MOTOR_RB_FORWARD_LEVEL, g_motor_output.rb);
            break;

        default:
            break;
    }
}

/* 一次性下发四个轮子的输出命令。 */
void motor_set_output_all(const app_wheel4_t *duty_out)
{
    if (duty_out == 0)
    {
        return;
    }

    motor_set_output(MOTOR_LF, duty_out->lf);
    motor_set_output(MOTOR_RF, duty_out->rf);
    motor_set_output(MOTOR_LB, duty_out->lb);
    motor_set_output(MOTOR_RB, duty_out->rb);
}

/* 正常停止四个轮子的输出。 */
void motor_stop_all(void)
{
    app_wheel4_t zero_output = {0};
    motor_set_output_all(&zero_output);
}

/* 急停四个轮子的输出，供故障或测试停止使用。 */
void motor_emergency_stop(void)
{
    motor_stop_all();
}
