#include "control.h"
#include "board_config.h"
#include "chassis.h"
#include "encoder.h"
#include "motor.h"
#include "odometry.h"
#include "parameter.h"
#include "pid.h"

/*
 * 文件作用:
 * 1. 纯速度内环控制器。
 * 2. 每 20ms 由 PIT ISR 调用: encoder → odometry → chassis → wheel PID → motor。
 * 3. 上层（motion_manager）通过 control_set_velocity_target() 写入车体速度命令。
 * 4. 位置环、距离移动等逻辑已迁移到 motion_manager 模块。
 */

/* ---- 模块静态变量 ---- */
static app_velocity_t g_velocity_target;
static app_velocity_t g_body_velocity_feedback;
static app_wheel4_t   g_wheel_speed_target;
static app_wheel4_t   g_wheel_speed_feedback;
static app_wheel4_t   g_wheel_output;
static pid_t          g_wheel_pid[MOTOR_MAX];
static int32_t        g_last_target_speed[MOTOR_MAX];
static uint8_t        g_startup_boost_count[MOTOR_MAX];
static bool           g_direct_wheel_mode;

/* ---- 内部函数声明 ---- */
static void    control_reset_wheel_pids(void);
static void    control_sync_pid_params(void);
static void    control_apply_wheel_speed_limit(app_wheel4_t *wheel_ref);
static int32_t control_update_single_wheel(motor_index_t id, int32_t target, int32_t feedback);
static int32_t control_limit(int32_t value, int32_t limit);
static int32_t control_get_min_effective_duty(motor_index_t id);
static int32_t control_get_anti_stall_duty(motor_index_t id);
static int32_t control_apply_startup_boost(motor_index_t id, int32_t target, int32_t feedback, int32_t duty, uint8_t *count);
static int32_t control_apply_anti_stall(motor_index_t id, int32_t target, int32_t feedback, int32_t duty);

/* ================================================================
 *  初始化
 * ================================================================ */
void control_init(void)
{
    parameter_set_t *param = parameter_get();
    uint8_t i;

    g_velocity_target        = (app_velocity_t){0.0f, 0.0f, 0.0f};
    g_body_velocity_feedback = (app_velocity_t){0.0f, 0.0f, 0.0f};
    g_wheel_speed_target     = (app_wheel4_t){0, 0, 0, 0};
    g_wheel_speed_feedback   = (app_wheel4_t){0, 0, 0, 0};
    g_wheel_output           = (app_wheel4_t){0, 0, 0, 0};
    g_direct_wheel_mode      = false;

    for (i = 0U; i < (uint8_t)MOTOR_MAX; ++i)
    {
        g_wheel_pid[i] = param->wheel_pid;
        pid_init(&g_wheel_pid[i]);
        g_last_target_speed[i]   = 0;
        g_startup_boost_count[i] = 0U;
    }
}

/* ================================================================
 *  20ms 周期更新
 * ================================================================ */
