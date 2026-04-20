/*===========================================================================
 * board_input.c
 * 硬件输入设备驱动：8 个轻触按键（手柄布局）+ 2 个模拟摇杆
 *
 * 采集策略：
 *   - 按键扫描 + 摇杆 ADC 读取在 PIT 10ms 定时中断中执行
 *   - 按键事件通过简易 ring buffer 传递给 LVGL read_cb
 *   - 摇杆数据通过 volatile 结构体传递给主循环
 *
 * UI 事件逻辑（焦点组、页面切换）在 gui/custom/ui_user_events.c 中
 *===========================================================================*/

#include "zf_common_headfile.h"
#include "board_input.h"
#include "ui_user_events.h"
#include "protocol.h"

/*===========================================================================
 * 车端速度映射增益（与 car/project/code/inc/cmd_dispatcher.h 保持同步）
 * 遥控端用这些增益在 UI 上预览车端实际速度
 *===========================================================================*/
#define CMD_VX_GAIN      (1.00f)    /* 摇杆 ±1000 → ±1000 mm/s          */
#define CMD_VY_GAIN      (1.00f)    /* 摇杆 ±1000 → ±1000 mm/s          */
#define CMD_WZ_GAIN      (0.40f)    /* 摇杆 ±1000 → ±400  deg/s         */
#define CMD_VX_SIGN      ( 1.0f)    /* vx 不取反                        */
#define CMD_VY_SIGN      ( 1.0f)    /* vy 不取反                        */
#define CMD_WZ_SIGN      (-1.0f)    /* wz 取反: 推右→顺时针            */

/*===========================================================================
 * 内部状态
 *===========================================================================*/

/* LVGL keypad indev 驱动与焦点组 */
static lv_indev_drv_t  s_keypad_drv;
static lv_indev_t     *s_keypad_indev = NULL;
static lv_group_t     *s_input_group  = NULL;

/* 当前活跃页面标识 */
static active_page_t s_active_page = PAGE_LOCKSCREEN;

/*--- 按键状态（ISR 内部使用） ---*/

typedef struct {
    gpio_pin_enum pin;
    uint32_t      lv_key;
    uint8         stable;       /* 消抖后的稳定电平：1=释放, 0=按下 */
    uint8         cnt;          /* 连续读到与 stable 相反电平的次数 */
} key_state_t;

/* 消抖阈值：连续 N 次采样（N×10ms）才认为状态改变 */
#define KEY_DEBOUNCE_CNT    3

static key_state_t s_keys[KEY_COUNT] = {
    /* 左侧 D-Pad — 使用 board_input.h 中的 KEY_MAP_* 宏 */
    { KEY_PIN_UP,    KEY_MAP_UP,    1, 0 },
    { KEY_PIN_DOWN,  KEY_MAP_DOWN,  1, 0 },
    { KEY_PIN_LEFT,  KEY_MAP_LEFT,  1, 0 },
    { KEY_PIN_RIGHT, KEY_MAP_RIGHT, 1, 0 },
    /* 右侧 ABXY */
    { KEY_PIN_A,     KEY_MAP_A,     1, 0 },
    { KEY_PIN_B,     KEY_MAP_B,     1, 0 },
    { KEY_PIN_X,     KEY_MAP_X,     1, 0 },
    { KEY_PIN_Y,     KEY_MAP_Y,     1, 0 },
};

/*--- 按键事件 ring buffer（ISR 写，read_cb 读） ---*/

#define KEY_RING_SIZE  16   /* 必须是 2 的幂 */

typedef struct {
    uint32_t        key;
    lv_indev_state_t state;
} key_event_t;

static volatile key_event_t s_key_ring[KEY_RING_SIZE];
static volatile uint8_t     s_ring_wr = 0;   /* ISR 写指针 */
static volatile uint8_t     s_ring_rd = 0;   /* read_cb 读指针 */

/* 最近一次报告给 LVGL 的键值（持续按住时重复发送） */
static uint32_t         s_last_key   = 0;
static lv_indev_state_t s_last_state = LV_INDEV_STATE_RELEASED;

