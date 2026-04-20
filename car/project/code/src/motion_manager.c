#include "motion_manager.h"
#include "board_config.h"
#include "chassis.h"
#include "control.h"
#include "encoder.h"
#include "imu.h"
#include "motor.h"
#include "odometry.h"
#include "parameter.h"
#include "pid.h"
#include "protocol.h"
#include <math.h>

/*
 * 文件作用:
 * 1. 实现运动模式状态机: IDLE / REMOTE / AUTO / TEST_WHEEL / TEST_MOVE。
 * 2. 在各模式下生成 app_velocity_t 目标，通过 control_set_velocity_target() 传递给速度控制器。
 * 3. 模式切换时执行安全停车过渡 (motor_stop_all + PID reset + encoder reset)。
 * 4. 距离移动测试和位置环逻辑从旧 control.c 迁移至此。
 */

/* ---- 到点判定参数 ---- */
#define MM_FINISH_WINDOW_DEG        (2.0f)
#define MM_FINISH_SPEED_DPS         (8.0f)
#define MM_RELAX_FINISH_WINDOW_DEG  (4.0f)
#define MM_RELAX_FINISH_CMD_DPS     (10)
#define MM_WARMUP_CYCLES            (2U)
#define MM_FUZZY_LEVEL_COUNT        (3U)

/* ---- 模块静态变量 ---- */
static motion_mode_t    g_mode;
static app_velocity_t   g_remote_cmd;            /* 遥控端写入的速度命令       */
static app_velocity_t   g_velocity_target;       /* 输出给速度控制器的目标     */

/* ---- 距离移动测试状态 ---- */
static bool              g_move_active;
static bool              g_move_finished;
static uint8_t           g_move_warmup_cycles;
static motion_test_axis_t g_move_axis;
static float             g_move_target_value;
static float             g_move_current_value;
static float             g_move_target_yaw_deg;
static int32_t           g_move_target_yaw_cdeg;
static float             g_yaw_correction_dps;
static int32_t           g_position_speed_cmd;
static int32_t           g_position_speed_cmd_ramped;
static pid_t             g_position_pid;
static bool              g_yaw_recover_active;
static uint8_t           g_yaw_recover_confirm_count;

/* ---- 单轮测试状态 ---- */
static app_wheel4_t      g_test_wheel_speed;

/* ---- 内部函数声明 ---- */
static void  mm_safe_stop(void);
static void  mm_reset_position_loop(void);
static void  mm_update_remote(void);
static void  mm_update_test_move(void);
static void  mm_update_test_wheel(void);

/* 工具函数 */
static int32_t mm_limit(int32_t value, int32_t limit);
static float   mm_limitf(float value, float limit);
static float   mm_clampf(float value, float min_value, float max_value);
static int32_t mm_wrap_angle_cdeg(int32_t angle_cdeg);
static int32_t mm_ramp_to(int32_t current, int32_t target, int32_t step);
static int32_t mm_get_axis_position(const app_pose_t *pose);
static float   mm_get_axis_feedback_speed(void);
static int32_t mm_get_axis_speed_limit(const parameter_set_t *p);
static bool    mm_prepare_position_integral(int32_t error);
static void    mm_fuzzy_membership_3(float x, float membership[MM_FUZZY_LEVEL_COUNT]);
static float   mm_fuzzy_infer_3x3(const float rule_table[MM_FUZZY_LEVEL_COUNT][MM_FUZZY_LEVEL_COUNT],
                                  const float e_membership[MM_FUZZY_LEVEL_COUNT],
                                  const float ec_membership[MM_FUZZY_LEVEL_COUNT]);
static void    mm_update_position_fuzzy_pid(const parameter_set_t *param,
                                            int32_t target_value,
                                            int32_t current_value);

/* ================================================================
 *  初始化
 * ================================================================ */