void control_update_20ms(void)
{
    parameter_set_t *param = parameter_get();

    /* 同步 PID 参数（支持热更新） */
    control_sync_pid_params();

    /* 1. 编码器采样 */
    encoder_update_20ms();

    /* 2. 里程计更新 */
    odometry_update_20ms();

    /* 3. 获取反馈 */
    g_wheel_speed_feedback   = encoder_get_speed();
    g_body_velocity_feedback = odometry_get_body_velocity();

    /* 4. 逆运动学: 车体速度 → 四轮速度 */
    if (!g_direct_wheel_mode)
    {
        chassis_set_velocity(&g_velocity_target);
        g_wheel_speed_target = chassis_get_wheel_reference();
        control_apply_wheel_speed_limit(&g_wheel_speed_target);
    }

    /* 5. 四轮 PID */
    g_wheel_output.lf = control_update_single_wheel(MOTOR_LF, g_wheel_speed_target.lf, g_wheel_speed_feedback.lf);
    g_wheel_output.rf = control_update_single_wheel(MOTOR_RF, g_wheel_speed_target.rf, g_wheel_speed_feedback.rf);
    g_wheel_output.lb = control_update_single_wheel(MOTOR_LB, g_wheel_speed_target.lb, g_wheel_speed_feedback.lb);
    g_wheel_output.rb = control_update_single_wheel(MOTOR_RB, g_wheel_speed_target.rb, g_wheel_speed_feedback.rb);

    /* 6. 输出限幅 */
    g_wheel_output.lf = control_limit(g_wheel_output.lf, param->wheel_pid.output_limit);
    g_wheel_output.rf = control_limit(g_wheel_output.rf, param->wheel_pid.output_limit);
    g_wheel_output.lb = control_limit(g_wheel_output.lb, param->wheel_pid.output_limit);
    g_wheel_output.rb = control_limit(g_wheel_output.rb, param->wheel_pid.output_limit);

    /* 7. 下发电机 */
    motor_set_output_all(&g_wheel_output);
}

/* ================================================================
 *  目标设置
 * ================================================================ */
void control_set_velocity_target(const app_velocity_t *target)
{
    if (target == 0)
    {
        return;
    }

    g_velocity_target   = *target;
    g_direct_wheel_mode = false;
}

void control_set_wheel_speed_direct(const app_wheel4_t *target_speed_mm_s)
{
    parameter_set_t *param = parameter_get();

    if (target_speed_mm_s == 0)
    {
        return;
    }

    g_wheel_speed_target = *target_speed_mm_s;
    control_apply_wheel_speed_limit(&g_wheel_speed_target);
    g_direct_wheel_mode = true;
}

void control_emergency_stop(void)
{
    g_velocity_target      = (app_velocity_t){0.0f, 0.0f, 0.0f};
    g_wheel_speed_target   = (app_wheel4_t){0, 0, 0, 0};
    g_wheel_output         = (app_wheel4_t){0, 0, 0, 0};
    g_direct_wheel_mode    = false;
    control_reset_wheel_pids();
    motor_stop_all();
}

/* ================================================================
 *  查询接口
 * ================================================================ */
app_velocity_t control_get_velocity_target(void)
{
    return g_velocity_target;
}

app_velocity_t control_get_body_velocity_feedback(void)
{
    return g_body_velocity_feedback;
}

app_wheel4_t control_get_wheel_speed_target(void)
{
    return g_wheel_speed_target;
}

app_wheel4_t control_get_wheel_speed_feedback(void)
{
    return g_wheel_speed_feedback;
}

app_wheel4_t control_get_wheel_output(void)
{
    return g_wheel_output;
}

/* ================================================================
 *  内部: PID 管理
 * ================================================================ */
static void control_reset_wheel_pids(void)
{
    uint8_t i;
    for (i = 0U; i < (uint8_t)MOTOR_MAX; ++i)
    {
        pid_reset(&g_wheel_pid[i]);
        g_last_target_speed[i]   = 0;
        g_startup_boost_count[i] = 0U;
    }
}

static void control_sync_pid_params(void)
{
    parameter_set_t *param = parameter_get();
    uint8_t i;
    for (i = 0U; i < (uint8_t)MOTOR_MAX; ++i)
    {
        g_wheel_pid[i].kp             = param->wheel_pid.kp;
        g_wheel_pid[i].ki             = param->wheel_pid.ki;
        g_wheel_pid[i].kd             = param->wheel_pid.kd;
        g_wheel_pid[i].integral_limit = param->wheel_pid.integral_limit;
        g_wheel_pid[i].output_limit   = param->wheel_pid.output_limit;
    }
}

static void control_apply_wheel_speed_limit(app_wheel4_t *w)
{
    parameter_set_t *param = parameter_get();
    if (w == 0) return;
    w->lf = control_limit(w->lf, param->speed_limit);
    w->rf = control_limit(w->rf, param->speed_limit);
    w->lb = control_limit(w->lb, param->speed_limit);
    w->rb = control_limit(w->rb, param->speed_limit);
}