/*--- 摇杆 ADC 数据（ISR 写，主循环读） ---*/

static stick_data_t s_stick = { 0, 0, 0, 0 };

/*--- 电池电压 ADC（ISR 写，主循环读） ---*/
static volatile uint16_t s_battery_adc_raw = 0;

/*--- EMA 低通滤波器状态（percent 域，ISR 内部） ---*/
/*  alpha = 64 / 256 ≈ 25%：每次采样只取 25% 新值 + 75% 历史值，
 *  10ms 周期下等效时间常数约 30ms，手感平滑且不太迟钝。 */
#define EMA_ALPHA           64
#define EMA_SCALE           256
/* 归一化值绝对值 ≤ SNAP_THRESHOLD 时直接归零，消除中位残余飘动 */
#define SNAP_THRESHOLD      30

static int32_t s_ema_lx = 0;
static int32_t s_ema_ly = 0;
static int32_t s_ema_rx = 0;
static int32_t s_ema_ry = 0;

/* EMA 一阶低通：new_ema = alpha * sample + (256 - alpha) * old_ema，定点运算 */
static int16_t stick_ema_update(int32_t *ema_state, int16_t sample)
{
    int32_t result;

    *ema_state = (EMA_ALPHA * (int32_t)sample + (EMA_SCALE - EMA_ALPHA) * (*ema_state)) / EMA_SCALE;
    result = *ema_state;

    /* snap-to-zero：残余小值归零 */
    if (result > -SNAP_THRESHOLD && result < SNAP_THRESHOLD) {
        result = 0;
    }

    return (int16_t)result;
}

/*--- 左摇杆虚拟导航键（阈值定义在 board_input.h） ---*/

/* 虚拟方向键状态：0=释放, 1=按下 */
static uint8 s_nav_up    = 0;
static uint8 s_nav_down  = 0;
static uint8 s_nav_left  = 0;
static uint8 s_nav_right = 0;

/*===========================================================================
 * 摇杆 ADC 工具（ISR 中调用）
 *===========================================================================*/

static int16_t stick_adc_to_percent(uint16 adc_raw)
{
    int32_t centered = (int32_t)adc_raw - STICK_ADC_CENTER;

    if (centered > -STICK_ADC_DEADZONE && centered < STICK_ADC_DEADZONE) {
        return 0;
    }

    if (centered > 0) {
        centered -= STICK_ADC_DEADZONE;
        return (int16_t)((centered * 1000) / (STICK_ADC_CENTER - STICK_ADC_DEADZONE));
    } else {
        centered += STICK_ADC_DEADZONE;
        return (int16_t)((centered * 1000) / (STICK_ADC_CENTER - STICK_ADC_DEADZONE));
    }
}

/* 向 ring buffer 推送一个按键事件（ISR 内部使用） */
static uint32_t board_input_remap_key_for_page(uint32_t key)
{
    if (s_active_page == PAGE_PID_TUNE) {
        if (key == LV_KEY_PREV) {
            return LV_KEY_UP;
        }
        if (key == LV_KEY_NEXT) {
            return LV_KEY_DOWN;
        }
    }

    return key;
}

static void ring_push(uint32_t key, lv_indev_state_t state)
{
    uint8_t next_wr = (s_ring_wr + 1) & (KEY_RING_SIZE - 1);
    uint32_t mapped_key = board_input_remap_key_for_page(key);
    if (next_wr != s_ring_rd) {
        s_key_ring[s_ring_wr].key   = mapped_key;
        s_key_ring[s_ring_wr].state = state;
        s_ring_wr = next_wr;
    }
}