void motion_manager_init(void)
{
    parameter_set_t *param = parameter_get();

    g_mode = MOTION_MODE_IDLE;
    g_remote_cmd         = (app_velocity_t){0.0f, 0.0f, 0.0f};
    g_velocity_target    = (app_velocity_t){0.0f, 0.0f, 0.0f};

    g_move_active        = false;
    g_move_finished      = false;
    g_move_warmup_cycles = 0U;
    g_move_axis          = MOTION_TEST_AXIS_Y;
    g_move_target_value  = 0.0f;
    g_move_current_value = 0.0f;
    g_move_target_yaw_deg   = 0.0f;
    g_move_target_yaw_cdeg  = 0;
    g_yaw_correction_dps    = 0.0f;
    g_position_speed_cmd        = 0;
    g_position_speed_cmd_ramped = 0;
    g_yaw_recover_active        = false;
    g_yaw_recover_confirm_count = 0U;

    g_position_pid = param->position_pid;
    pid_init(&g_position_pid);

    g_test_wheel_speed = (app_wheel4_t){0, 0, 0, 0};
}

/* ================================================================
 *  模式切换
 * ================================================================ */
bool motion_manager_set_mode(motion_mode_t mode)
{
    if (mode >= MOTION_MODE_MAX)
    {
        return false;
    }

    if (mode == g_mode)
    {
        return true;
    }

    /* 安全停车过渡 */
    mm_safe_stop();

    g_mode = mode;
    return true;
}

/* ================================================================
 *  遥控速度命令写入
 * ================================================================ */
void motion_manager_set_remote_cmd(const app_velocity_t *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_remote_cmd = *cmd;

    /* 收到遥控命令自动进入 REMOTE 模式 */
    if (g_mode == MOTION_MODE_IDLE)
    {
        g_mode = MOTION_MODE_REMOTE;
    }
}

/* ================================================================
 *  20ms 周期更新（在 PIT ISR 的 20ms 分频中调用）
 * ================================================================ */
void motion_manager_update_20ms(void)
{
    switch (g_mode)
    {
        case MOTION_MODE_REMOTE:
            mm_update_remote();
            break;

        case MOTION_MODE_TEST_MOVE:
            mm_update_test_move();
            break;

        case MOTION_MODE_TEST_WHEEL:
            mm_update_test_wheel();
            break;

        case MOTION_MODE_AUTO:
            /* 预留：从 navigator 获取目标点并生成速度命令 */
            g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
            control_set_velocity_target(&g_velocity_target);
            break;

        case MOTION_MODE_IDLE:
        default:
            g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
            control_set_velocity_target(&g_velocity_target);
            break;
    }
}

/* ================================================================
 *  距离移动测试
 * ================================================================ */
void motion_manager_start_test_move(motion_test_axis_t axis, float target_value)
{
    parameter_set_t *param = parameter_get();

    mm_safe_stop();

    g_mode = MOTION_MODE_TEST_MOVE;

    encoder_reset();
    odometry_reset();
    imu_reset_yaw();

    g_position_pid = param->position_pid;
    pid_init(&g_position_pid);

    g_move_axis          = axis;
    g_move_target_value  = target_value;
    g_move_current_value = 0.0f;
    g_move_target_yaw_deg   = 0.0f;
    g_move_target_yaw_cdeg  = 0;
    g_yaw_correction_dps    = 0.0f;
    g_position_speed_cmd        = 0;
    g_position_speed_cmd_ramped = 0;
    g_move_active        = true;
    g_move_finished      = false;
    g_move_warmup_cycles = MM_WARMUP_CYCLES;
    g_yaw_recover_active        = false;
    g_yaw_recover_confirm_count = 0U;
    g_velocity_target    = (app_velocity_t){0.0f, 0.0f, 0.0f};
}

void motion_manager_stop_test_move(void)
{
    g_move_active        = false;
    g_move_warmup_cycles = 0U;
    g_yaw_recover_active        = false;
    g_yaw_recover_confirm_count = 0U;
    g_position_speed_cmd        = 0;
    g_position_speed_cmd_ramped = 0;
    g_yaw_correction_dps        = 0.0f;
    g_move_target_yaw_cdeg      = 0;
    g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
    control_set_velocity_target(&g_velocity_target);
    chassis_stop();
    pid_reset(&g_position_pid);
    motor_stop_all();
}

void motion_manager_set_test_wheel_speed(const app_wheel4_t *speed)
{
    if (speed == 0)
    {
        return;
    }

    g_test_wheel_speed = *speed;

    if (g_mode != MOTION_MODE_TEST_WHEEL)
    {
        mm_safe_stop();
        g_mode = MOTION_MODE_TEST_WHEEL;
    }
}

/* ================================================================
 *  查询接口
 * ================================================================ */