static int32_t control_update_single_wheel(motor_index_t id, int32_t target, int32_t feedback)
{
    int32_t output;
    int32_t last_target = g_last_target_speed[(uint8_t)id];

    if (func_abs(target) < 1)
    {
        pid_reset(&g_wheel_pid[(uint8_t)id]);
        g_startup_boost_count[(uint8_t)id] = 0U;
        g_last_target_speed[(uint8_t)id]   = 0;
        return 0;
    }

    if ((func_abs(last_target) < 1) || ((target > 0) != (last_target > 0)))
    {
        g_startup_boost_count[(uint8_t)id] = BOARD_STARTUP_BOOST_MAX_CYCLES;
    }

    output = pid_update_incremental(&g_wheel_pid[(uint8_t)id], target, feedback);
    output = control_apply_startup_boost(id, target, feedback, output, &g_startup_boost_count[(uint8_t)id]);
    output = control_apply_anti_stall(id, target, feedback, output);
    g_last_target_speed[(uint8_t)id] = target;

    return output;
}

/* ================================================================
 *  内部: 启动补偿和防失速
 * ================================================================ */
static int32_t control_limit(int32_t value, int32_t limit)
{
    if (limit <= 0) return value;
    return func_limit(value, limit);
}

static int32_t control_get_min_effective_duty(motor_index_t id)
{
    switch (id)
    {
        case MOTOR_LF: return BOARD_MOTOR_LF_MIN_EFFECTIVE_DUTY;
        case MOTOR_RF: return BOARD_MOTOR_RF_MIN_EFFECTIVE_DUTY;
        case MOTOR_LB: return BOARD_MOTOR_LB_MIN_EFFECTIVE_DUTY;
        case MOTOR_RB: return BOARD_MOTOR_RB_MIN_EFFECTIVE_DUTY;
        default:       return BOARD_MOTOR_MIN_EFFECTIVE_DUTY;
    }
}

static int32_t control_get_anti_stall_duty(motor_index_t id)
{
    switch (id)
    {
        case MOTOR_LF: return BOARD_MOTOR_LF_ANTI_STALL_DUTY;
        case MOTOR_RF: return BOARD_MOTOR_RF_ANTI_STALL_DUTY;
        case MOTOR_LB: return BOARD_MOTOR_LB_ANTI_STALL_DUTY;
        case MOTOR_RB: return BOARD_MOTOR_RB_ANTI_STALL_DUTY;
        default:       return 0;
    }
}

static int32_t control_apply_startup_boost(motor_index_t id,
                                           int32_t target, int32_t feedback,
                                           int32_t duty, uint8_t *count)
{
    int32_t min_duty = control_get_min_effective_duty(id);

    if ((count != 0)
        && (*count > 0U)
        && (func_abs(target) > 1)
        && (func_abs(feedback) < BOARD_STARTUP_BOOST_FEEDBACK_MM_S)
        && (func_abs(duty) < min_duty))
    {
        (*count)--;
        return (target >= 0) ? min_duty : (-min_duty);
    }

    return duty;
}

static int32_t control_apply_anti_stall(motor_index_t id,
                                        int32_t target, int32_t feedback,
                                        int32_t duty)
{
    int32_t anti_stall = control_get_anti_stall_duty(id);
    int32_t abs_target   = func_abs(target);
    int32_t abs_feedback = func_abs(feedback);

    if ((anti_stall > 0)
        && (abs_target > BOARD_ANTI_STALL_MIN_TARGET_MM_S)
        && ((abs_feedback * 100) < (abs_target * BOARD_ANTI_STALL_SPEED_RATIO))
        && (func_abs(duty) < anti_stall))
    {
        return (target >= 0) ? anti_stall : (-anti_stall);
    }

    return duty;
}