/* 单轴虚拟按键更新（带滞回） */
static void stick_nav_axis(int16_t value, uint8 *neg_state, uint32_t neg_key,
                                          uint8 *pos_state, uint32_t pos_key)
{
    uint8 new_neg = (value < -STICK_NAV_THRESHOLD) ? 1 : 0;
    uint8 new_pos = (value >  STICK_NAV_THRESHOLD) ? 1 : 0;

    /* 滞回释放：只有回到 RELEASE 阈值以内才释放 */
    if (*neg_state && !new_neg && value > -STICK_NAV_RELEASE) {
        ring_push(neg_key, LV_INDEV_STATE_RELEASED);
        *neg_state = 0;
    } else if (!*neg_state && new_neg) {
        ring_push(neg_key, LV_INDEV_STATE_PRESSED);
        *neg_state = 1;
    }

    if (*pos_state && !new_pos && value < STICK_NAV_RELEASE) {
        ring_push(pos_key, LV_INDEV_STATE_RELEASED);
        *pos_state = 0;
    } else if (!*pos_state && new_pos) {
        ring_push(pos_key, LV_INDEV_STATE_PRESSED);
        *pos_state = 1;
    }
}

/*===========================================================================
 * 10ms 定时中断服务函数（在 isr.c 的 PIT_CH1 中调用）
 *===========================================================================*/

void board_input_isr_10ms(void)
{
    uint8   raw;
    int16_t raw_percent;

    /*--- 1. 扫描 8 个按键（带软件消抖） ---*/
    for (uint32_t i = 0; i < KEY_COUNT; i++) {
        raw = gpio_get_level(s_keys[i].pin);

        if (raw != s_keys[i].stable) {
            s_keys[i].cnt++;
            if (s_keys[i].cnt >= KEY_DEBOUNCE_CNT) {
                s_keys[i].stable = raw;
                s_keys[i].cnt    = 0;
                ring_push(s_keys[i].lv_key,
                          (raw == 0) ? LV_INDEV_STATE_PRESSED
                                     : LV_INDEV_STATE_RELEASED);
            }
        } else {
            s_keys[i].cnt = 0;
        }
    }

    /*--- 2. 左摇杆 ADC（始终读取 — 用于导航 + joystick_mode 页视觉）---*/
    raw_percent = -stick_adc_to_percent(adc_mean_filter_convert(STICK_L_X_CH, 4));
    s_stick.lx  = stick_ema_update(&s_ema_lx, raw_percent);

    raw_percent = -stick_adc_to_percent(adc_mean_filter_convert(STICK_L_Y_CH, 4));
    s_stick.ly  = stick_ema_update(&s_ema_ly, raw_percent);

    /* 左摇杆 → 虚拟方向键（使用 board_input.h 中的 STICK_NAV_*_KEY 宏） */
    stick_nav_axis(s_stick.lx, &s_nav_left,  STICK_NAV_NEG_X_KEY,
                               &s_nav_right, STICK_NAV_POS_X_KEY);
    stick_nav_axis(s_stick.ly, &s_nav_up,    STICK_NAV_NEG_Y_KEY,
                               &s_nav_down,  STICK_NAV_POS_Y_KEY);

    /*--- 3. 右摇杆 ADC（始终读取 — 用于协议发送 + joystick_mode 页视觉）---*/
    raw_percent = -stick_adc_to_percent(adc_mean_filter_convert(STICK_R_X_CH, 4));
    s_stick.rx  = stick_ema_update(&s_ema_rx, raw_percent);

    raw_percent = -stick_adc_to_percent(adc_mean_filter_convert(STICK_R_Y_CH, 4));
    s_stick.ry  = stick_ema_update(&s_ema_ry, raw_percent);

    /*--- 4. 电池电压 ADC（均值滤波 4 次采样） ---*/
    s_battery_adc_raw = adc_mean_filter_convert(BATTERY_ADC_CH, 4);
}

/*===========================================================================
 * LVGL keypad read callback（主线程，由 lv_timer_handler 调用）
 *===========================================================================*/