motion_mode_t motion_manager_get_mode(void)
{
    return g_mode;
}

motion_status_t motion_manager_get_status(void)
{
    motion_status_t s;
    s.mode          = g_mode;
    s.move_active   = g_move_active;
    s.move_finished = g_move_finished;
    s.move_target   = g_move_target_value;
    s.move_current  = g_move_current_value;
    return s;
}

bool motion_manager_is_move_active(void)
{
    return g_move_active;
}

bool motion_manager_is_move_finished(void)
{
    return g_move_finished;
}

float motion_manager_get_move_target_value(void)
{
    return g_move_target_value;
}

float motion_manager_get_move_current_value(void)
{
    return g_move_current_value;
}

/* ================================================================
 *  内部: 安全停车
 * ================================================================ */
static void mm_safe_stop(void)
{
    g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
    g_remote_cmd      = (app_velocity_t){0.0f, 0.0f, 0.0f};
    control_set_velocity_target(&g_velocity_target);
    control_emergency_stop();
    chassis_stop();
    motor_stop_all();

    g_move_active   = false;
    g_move_finished = false;
    g_move_warmup_cycles = 0U;
    g_yaw_recover_active = false;
    g_yaw_recover_confirm_count = 0U;
    g_position_speed_cmd = 0;
    g_position_speed_cmd_ramped = 0;
}

static void mm_reset_position_loop(void)
{
    pid_reset(&g_position_pid);
    g_position_speed_cmd = 0;
    g_position_speed_cmd_ramped = 0;
    g_yaw_correction_dps = 0.0f;
}

/* ================================================================
 *  内部: 位置外环模糊 PID 在线调参
 *  只调 TEST_MOVE 外环，轮速内环保持原有实现不变。
 * ================================================================ */
static void mm_fuzzy_membership_3(float x, float membership[MM_FUZZY_LEVEL_COUNT])
{
    float small;
    float medium;
    float large;
    float sum;

    x = mm_clampf(x, 0.0f, 1.0f);

    if (x <= 0.50f) {
        small = (0.50f - x) / 0.50f;
    } else {
        small = 0.0f;
    }

    if ((x <= 0.15f) || (x >= 0.85f)) {
        medium = 0.0f;
    } else if (x < 0.50f) {
        medium = (x - 0.15f) / 0.35f;
    } else {
        medium = (0.85f - x) / 0.35f;
    }

    if (x <= 0.50f) {
        large = 0.0f;
    } else {
        large = (x - 0.50f) / 0.50f;
    }

    membership[0] = mm_clampf(small, 0.0f, 1.0f);
    membership[1] = mm_clampf(medium, 0.0f, 1.0f);
    membership[2] = mm_clampf(large, 0.0f, 1.0f);

    sum = membership[0] + membership[1] + membership[2];
    if (sum <= 0.0001f) {
        membership[1] = 1.0f;
        membership[0] = 0.0f;
        membership[2] = 0.0f;
        return;
    }

    membership[0] /= sum;
    membership[1] /= sum;
    membership[2] /= sum;
}

static float mm_fuzzy_infer_3x3(const float rule_table[MM_FUZZY_LEVEL_COUNT][MM_FUZZY_LEVEL_COUNT],
                                const float e_membership[MM_FUZZY_LEVEL_COUNT],
                                const float ec_membership[MM_FUZZY_LEVEL_COUNT])
{
    float weighted_sum = 0.0f;
    float weight_total = 0.0f;
    uint8_t i;
    uint8_t j;

    for (i = 0U; i < MM_FUZZY_LEVEL_COUNT; i++) {
        for (j = 0U; j < MM_FUZZY_LEVEL_COUNT; j++) {
            float weight = e_membership[i] * ec_membership[j];
            weighted_sum += weight * rule_table[i][j];
            weight_total += weight;
        }
    }

    if (weight_total <= 0.0001f) {
        return 0.0f;
    }

    return weighted_sum / weight_total;
}

