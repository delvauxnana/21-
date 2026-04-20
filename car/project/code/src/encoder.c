#include "encoder.h"
#include "board_config.h"
#include "filter.h"

static app_wheel4_t g_encoder_speed;
static app_wheel4_t g_encoder_delta;
static kalman1_t g_encoder_speed_filter[BOARD_ENCODER_COUNT];

/* 读取一路编码器的计数并立刻清零。 */
static int16 encoder_read_and_clear(encoder_index_enum index)
{
    int16 count = encoder_get_count(index);
    encoder_clear_count(index);
    return count;
}

/* 将单周期编码器计数换算成 0.01mm 位移增量。 */
static int32_t encoder_count_to_delta_x100(int16 count, int32_t sign)
{
    int32_t scaled_count = (int32_t)count * sign;
    int32_t delta_x100 = (scaled_count * BOARD_WHEEL_DELTA_X100_NUM) / BOARD_WHEEL_DELTA_X100_DEN;
    return delta_x100;
}

/* 根据单周期位移增量计算整数速度，单位 mm/s。 */
static int32_t encoder_delta_x100_to_speed_mm_s(int32_t delta_x100)
{
    return delta_x100 / 2;
}

/* 初始化四路方向编码器并清空内部缓存。 */
void encoder_init(void)
{
    uint8_t i;

    g_encoder_speed = (app_wheel4_t){0};
    g_encoder_delta = (app_wheel4_t){0};

    encoder_dir_init(BOARD_ENCODER_LF_INDEX, BOARD_ENCODER_LF_CH1, BOARD_ENCODER_LF_CH2);
    encoder_dir_init(BOARD_ENCODER_RF_INDEX, BOARD_ENCODER_RF_CH1, BOARD_ENCODER_RF_CH2);
    encoder_dir_init(BOARD_ENCODER_LB_INDEX, BOARD_ENCODER_LB_CH1, BOARD_ENCODER_LB_CH2);
    encoder_dir_init(BOARD_ENCODER_RB_INDEX, BOARD_ENCODER_RB_CH1, BOARD_ENCODER_RB_CH2);

    encoder_clear_count(BOARD_ENCODER_LF_INDEX);
    encoder_clear_count(BOARD_ENCODER_RF_INDEX);
    encoder_clear_count(BOARD_ENCODER_LB_INDEX);
    encoder_clear_count(BOARD_ENCODER_RB_INDEX);

    for (i = 0U; i < BOARD_ENCODER_COUNT; ++i)
    {
        kalman1_init(&g_encoder_speed_filter[i], 5, 80, 0);
    }
}

/* 清空编码器硬件计数与软件滤波状态，避免任务启动第一拍读到陈旧累计量。 */
void encoder_reset(void)
{
    uint8_t i;

    g_encoder_speed = (app_wheel4_t){0};
    g_encoder_delta = (app_wheel4_t){0};

    encoder_clear_count(BOARD_ENCODER_LF_INDEX);
    encoder_clear_count(BOARD_ENCODER_RF_INDEX);
    encoder_clear_count(BOARD_ENCODER_LB_INDEX);
    encoder_clear_count(BOARD_ENCODER_RB_INDEX);

    for (i = 0U; i < BOARD_ENCODER_COUNT; ++i)
    {
        kalman1_reset(&g_encoder_speed_filter[i], 0);
    }
}

/* 按 20ms 周期读取编码器并更新轮速。 */
void encoder_update_20ms(void)
{
    int32_t lf_delta_x100;
    int32_t rf_delta_x100;
    int32_t lb_delta_x100;
    int32_t rb_delta_x100;

    lf_delta_x100 = encoder_count_to_delta_x100(encoder_read_and_clear(BOARD_ENCODER_LF_INDEX), BOARD_ENCODER_LF_SIGN);
    rf_delta_x100 = encoder_count_to_delta_x100(encoder_read_and_clear(BOARD_ENCODER_RF_INDEX), BOARD_ENCODER_RF_SIGN);
    lb_delta_x100 = encoder_count_to_delta_x100(encoder_read_and_clear(BOARD_ENCODER_LB_INDEX), BOARD_ENCODER_LB_SIGN);
    rb_delta_x100 = encoder_count_to_delta_x100(encoder_read_and_clear(BOARD_ENCODER_RB_INDEX), BOARD_ENCODER_RB_SIGN);

    g_encoder_delta.lf = lf_delta_x100;
    g_encoder_delta.rf = rf_delta_x100;
    g_encoder_delta.lb = lb_delta_x100;
    g_encoder_delta.rb = rb_delta_x100;

    g_encoder_speed.lf = kalman1_update(&g_encoder_speed_filter[0], encoder_delta_x100_to_speed_mm_s(lf_delta_x100));
    g_encoder_speed.rf = kalman1_update(&g_encoder_speed_filter[1], encoder_delta_x100_to_speed_mm_s(rf_delta_x100));
    g_encoder_speed.lb = kalman1_update(&g_encoder_speed_filter[2], encoder_delta_x100_to_speed_mm_s(lb_delta_x100));
    g_encoder_speed.rb = kalman1_update(&g_encoder_speed_filter[3], encoder_delta_x100_to_speed_mm_s(rb_delta_x100));
}

/* 获取四个轮子的滤波后线速度，单位 mm/s。 */
app_wheel4_t encoder_get_speed(void)
{
    return g_encoder_speed;
}

/* 获取四个轮子本周期的位移增量，单位 0.01mm。 */
app_wheel4_t encoder_get_delta(void)
{
    return g_encoder_delta;
}