static void keypad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    /* 从 ring buffer 中消费一个事件 */
    if (s_ring_rd != s_ring_wr) {
        s_last_key   = s_key_ring[s_ring_rd].key;
        s_last_state = s_key_ring[s_ring_rd].state;
        s_ring_rd    = (s_ring_rd + 1) & (KEY_RING_SIZE - 1);

        /* 如果还有更多事件，告诉 LVGL 继续读 */
        data->continue_reading = (s_ring_rd != s_ring_wr);
    } else {
        data->continue_reading = false;
    }

    data->key   = s_last_key;
    data->state = s_last_state;
}

/*===========================================================================
 * 访问器
 *===========================================================================*/

lv_group_t *board_input_get_group(void)
{
    return s_input_group;
}

void board_input_set_active_page(active_page_t page)
{
    s_active_page = page;
}

active_page_t board_input_get_active_page(void)
{
    return s_active_page;
}

/*===========================================================================
 * 摇杆更新（在 main loop 中调用，读取 ISR 采集的数据）
 *
 * 性能优化策略：
 *   - 摇杆帽位置：每帧都更新（最小化感知延迟）
 *   - 数值标签：仅在值变化时更新（避免无谓的文本重绘）
 *   - 连接状态：仅在状态翻转时更新（避免每帧触发样式重算）
 *===========================================================================*/

/* 缓存上次写入标签的值，避免相同文本反复触发 LVGL 重绘 */
static int16_t s_cache_lx = 0x7FFF;
static int16_t s_cache_ly = 0x7FFF;
static int16_t s_cache_rx = 0x7FFF;
static int16_t s_cache_ry = 0x7FFF;
static int16_t s_cache_vx = 0x7FFF;
static int16_t s_cache_vy = 0x7FFF;
static int16_t s_cache_wz = 0x7FFF;
static int8_t  s_cache_conn = -1;    /* -1=未初始化, 0=离线, 1=在线 */
static int8_t  s_cache_conn_menu = -1;  /* 菜单页连接状态缓存 */
static int8_t  s_cache_conn_pid = -1;   /* PID 页连接状态缓存 */