static void mm_update_position_fuzzy_pid(const parameter_set_t *param,
                                         int32_t target_value,
                                         int32_t current_value)
{
    static const float kp_rule[MM_FUZZY_LEVEL_COUNT][MM_FUZZY_LEVEL_COUNT] = {
        {-0.40f, -0.20f,  0.00f},
        { 0.25f,  0.12f, -0.05f},
        { 0.85f,  0.60f,  0.32f},
    };
    static const float ki_rule[MM_FUZZY_LEVEL_COUNT][MM_FUZZY_LEVEL_COUNT] = {
        { 0.85f,  0.45f,  0.10f},
        { 0.18f,  0.00f, -0.25f},
        {-0.80f, -0.92f, -1.00f},
    };
    static const float kd_rule[MM_FUZZY_LEVEL_COUNT][MM_FUZZY_LEVEL_COUNT] = {
        {-0.45f, -0.10f,  0.25f},
        {-0.05f,  0.28f,  0.62f},
        { 0.15f,  0.52f,  0.90f},
    };
    int32_t error_value;
    int32_t delta_error;
    float abs_target;
    float error_scale;
    float delta_scale;
    float e_norm;
    float ec_norm;
    float e_membership[MM_FUZZY_LEVEL_COUNT];
    float ec_membership[MM_FUZZY_LEVEL_COUNT];
    float dkp_norm;
    float dki_norm;
    float dkd_norm;
    float kp_span;
    float ki_span;
    float kd_span;
    int32_t kp;
    int32_t ki;
    int32_t kd;

    if (param == 0) {
        return;
    }

    error_value = target_value - current_value;
    delta_error = error_value - g_position_pid.last_error;
    abs_target = fabsf(g_move_target_value);

    if (MOTION_TEST_AXIS_Z == g_move_axis) {
        error_scale = mm_clampf(abs_target * 0.60f, 8.0f, 60.0f);
        delta_scale = mm_clampf(error_scale * 0.18f, 1.5f, 12.0f);
        kp_span = 700.0f;
        ki_span = 0.0f;
        kd_span = 220.0f;
    } else {
        error_scale = mm_clampf(abs_target * 0.60f, 40.0f, 240.0f);
        delta_scale = mm_clampf(error_scale * 0.20f, 8.0f, 60.0f);
        kp_span = 800.0f;
        ki_span = 120.0f;
        kd_span = 160.0f;
    }

    e_norm = mm_clampf((float)func_abs(error_value) / error_scale, 0.0f, 1.0f);
    ec_norm = mm_clampf((float)func_abs(delta_error) / delta_scale, 0.0f, 1.0f);

    mm_fuzzy_membership_3(e_norm, e_membership);
    mm_fuzzy_membership_3(ec_norm, ec_membership);

    dkp_norm = mm_fuzzy_infer_3x3(kp_rule, e_membership, ec_membership);
    dki_norm = mm_fuzzy_infer_3x3(ki_rule, e_membership, ec_membership);
    dkd_norm = mm_fuzzy_infer_3x3(kd_rule, e_membership, ec_membership);

    kp = param->position_pid.kp + (int32_t)(dkp_norm * kp_span);
    ki = param->position_pid.ki + (int32_t)(dki_norm * ki_span);
    kd = param->position_pid.kd + (int32_t)(dkd_norm * kd_span);

    kp = func_limit_ab(kp, 0, 4000);
    ki = func_limit_ab(ki, 0, 2000);
    kd = func_limit_ab(kd, 0, 2000);

    g_position_pid.kp = kp;
    g_position_pid.ki = ki;
    g_position_pid.kd = kd;
    g_position_pid.integral_limit = param->position_pid.integral_limit;
    g_position_pid.output_limit = param->position_pid.output_limit;
}

/* ================================================================
 *  内部: REMOTE 模式更新（含陀螺仪航向修正）
 * ================================================================ */