void board_input_joystick_update(lv_ui *ui)
{
    if (ui == NULL) return;
    if (s_active_page != PAGE_JOYSTICK_MODE) return;

    /* 防御性检查：页面切换后 ui_nullify_screen_ptrs 会将指针置 NULL */
    if (ui->joystick_mode == NULL) {
        /* 页面离开时重置缓存，确保下次进入会刷新所有标签 */
        s_cache_lx = 0x7FFF;
        s_cache_ly = 0x7FFF;
        s_cache_rx = 0x7FFF;
        s_cache_ry = 0x7FFF;
        s_cache_vx = 0x7FFF;
        s_cache_vy = 0x7FFF;
        s_cache_wz = 0x7FFF;
        s_cache_conn = -1;
        return;
    }

    /* 读取 ISR 已经计算好的归一化值 */
    int16_t lx = s_stick.lx;
    int16_t ly = s_stick.ly;
    int16_t rx = s_stick.rx;
    int16_t ry = s_stick.ry;

    /* ---- 摇杆帽位置更新（每帧执行，确保手感跟手） ---- */
    if (ui->joystick_mode_joystick_mode_left_stick_area != NULL &&
        ui->joystick_mode_joystick_mode_left_stick_knob != NULL) {
        ui_user_left_stick_set_percent(ui, lx, ly);
    }
    if (ui->joystick_mode_joystick_mode_right_stick_area != NULL &&
        ui->joystick_mode_joystick_mode_right_stick_knob != NULL) {
        ui_user_right_stick_set_percent(ui, rx, ry);
    }

    /* ---- 数值标签（仅值变化时才刷新，避免无效文本重绘） ---- */
    if (ui->joystick_mode_joystick_mode_left_stick_value_label != NULL) {
        if (lx != s_cache_lx || ly != s_cache_ly) {
            s_cache_lx = lx;
            s_cache_ly = ly;
            lv_label_set_text_fmt(ui->joystick_mode_joystick_mode_left_stick_value_label,
                                  "%d, %d", (int)lx, (int)(-ly));
        }
    }
    if (ui->joystick_mode_joystick_mode_right_stick_value_label != NULL) {
        if (rx != s_cache_rx || ry != s_cache_ry) {
            s_cache_rx = rx;
            s_cache_ry = ry;
            lv_label_set_text_fmt(ui->joystick_mode_joystick_mode_right_stick_value_label,
                                  "%d, %d", (int)rx, (int)(-ry));
        }
    }

    /* ---- 车端映射速度面板（仅值变化时才刷新） ---- */
    {
        /* 计算车端实际速度映射：与 car/cmd_dispatcher.c 中的公式一致
         * 协议发送: vx=lx, vy=-ly, wz=rx
         * 前进(forward) 对应摇杆纵轴(-ly), 横移(lateral) 对应摇杆横轴(lx) */
        int16_t mapped_fwd = (int16_t)((float)(-ly) * CMD_VY_GAIN * CMD_VY_SIGN);
        int16_t mapped_lat = (int16_t)((float)lx    * CMD_VX_GAIN * CMD_VX_SIGN);
        int16_t mapped_wz  = (int16_t)((float)rx    * CMD_WZ_GAIN * CMD_WZ_SIGN);

        if (ui->joystick_mode_joystick_mode_axis_x_value_label != NULL &&
            mapped_fwd != s_cache_vx) {
            s_cache_vx = mapped_fwd;
            lv_label_set_text_fmt(ui->joystick_mode_joystick_mode_axis_x_value_label,
                                  "%d mm/s", (int)mapped_fwd);
        }
        if (ui->joystick_mode_joystick_mode_axis_y_value_label != NULL &&
            mapped_lat != s_cache_vy) {
            s_cache_vy = mapped_lat;
            lv_label_set_text_fmt(ui->joystick_mode_joystick_mode_axis_y_value_label,
                                  "%d mm/s", (int)mapped_lat);
        }
        if (ui->joystick_mode_joystick_mode_axis_z_value_label != NULL &&
            mapped_wz != s_cache_wz) {
            s_cache_wz = mapped_wz;
            lv_label_set_text_fmt(ui->joystick_mode_joystick_mode_axis_z_value_label,
                                  "%d d/s", (int)mapped_wz);
        }
    }
}

/*===========================================================================
 * 总初始化
 *===========================================================================*/

void board_input_init(lv_ui *ui)
{
    /*--- 1. GPIO 按键初始化（输入 + 上拉）— 8 个 ---*/
    gpio_init(KEY_PIN_UP,    GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_DOWN,  GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_LEFT,  GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_RIGHT, GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_A,     GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_B,     GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_X,     GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(KEY_PIN_Y,     GPI, GPIO_HIGH, GPI_PULL_UP);

    /*--- 2. ADC 摇杆初始化（12-bit） ---*/
    adc_init(STICK_L_X_CH, ADC_12BIT);
    adc_init(STICK_L_Y_CH, ADC_12BIT);
    adc_init(STICK_R_X_CH, ADC_12BIT);
    adc_init(STICK_R_Y_CH, ADC_12BIT);

    /*--- 2b. 电池电压 ADC 初始化 ---*/
    adc_init(BATTERY_ADC_CH, ADC_12BIT);

    /*--- 3. 创建 LVGL 焦点组 ---*/
    s_input_group = lv_group_create();

    /*--- 4. 注册 LVGL keypad indev ---*/
    lv_indev_drv_init(&s_keypad_drv);
    s_keypad_drv.type    = LV_INDEV_TYPE_KEYPAD;
    s_keypad_drv.read_cb = keypad_read_cb;
    s_keypad_indev = lv_indev_drv_register(&s_keypad_drv);
    lv_indev_set_group(s_keypad_indev, s_input_group);

    /*--- 5. 注册 UI 页面事件（焦点组切换等） ---*/
    ui_user_input_events_init(ui);
}

void board_input_get_stick_data(int16_t *lx, int16_t *ly, int16_t *rx, int16_t *ry)
{
    if (lx) *lx = s_stick.lx;
    if (ly) *ly = s_stick.ly;
    if (rx) *rx = s_stick.rx;
    if (ry) *ry = s_stick.ry;
}

/*===========================================================================
 * 连接状态刷新（在主循环中调用）
 * 所有非锁屏页面的顶部状态栏均显示车端连接状态。
 *===========================================================================*/

static void update_status_label(lv_obj_t *label, int8_t *cache, int8_t conn_now)
{
    if (label == NULL) return;
    if (conn_now == *cache) return;
    *cache = conn_now;
    if (conn_now) {
        lv_label_set_text(label, "在线");
        lv_obj_set_style_text_color(label, lv_color_hex(0x33d233),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(label, "离线");
        lv_obj_set_style_text_color(label, lv_color_hex(0xd23333),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

/*--- 电池电压标签更新（内部辅助） ---*/
static int16_t s_cache_bat_mv = -1;       /* 上次写入的电压 mV 值 */
static int16_t s_cache_bat_mv_menu = -1;  /* 菜单页缓存 */
static int16_t s_cache_bat_mv_pid = -1;   /* PID 页缓存 */
static active_page_t s_status_page_cache = (active_page_t)(-1);

static void update_battery_label(lv_obj_t *label, int16_t *cache)
{
    if (label == NULL) return;

    /* 读取 ISR 采集的电池 ADC 原始值并换算为 mV */
    float v_adc = (float)s_battery_adc_raw * BATTERY_VREF / BATTERY_ADC_MAX;
    float v_bat = v_adc * BATTERY_DIVIDER;
    int16_t mv = (int16_t)(v_bat * 1000.0f);  /* 转成 mV 方便整数比较 */

    /* 量化到 10mV 精度，减少刷新频率 */
    mv = (int16_t)((mv / 10) * 10);

    if (mv == *cache) return;
    *cache = mv;

    /* 显示为 "X.XXV" 格式 */
    lv_label_set_text_fmt(label, "%d.%02dV",
                          (int)(mv / 1000), (int)((mv % 1000) / 10));

    /* 低电压变红色警告（< 3.3V），否则绿色 */
    if (mv < 3300) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF6B6B),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(label, lv_color_hex(0x5BE7A9),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void board_input_status_update(lv_ui *ui)
{
    if (ui == NULL) return;

    const proto_heartbeat_t *hb = protocol_get_heartbeat();
    int8_t conn_now = (hb != NULL && hb->connected) ? 1 : 0;

    if (s_status_page_cache != s_active_page) {
        s_status_page_cache = s_active_page;

        switch (s_active_page) {
        case PAGE_MENU:
            s_cache_conn_menu = -1;
            s_cache_bat_mv_menu = -1;
            break;
        case PAGE_JOYSTICK_MODE:
            s_cache_conn = -1;
            s_cache_bat_mv = -1;
            break;
        case PAGE_PID_TUNE:
            s_cache_conn_pid = -1;
            s_cache_bat_mv_pid = -1;
            break;
        case PAGE_LOCKSCREEN:
        default:
            break;
        }
    }

    switch (s_active_page) {
    case PAGE_MENU:
        update_status_label(ui->menu_menu_status_value_label,
                            &s_cache_conn_menu, conn_now);
        update_battery_label(ui->menu_menu_battery_label,
                             &s_cache_bat_mv_menu);
        break;
    case PAGE_JOYSTICK_MODE:
        update_status_label(ui->joystick_mode_joystick_mode_status_value_label,
                            &s_cache_conn, conn_now);
        update_battery_label(ui->joystick_mode_joystick_mode_battery_label,
                             &s_cache_bat_mv);
        break;
    case PAGE_PID_TUNE:
        update_status_label(ui->pid_tune_status_value_label,
                            &s_cache_conn_pid, conn_now);
        update_battery_label(ui->pid_tune_battery_label,
                             &s_cache_bat_mv_pid);
        break;
    case PAGE_LOCKSCREEN:
    default:
        /* 锁屏页不显示连接状态和电池电压 */
        break;
    }
}