static void mm_update_remote(void)
{
    static int32_t s_target_yaw_cdeg = 0;
    static bool    s_yaw_locked      = false;

    const proto_velocity_t *proto_vel = protocol_get_velocity();
    parameter_set_t *param = parameter_get();
    imu_state_t imu = imu_get_state();

    /* 失控保护 → 清零 */
    if (proto_vel->failsafe)
    {
        g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
        s_yaw_locked = false;
        control_set_velocity_target(&g_velocity_target);
        return;
    }

    g_velocity_target = g_remote_cmd;

    /* 陀螺仪航向修正（仅 IMU 就绪且已校准时生效） */
    if (imu.ready && imu.calibrated)
    {
        bool has_rotation_cmd = (fabsf(g_remote_cmd.wz) > 1.0f);

        if (has_rotation_cmd)
        {
            /* 有旋转指令 → 随动更新航向目标 */
            s_target_yaw_cdeg = imu.yaw_cdeg;
            s_yaw_locked = false;
        }
        else
        {
            /* 无旋转指令 → 锁定当前航向 */
            if (!s_yaw_locked)
            {
                s_target_yaw_cdeg = imu.yaw_cdeg;
                s_yaw_locked = true;
            }

            /* PD 控制修正航向偏差 + 陀螺仪阻尼 */
            int32_t ye_cdeg = mm_wrap_angle_cdeg(s_target_yaw_cdeg - imu.yaw_cdeg);
            int32_t db_cdeg = (int32_t)(param->yaw_hold_deadband_deg * 100.0f + 0.5f);
            float ye_deg = (float)ye_cdeg * 0.01f;
            float yaw_hold_err = ye_deg;

            /* 死区内仅阻尼，死区外 P+D */
            if (func_abs(ye_cdeg) <= db_cdeg)
            {
                yaw_hold_err = 0.0f;
            }

            float wz_corr = (yaw_hold_err * param->yaw_hold_kp
                           - imu.gyro_z_dps * param->yaw_hold_gyro_damp)
                           * param->yaw_control_sign;
            wz_corr = mm_limitf(wz_corr, param->yaw_hold_limit_dps);
            g_velocity_target.wz = wz_corr;
        }
    }

    control_set_velocity_target(&g_velocity_target);
}

/* ================================================================
 *  内部: TEST_WHEEL 模式更新
 * ================================================================ */
static void mm_update_test_wheel(void)
{
    control_set_wheel_speed_direct(&g_test_wheel_speed);
}

/* ================================================================
 *  内部: TEST_MOVE 距离移动测试更新
 *  (从旧 control.c 的 control_update_distance_move 迁移而来)
 * ================================================================ */
static void mm_update_test_move(void)
{
    parameter_set_t *param = parameter_get();
    app_pose_t  pose       = odometry_get_pose();
    imu_state_t imu_state  = imu_get_state();
    int32_t target_value   = (int32_t)g_move_target_value;
    int32_t current_value  = mm_get_axis_position(&pose);
    int32_t feedback_limit = mm_get_axis_speed_limit(param);
    float   axis_fb_speed  = mm_get_axis_feedback_speed();
    int32_t yaw_error_cdeg    = 0;
    int32_t z_axis_fb_cdeg_dps = 0;
    float   yaw_error_deg     = 0.0f;
    float   move_error        = 0.0f;
    int32_t position_ki_backup;
    bool    allow_integral;

    if (!g_move_active)
    {
        g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
        control_set_velocity_target(&g_velocity_target);
        return;
    }

    /* 预热阶段：编码器/里程计对齐零点 */
    if (g_move_warmup_cycles > 0U)
    {
        g_move_warmup_cycles--;
        g_move_current_value           = 0.0f;
        g_move_target_yaw_cdeg         = imu_state.yaw_cdeg;
        g_move_target_yaw_deg          = imu_state.yaw_deg;
        g_yaw_correction_dps           = 0.0f;
        g_position_speed_cmd           = 0;
        g_position_speed_cmd_ramped    = 0;
        g_yaw_recover_confirm_count    = 0U;
        g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};
        control_set_velocity_target(&g_velocity_target);
        return;
    }

    g_move_current_value = (float)current_value;

    /* Z 轴旋转模式 */
    if (MOTION_TEST_AXIS_Z == g_move_axis)
    {
        /* Z 轴旋转: 误差公式与 wz 输出(不乘 sign)配对 */
        yaw_error_cdeg    = mm_wrap_angle_cdeg((int32_t)(g_move_target_value * 100.0f) - imu_state.yaw_cdeg);
        z_axis_fb_cdeg_dps = func_abs(imu_state.gyro_z_cdeg_dps);
        yaw_error_deg     = (float)yaw_error_cdeg * 0.01f;
        current_value     = target_value - (int32_t)(yaw_error_cdeg / 100);
        g_move_current_value = imu_state.yaw_deg;
    }

    /* 位置环：模糊调参 + PID */
    mm_update_position_fuzzy_pid(param, target_value, current_value);
    allow_integral      = mm_prepare_position_integral(target_value - current_value);
    position_ki_backup  = g_position_pid.ki;
    if (false == allow_integral)
    {
        g_position_pid.ki = 0;
    }
    g_position_speed_cmd = pid_update(&g_position_pid, target_value, current_value);
    g_position_pid.ki    = position_ki_backup;
    g_position_speed_cmd = mm_limit(g_position_speed_cmd, feedback_limit);
    g_position_speed_cmd_ramped = mm_ramp_to(g_position_speed_cmd_ramped,
                                              g_position_speed_cmd,
                                              param->position_ramp_step_mm_s);

    /* 生成速度目标 */
    g_velocity_target = (app_velocity_t){0.0f, 0.0f, 0.0f};

    if (MOTION_TEST_AXIS_Z == g_move_axis)
    {
        /* Z 轴旋转: PID 输出方向已匹配运动学，不乘 yaw_control_sign
         * （yaw_control_sign 仅用于 X/Y 保向修正，两者符号约定不同） */
        g_yaw_recover_active = false;
        g_yaw_recover_confirm_count = 0U;
        g_yaw_correction_dps = (float)g_position_speed_cmd_ramped;
        g_velocity_target.wz = g_yaw_correction_dps;

        move_error = fabsf(yaw_error_deg);
    }
    else
    {
        /* X/Y 平移 + 航向保持 */
        int32_t ye_cdeg = mm_wrap_angle_cdeg(g_move_target_yaw_cdeg - imu_state.yaw_cdeg);
        int32_t abs_ye_cdeg = func_abs(ye_cdeg);
        int32_t yaw_db_cdeg = (int32_t)(param->yaw_hold_deadband_deg * 100.0f + 0.5f);
        float   ye_deg = (float)ye_cdeg * 0.01f;
        float   abs_ye_deg = (float)abs_ye_cdeg * 0.01f;
        float   yaw_hold_err = ye_deg;
        /* 使用当前周期 IMU 陀螺仪读数（而非上一周期的 odometry 反馈），
         * 消除 20ms 阻尼延迟，启动瞬间即有阻尼修正 */
        float   current_gyro_z_dps = imu_state.gyro_z_dps;

        /* 大角度偏航恢复 */
        if (g_yaw_recover_active)
        {
            if (abs_ye_deg <= param->yaw_recover_exit_deg)
            {
                g_yaw_recover_active = false;
                g_yaw_recover_confirm_count = 0U;
            }
        }
        else if (abs_ye_deg >= param->yaw_recover_enter_deg)
        {
            if (g_yaw_recover_confirm_count < 3U)
            {
                g_yaw_recover_confirm_count++;
            }
            if (g_yaw_recover_confirm_count >= 3U)
            {
                g_yaw_recover_active = true;
            }
        }
        else
        {
            g_yaw_recover_confirm_count = 0U;
        }

        if (g_yaw_recover_active)
        {
            g_position_speed_cmd_ramped = 0;
            g_yaw_correction_dps = mm_limitf(ye_deg * param->yaw_recover_kp * param->yaw_control_sign,
                                              param->yaw_recover_limit_dps);
            g_velocity_target.wz = g_yaw_correction_dps;
            control_set_velocity_target(&g_velocity_target);
            return;
        }

        /* 正常直行保向 */
        if (func_abs(ye_cdeg) < yaw_db_cdeg)
        {
            yaw_hold_err = 0.0f;
        }

        g_yaw_correction_dps = (yaw_hold_err * param->yaw_hold_kp
                             - current_gyro_z_dps * param->yaw_hold_gyro_damp)
                             * param->yaw_control_sign;
        g_yaw_correction_dps = mm_limitf(g_yaw_correction_dps, param->yaw_hold_limit_dps);

        if (MOTION_TEST_AXIS_X == g_move_axis)
        {
            g_velocity_target.vx = (float)g_position_speed_cmd_ramped;
        }
        else
        {
            g_velocity_target.vy = (float)g_position_speed_cmd_ramped;
        }
        g_velocity_target.wz = g_yaw_correction_dps;

        move_error = fabsf((float)(target_value - current_value));
    }

    control_set_velocity_target(&g_velocity_target);

    /* 到点判定 */
    if (MOTION_TEST_AXIS_Z == g_move_axis)
    {
        if (((func_abs(yaw_error_cdeg) <= (int32_t)(MM_FINISH_WINDOW_DEG * 100.0f))
            && (z_axis_fb_cdeg_dps <= (int32_t)(MM_FINISH_SPEED_DPS * 100.0f)))
         || ((func_abs(yaw_error_cdeg) <= (int32_t)(MM_RELAX_FINISH_WINDOW_DEG * 100.0f))
            && (func_abs(g_position_speed_cmd_ramped) <= MM_RELAX_FINISH_CMD_DPS)
            && (z_axis_fb_cdeg_dps <= (int32_t)(MM_FINISH_SPEED_DPS * 100.0f))))
        {
            motion_manager_stop_test_move();
            g_move_finished = true;
        }
    }
    else
    {
        if (((move_error <= BOARD_MOVE_FINISH_WINDOW_MM)
            && (fabsf(axis_fb_speed) <= BOARD_MOVE_FINISH_SPEED_MM_S))
         || ((move_error <= BOARD_MOVE_RELAX_FINISH_WINDOW_MM)
            && (func_abs(g_position_speed_cmd_ramped) <= BOARD_MOVE_RELAX_FINISH_CMD_MM_S)
            && (fabsf(axis_fb_speed) <= BOARD_MOVE_FINISH_SPEED_MM_S)))
        {
            motion_manager_stop_test_move();
            g_move_finished = true;
        }
    }
}

/* ================================================================
 *  工具函数
 * ================================================================ */
static int32_t mm_limit(int32_t value, int32_t limit)
{
    if (limit <= 0)
    {
        return value;
    }
    return func_limit(value, limit);
}

static float mm_clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float mm_limitf(float value, float limit)
{
    if (limit <= 0.0f) return value;
    if (value > limit)  return limit;
    if (value < -limit) return -limit;
    return value;
}

static int32_t mm_wrap_angle_cdeg(int32_t angle_cdeg)
{
    while (angle_cdeg >= 18000)  angle_cdeg -= 36000;
    while (angle_cdeg < -18000)  angle_cdeg += 36000;
    return angle_cdeg;
}

static int32_t mm_ramp_to(int32_t current, int32_t target, int32_t step)
{
    if (step <= 0) return target;
    if ((target - current) > step)  return current + step;
    if ((current - target) > step)  return current - step;
    return target;
}

static int32_t mm_get_axis_position(const app_pose_t *pose)
{
    if (pose == 0) return 0;
    switch (g_move_axis)
    {
        case MOTION_TEST_AXIS_X: return (int32_t)pose->x_mm;
        case MOTION_TEST_AXIS_Z: return (int32_t)pose->yaw_deg;
        case MOTION_TEST_AXIS_Y:
        default:                 return (int32_t)pose->y_mm;
    }
}

static float mm_get_axis_feedback_speed(void)
{
    app_velocity_t vf = control_get_body_velocity_feedback();
    switch (g_move_axis)
    {
        case MOTION_TEST_AXIS_X: return vf.vx;
        case MOTION_TEST_AXIS_Z: return vf.wz;
        case MOTION_TEST_AXIS_Y:
        default:                 return vf.vy;
    }
}

static int32_t mm_get_axis_speed_limit(const parameter_set_t *p)
{
    if (p == 0) return 0;
    if (MOTION_TEST_AXIS_Z == g_move_axis)
    {
        return (int32_t)p->yaw_recover_limit_dps;
    }
    return p->position_speed_limit;
}

static bool mm_prepare_position_integral(int32_t error_value)
{
    int32_t abs_error = func_abs(error_value);
    int32_t enable_window;
    int32_t reset_window;
    bool error_cross_zero = (((error_value > 0) && (g_position_pid.last_error < 0))
                          || ((error_value < 0) && (g_position_pid.last_error > 0)));

    if (MOTION_TEST_AXIS_Z == g_move_axis)
    {
        enable_window = BOARD_POSITION_I_ENABLE_WINDOW_DEG;
        reset_window  = BOARD_POSITION_I_RESET_WINDOW_DEG;
    }
    else
    {
        enable_window = BOARD_POSITION_I_ENABLE_WINDOW_MM;
        reset_window  = BOARD_POSITION_I_RESET_WINDOW_MM;
    }

    if (error_cross_zero || (abs_error <= reset_window) || (abs_error > enable_window))
    {
        g_position_pid.integral = 0;
    }

    return ((abs_error > reset_window) && (abs_error <= enable_window));
}
