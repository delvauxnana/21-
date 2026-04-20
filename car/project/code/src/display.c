#include "display.h"
#include "board_config.h"
#include "control.h"
#include "motion_manager.h"
#include "encoder.h"
#include "imu.h"
#include "menu.h"
#include "protocol.h"
#include "odometry.h"
#include "parameter.h"
#include "system_state.h"
#include <stdio.h>
#include <string.h>

/*
 * IPS200 Pro - Windows 11 Fluent Design
 *
 * 浅色亚克力背�?+ 微软蓝强调色 + 圆角卡片风格
 * UTF-8 编码中文（通过 ips200pro_set_format 启用�? * 简洁、现代、留白充�? */

/* ===== 黑金配色 (RGB565) ===== */
#define W_BG         ((uint16)0x0000)   /* #000000 纯黑�?*/
#define W_CARD       ((uint16)0x2945)   /* #282828 深灰卡片 */
#define W_TEXT       ((uint16)0xFEA0)   /* #FFD700 金色主文�?*/
#define W_TEXT2      ((uint16)0xB4E0)   /* #B89C40 暗金副文�?*/
#define W_DIM        ((uint16)0x7B00)   /* #786020 暗金占位 */
#define W_ACCENT     ((uint16)0xFEA0)   /* #FFD700 金色强调 */
#define W_ACCENT_LT  ((uint16)0x3960)   /* #382C00 深金高亮�?*/
#define W_ACCENT_BG  ((uint16)0xFEA0)   /* #FFD700 金底(编辑�? */
#define W_WHITE      ((uint16)0x0000)   /* #000000 编辑�?标题文字为黑 */
#define W_GREEN      ((uint16)0x4608)   /* #40C040 绿色 */
#define W_RED        ((uint16)0xE208)   /* #E04040 红色 */
#define W_AMBER      ((uint16)0xFD00)   /* #FF9800 橙色 */
#define W_DIVIDER    ((uint16)0x3186)   /* #333333 分割�?*/
#define W_FOOTER     ((uint16)0x0000)   /* #000000 页脚底色 */

/* ===== 布局 ===== */
#define ROWS      5
#define HDR_H     44
#define ROW_Y0    46
#define ROW_H     44
#define ROW_GAP   2
#define NM_X      4
#define NM_W      110
#define VL_X      116
#define VL_W      122
#define FT_Y      276
#define FT_H      44

/* ===== 控件 ===== */
static uint16_t wHdr;
static uint16_t wNm[ROWS];
static uint16_t wVl[ROWS];
static uint16_t wFt;
static bool     s_ok = false;
static menu_page_t s_pp = (menu_page_t)0xFF;
static uint8_t  s_lock_blink = 0;   /* 锁屏页闪烁计数器 */
static uint16_t wImg;                /* lockscreen image widget */
static bool     s_img_created = false;

static int16_t ry(uint8_t i) { return (int16_t)(ROW_Y0 + i * (ROW_H + ROW_GAP)); }

/* ===== 行样�?===== */
static void rs(uint8_t i, bool sel, bool ed)
{
    if (ed) {
        /* 编辑�? 蓝底白字 */
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_ACCENT_BG);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_ACCENT_BG);
    } else if (sel) {
        /* 选中: 浅蓝�?+ 蓝字 */
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_ACCENT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_ACCENT_LT);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_ACCENT);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_ACCENT_LT);
    } else {
        /* 普�? 白底黑字 */
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_TEXT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_CARD);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_TEXT2);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_CARD);
    }
}

/* ===== 初始�?===== */
void display_init(void)
{
    uint8_t i;
    system_delay_ms(200);

    ips200pro_init("", IPS200PRO_TITLE_LEFT, 0);  /* title_size=0 隐藏标题�?*/
    system_delay_ms(100);

    /* 启用 UTF-8 编码 */
    ips200pro_set_format(IPS200PRO_FORMAT_UTF8);
    system_delay_ms(20);

    /* 标题�? 微软蓝底 */
    wHdr = ips200pro_label_create(0, 0, 240, HDR_H);
    ips200pro_set_font(wHdr, FONT_SIZE_16);
    ips200pro_set_color(wHdr, COLOR_FOREGROUND, W_WHITE);
    ips200pro_set_color(wHdr, COLOR_BACKGROUND, W_ACCENT);
    /* UTF-8: 特种部队 (居中, 11个空格前缀) */
    ips200pro_label_show_string(wHdr, "           \xe7\x89\xb9\xe7\xa7\x8d\xe9\x83\xa8\xe9\x98\x9f");
    system_delay_ms(15);

    /* 数据�? 白色卡片 */
    for (i = 0; i < ROWS; i++)
    {
        wNm[i] = ips200pro_label_create(NM_X, ry(i), NM_W, ROW_H);
        ips200pro_set_font(wNm[i], FONT_SIZE_16);
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_TEXT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_CARD);
        system_delay_ms(10);

        wVl[i] = ips200pro_label_create(VL_X, ry(i), VL_W, ROW_H);
        ips200pro_set_font(wVl[i], FONT_SIZE_16);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_TEXT2);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_CARD);
        system_delay_ms(10);
    }

    /* 页脚 */
    wFt = ips200pro_label_create(0, FT_Y, 240, FT_H);
    ips200pro_set_font(wFt, FONT_SIZE_16);
    ips200pro_set_color(wFt, COLOR_FOREGROUND, W_DIM);
    ips200pro_set_color(wFt, COLOR_BACKGROUND, W_FOOTER);
    system_delay_ms(10);


    system_delay_ms(8);

    system_delay_ms(8);

    system_delay_ms(8);

    system_delay_ms(8);

    system_delay_ms(8);

    /* lockscreen: full-screen image widget */
    wImg = ips200pro_image_create(0, 0, 240, 320);
    if (wImg != 0) {
        s_img_created = true;
        /* black background */
        ips200pro_image_draw_rectangle(wImg, 1, 0, 0, 240, 320, 0x0000);
        /* top gold bar */
        ips200pro_image_draw_rectangle(wImg, 2, 60, 55, 120, 3, 0xFEA0);
        /* bottom gold bar */
        ips200pro_image_draw_rectangle(wImg, 3, 60, 205, 120, 3, 0xFEA0);
        /* center accent line */
        ips200pro_image_draw_rectangle(wImg, 4, 40, 145, 160, 2, 0x7B00);
        /* display the image */
        ips200pro_image_display(wImg, NULL, 0, 0, IMAGE_NULL, 0);
    }
    system_delay_ms(15);

    s_ok = true;
}

/* ===== LOCKSCREEN - pure image cover (hidden on exit) ===== */
static void pg_lockscreen(void)
{
    if (!s_img_created) return;

    /* image widget covers all labels, just animate blink */
    s_lock_blink++;
    if (s_lock_blink & 0x01) {
        ips200pro_image_draw_rectangle(wImg, 5, 70, 250, 100, 2, 0xFEA0);
    } else {
        ips200pro_image_draw_rectangle(wImg, 5, 70, 250, 100, 2, 0x3960);
    }
    ips200pro_image_display(wImg, NULL, 0, 0, IMAGE_NULL, 0);
}

/* ===== HOME ===== */
static void pg_home(const menu_snapshot_t *s)
{
    /* UTF-8 中文菜单�?*/
    static const char *items[] = {
        "  \xe7\x8a\xb6\xe6\x80\x81\xe6\x80\xbb\xe8\xa7\x88",   /* 状态总览 */
        "  \xe8\xbf\x90\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95",   /* 运动测试 */
        "  \xe5\xa7\xbf\xe6\x80\x81\xe9\x87\x8c\xe7\xa8\x8b",   /* 姿态里�?*/
        "  \xe5\x8f\x82\xe6\x95\xb0\xe8\xae\xbe\xe7\xbd\xae"    /* 参数设置 */
    };
    uint8_t i;

    for (i = 0; i < ROWS; i++)
    {
        bool sel = (s->cursor_index == i);
        rs(i, sel, false);

        if (i < MENU_HOME_ITEM_COUNT) {
            ips200pro_label_show_string(wNm[i], items[i]);
            ips200pro_label_show_string(wVl[i], sel ? ">" : "");
        } else {
            ips200pro_label_show_string(wNm[i], "");
            ips200pro_label_show_string(wVl[i], "");
        }
    }
}

/* ===== STATUS ===== */
static void pg_status(void)
{
    char buf[24];
    system_state_t st = system_state_get_snapshot();
    imu_state_t imu = imu_get_state();
    uint8_t i;

    for (i = 0; i < ROWS; i++) rs(i, false, false);

    /* 当前模式 */
    ips200pro_label_show_string(wNm[0],
        "  \xe5\xbd\x93\xe5\x89\x8d\xe6\xa8\xa1\xe5\xbc\x8f");
    switch (st.run_mode) {
        case APP_MODE_AUTO:
            ips200pro_label_show_string(wVl[0], "\xe8\x87\xaa\xe5\x8a\xa8"); break;
        case APP_MODE_DEBUG_TEST:
            ips200pro_label_show_string(wVl[0], "\xe8\xb0\x83\xe8\xaf\x95"); break;
        case APP_MODE_PARAM_EDIT:
            ips200pro_label_show_string(wVl[0], "\xe7\xbc\x96\xe8\xbe\x91"); break;
        case APP_MODE_MANUAL:
            ips200pro_label_show_string(wVl[0], "\xe6\x89\x8b\xe5\x8a\xa8"); break;
        default:
            ips200pro_label_show_string(wVl[0], "\xe7\xa9\xba\xe9\x97\xb2"); break;
    }

    /* 运行状�?*/
    ips200pro_label_show_string(wNm[1],
        "  \xe8\xbf\x90\xe8\xa1\x8c\xe7\x8a\xb6\xe6\x80\x81");
    if (motion_manager_is_move_active()) {
        ips200pro_set_color(wVl[1], COLOR_FOREGROUND, W_ACCENT);
        ips200pro_label_show_string(wVl[1], "\xe8\xbf\x90\xe8\xa1\x8c\xe4\xb8\xad");
    } else if (motion_manager_is_move_finished()) {
        ips200pro_set_color(wVl[1], COLOR_FOREGROUND, W_GREEN);
        ips200pro_label_show_string(wVl[1], "\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90");
    } else {
        ips200pro_set_color(wVl[1], COLOR_FOREGROUND, W_DIM);
        ips200pro_label_show_string(wVl[1], "\xe5\xb0\xb1\xe7\xbb\xaa");
    }

    /* 车体速度反馈（编码器�?*/
    {
        app_velocity_t vel = control_get_body_velocity_feedback();
        /* UTF-8: 编码速度 */
        ips200pro_label_show_string(wNm[2],
            "  \xe7\xbc\x96\xe7\xa0\x81\xe9\x80\x9f\xe5\xba\xa6");
        snprintf(buf, sizeof(buf), "%.0f %.0f %.1f",
                 vel.vx, vel.vy, vel.wz);
        ips200pro_label_show_string(wVl[2], buf);
    }

    /* 四元�?Z 分量 */
    /* UTF-8: 四元�?Z */
    ips200pro_label_show_string(wNm[3],
        "  \xe5\x9b\x9b\xe5\x85\x83\xe6\x95\xb0 Z");
    if (imu.ready) {
        snprintf(buf, sizeof(buf), "%.4f", imu.quat_z);
    } else {
        /* UTF-8: �?*/
        snprintf(buf, sizeof(buf), "\xe6\x97\xa0");
    }
    ips200pro_label_show_string(wVl[3], buf);

    /* 偏航角（从四元数解算�?*/
    /* UTF-8: 偏航�?*/
    ips200pro_label_show_string(wNm[4],
        "  \xe5\x81\x8f\xe8\x88\xaa\xe8\xa7\x92");
    if (imu.ready) {
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", imu.yaw_deg);
    } else {
        /* UTF-8: �?*/
        snprintf(buf, sizeof(buf), "\xe6\x97\xa0");
    }
    ips200pro_label_show_string(wVl[4], buf);
}

/* ===== MOTION �?子菜单选择�?===== */
static void pg_motion(const menu_snapshot_t *s)
{
    /* UTF-8: 轴移动测�? 单轮测试 */
    static const char *items[] = {
        "  \xe8\xbd\xb4\xe7\xa7\xbb\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95",
        "  \xe5\x8d\x95\xe8\xbd\xae\xe6\xb5\x8b\xe8\xaf\x95"
    };
    uint8_t i;

    for (i = 0; i < ROWS; i++)
    {
        bool sel = (s->cursor_index == i);
        rs(i, sel, false);

        if (i < MENU_MOTION_ITEM_COUNT) {
            ips200pro_label_show_string(wNm[i], items[i]);
            ips200pro_label_show_string(wVl[i], sel ? ">" : "");
        } else {
            ips200pro_label_show_string(wNm[i], "");
            ips200pro_label_show_string(wVl[i], "");
        }
    }
}

/* ===== AXIS TEST �?轴移动测试子�?===== */
static void pg_axis_test(const menu_snapshot_t *s)
{
    char buf[24];
    parameter_set_t *p = parameter_get();
    app_wheel4_t out = control_get_wheel_output();
    app_wheel4_t enc = encoder_get_speed();
    uint8_t vi;

    for (vi = 0; vi < ROWS; vi++)
    {
        uint8_t idx = (uint8_t)(s->list_offset + vi);
        bool sel = (s->cursor_index == idx);
        bool editing = s->edit_mode && sel;
        rs(vi, sel, editing);

        if (idx >= MENU_AXIS_TEST_ITEM_COUNT) {
            ips200pro_label_show_string(wNm[vi], "");
            ips200pro_label_show_string(wVl[vi], "");
            continue;
        }

        switch (idx)
        {
            case 0U:
                /* 启动/停止 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe5\x90\xaf\xe5\x8a\xa8/\xe5\x81\x9c\xe6\xad\xa2");
                if (motion_manager_is_move_active()) {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_RED);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x81\x9c\xe6\xad\xa2");
                } else {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_GREEN);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x90\xaf\xe5\x8a\xa8");
                }
                break;

            case 1U:
                /* 测试轴向 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe6\xb5\x8b\xe8\xaf\x95\xe8\xbd\xb4\xe5\x90\x91");
                ips200pro_label_show_string(wVl[vi],
                    s->move_axis == MOTION_TEST_AXIS_X ? "< X >" :
                    s->move_axis == MOTION_TEST_AXIS_Z ? "< Z >" : "< Y >");
                break;

            case 2U:
                /* 目标数�?*/
                ips200pro_label_show_string(wNm[vi],
                    "  \xe7\x9b\xae\xe6\xa0\x87\xe6\x95\xb0\xe5\x80\xbc");
                snprintf(buf, sizeof(buf), "%ld", (long)s->move_target_value);
                ips200pro_label_show_string(wVl[vi], buf);
                break;

            case 3U:
                /* Spd Lim      */
                ips200pro_label_show_string(wNm[vi],
                    "  Spd Lim");
                if (s->move_axis == MOTION_TEST_AXIS_Z)
                    snprintf(buf, sizeof(buf), "%.0f d/s", p->yaw_recover_limit_dps);
                else
                    snprintf(buf, sizeof(buf), "%ld mm/s", (long)p->position_speed_limit);
                ips200pro_label_show_string(wVl[vi], buf);
                break;

            case 4U:
                /* 加速斜�?*/
                ips200pro_label_show_string(wNm[vi],
                    "  \xe5\x8a\xa0\xe9\x80\x9f\xe6\x96\x9c\xe7\x8e\x87");
                snprintf(buf, sizeof(buf), "%ld", (long)p->position_ramp_step_mm_s);
                ips200pro_label_show_string(wVl[vi], buf);
                break;

            case 5U:
                /* 电机输出（只读） */
                /* UTF-8: 输出 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe8\xbe\x93\xe5\x87\xba");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld",
                         (long)out.lf, (long)out.rf,
                         (long)out.lb, (long)out.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;

            case 6U:
                /* 编码器速度（只读） */
                /* UTF-8: 编码 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe7\xbc\x96\xe7\xa0\x81");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld",
                         (long)enc.lf, (long)enc.rf,
                         (long)enc.lb, (long)enc.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
        }
    }
}

/* ===== WHEEL TEST �?单轮测试子页 ===== */
static void pg_wheel_test(const menu_snapshot_t *s)
{
    char buf[24];
    /* UTF-8: 左前, 右前, 左后, 右后 */
    static const char *motor_names[] = {
        "< \xe5\xb7\xa6\xe5\x89\x8d >",
        "< \xe5\x8f\xb3\xe5\x89\x8d >",
        "< \xe5\xb7\xa6\xe5\x90\x8e >",
        "< \xe5\x8f\xb3\xe5\x90\x8e >"
    };
    app_wheel4_t out = control_get_wheel_output();
    app_wheel4_t enc = encoder_get_speed();
    uint8_t vi;

    for (vi = 0; vi < ROWS; vi++)
    {
        bool sel = (s->cursor_index == vi);
        bool editing = s->edit_mode && sel;
        rs(vi, sel, editing);

        switch (vi)
        {
            case 0U:
                /* 启动/停止 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe5\x90\xaf\xe5\x8a\xa8/\xe5\x81\x9c\xe6\xad\xa2");
                if (s->wheel_test_active) {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_RED);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x81\x9c\xe6\xad\xa2");
                } else {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_AMBER);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x90\xaf\xe5\x8a\xa8");
                }
                break;

            case 1U:
                /* 电机选择 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe7\x94\xb5\xe6\x9c\xba\xe9\x80\x89\xe6\x8b\xa9");
                ips200pro_label_show_string(wVl[vi],
                    motor_names[s->wheel_test_motor & 0x03]);
                break;

            case 2U:
                /* 目标速度 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe7\x9b\xae\xe6\xa0\x87\xe9\x80\x9f\xe5\xba\xa6");
                snprintf(buf, sizeof(buf), "%ld mm/s", (long)s->wheel_test_speed);
                ips200pro_label_show_string(wVl[vi], buf);
                break;

            case 3U:
                /* 电机输出（只读） */
                /* UTF-8: 输出 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe8\xbe\x93\xe5\x87\xba");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld",
                         (long)out.lf, (long)out.rf,
                         (long)out.lb, (long)out.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;

            case 4U:
                /* 编码器速度（只读） */
                /* UTF-8: 编码 */
                ips200pro_label_show_string(wNm[vi],
                    "  \xe7\xbc\x96\xe7\xa0\x81");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld",
                         (long)enc.lf, (long)enc.rf,
                         (long)enc.lb, (long)enc.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
        }
    }
}

/* ===== POSE ===== */
static void pg_pose(void)
{
    char buf[24];
    app_pose_t pose = odometry_get_pose();
    app_velocity_t vel = control_get_body_velocity_feedback();
    imu_state_t imu = imu_get_state();
    uint8_t i;

    for (i = 0; i < ROWS; i++) rs(i, false, false);

    ips200pro_label_show_string(wNm[0], "  \xe4\xbd\x8d\xe7\xbd\xae X");
    snprintf(buf, sizeof(buf), "%.0f mm", pose.x_mm);
    ips200pro_label_show_string(wVl[0], buf);

    ips200pro_label_show_string(wNm[1], "  \xe4\xbd\x8d\xe7\xbd\xae Y");
    snprintf(buf, sizeof(buf), "%.0f mm", pose.y_mm);
    ips200pro_label_show_string(wVl[1], buf);

    ips200pro_label_show_string(wNm[2],
        "  \xe5\x81\x8f\xe8\x88\xaa\xe8\xa7\x92");
    snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", pose.yaw_deg);
    ips200pro_label_show_string(wVl[2], buf);

    ips200pro_label_show_string(wNm[3],
        "  \xe5\x89\x8d\xe5\x90\x91\xe9\x80\x9f\xe5\xba\xa6");
    snprintf(buf, sizeof(buf), "%.0f mm/s", vel.vy);
    ips200pro_label_show_string(wVl[3], buf);

    ips200pro_label_show_string(wNm[4],
        "  \xe8\xa7\x92\xe9\x80\x9f\xe5\xba\xa6 Z");
    /* UTF-8:   /s */
    snprintf(buf, sizeof(buf), "%.1f\xc2\xb0/s", imu.gyro_z_dps);
    ips200pro_label_show_string(wVl[4], buf);
}

/* ===== PARAM ===== */
static void pg_param(const menu_snapshot_t *s)
{
    char buf[24];
    parameter_set_t *p = parameter_get();
    /* UTF-8: 位置比例, 位置积分, 位置微分, 速度比例, 速度积分, 速度微分, 航向增益, 航向死区, 回正阈�? 回正角�?*/
    static const char *nm[] = {
        "  \xe4\xbd\x8d\xe7\xbd\xae\xe6\xaf\x94\xe4\xbe\x8b",
        "  \xe4\xbd\x8d\xe7\xbd\xae\xe7\xa7\xaf\xe5\x88\x86",
        "  \xe4\xbd\x8d\xe7\xbd\xae\xe5\xbe\xae\xe5\x88\x86",
        "  \xe9\x80\x9f\xe5\xba\xa6\xe6\xaf\x94\xe4\xbe\x8b",
        "  \xe9\x80\x9f\xe5\xba\xa6\xe7\xa7\xaf\xe5\x88\x86",
        "  \xe9\x80\x9f\xe5\xba\xa6\xe5\xbe\xae\xe5\x88\x86",
        "  \xe8\x88\xaa\xe5\x90\x91\xe5\xa2\x9e\xe7\x9b\x8a",
        "  \xe8\x88\xaa\xe5\x90\x91\xe6\xad\xbb\xe5\x8c\xba",
        "  \xe5\x9b\x9e\xe6\xad\xa3\xe9\x98\x88\xe5\x80\xbc",
        "  \xe5\x9b\x9e\xe6\xad\xa3\xe8\xa7\x92\xe9\x80\x9f"
    };
    uint8_t vi;

    for (vi = 0; vi < ROWS; vi++)
    {
        uint8_t idx = (uint8_t)(s->list_offset + vi);
        bool sel = (s->cursor_index == idx);
        bool editing = s->edit_mode && sel;
        rs(vi, sel, editing);

        if (idx < MENU_PARAM_ITEM_COUNT) {
            float v = 0;
            switch (idx) {
                case 0: v = (float)p->position_pid.kp / PID_GAIN_SCALE; break;
                case 1: v = (float)p->position_pid.ki / PID_GAIN_SCALE; break;
                case 2: v = (float)p->position_pid.kd / PID_GAIN_SCALE; break;
                case 3: v = (float)p->wheel_pid.kp / PID_GAIN_SCALE; break;
                case 4: v = (float)p->wheel_pid.ki / PID_GAIN_SCALE; break;
                case 5: v = (float)p->wheel_pid.kd / PID_GAIN_SCALE; break;
                case 6: v = p->yaw_hold_kp; break;
                case 7: v = p->yaw_hold_deadband_deg; break;
                case 8: v = p->yaw_recover_enter_deg; break;
                case 9: v = p->yaw_recover_limit_dps; break;
            }
            ips200pro_label_show_string(wNm[vi], nm[idx]);
            if (editing) snprintf(buf, sizeof(buf), "[ %.2f ]", v);
            else         snprintf(buf, sizeof(buf), "%.2f", v);
            ips200pro_label_show_string(wVl[vi], buf);
        } else {
            ips200pro_label_show_string(wNm[vi], "");
            ips200pro_label_show_string(wVl[vi], "");
        }
    }
}

/* ===== 公共接口 ===== */
void display_show_page(uint8_t page_id) { (void)page_id; }

void display_update_100ms(void)
{
    menu_snapshot_t snap;
    if (!s_ok) return;
    snap = menu_get_snapshot();

    /* 锁屏页单独处�?*/
    if (snap.current_page == MENU_PAGE_LOCKSCREEN) {
        pg_lockscreen();
        return;
    }

    /* 标题 (居中显示, 从锁屏页切回时恢复金色标题栏) */
    if (snap.current_page != s_pp) {
        /* hide lockscreen image widget permanently */
        if (s_img_created) { ips200pro_set_hidden(wImg, 1); s_img_created = false; }


        /* 恢复标题栏到金底黑字(锁屏页会将其改为纯黑) */
        ips200pro_set_color(wHdr, COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wHdr, COLOR_BACKGROUND, W_ACCENT);

        switch (snap.current_page) {
            case MENU_PAGE_HOME:
                ips200pro_label_show_string(wHdr,
                    "           \xe7\x89\xb9\xe7\xa7\x8d\xe9\x83\xa8\xe9\x98\x9f");
                break;
            case MENU_PAGE_STATUS:
                ips200pro_label_show_string(wHdr,
                    "           \xe7\x8a\xb6\xe6\x80\x81\xe6\x80\xbb\xe8\xa7\x88");
                break;
            case MENU_PAGE_MOTION:
                ips200pro_label_show_string(wHdr,
                    "           \xe8\xbf\x90\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95");
                break;
            case MENU_PAGE_AXIS_TEST:
                ips200pro_label_show_string(wHdr,
                    "          \xe8\xbd\xb4\xe7\xa7\xbb\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95");
                break;
            case MENU_PAGE_WHEEL_TEST:
                ips200pro_label_show_string(wHdr,
                    "           \xe5\x8d\x95\xe8\xbd\xae\xe6\xb5\x8b\xe8\xaf\x95");
                break;
            case MENU_PAGE_POSE:
                ips200pro_label_show_string(wHdr,
                    "           \xe5\xa7\xbf\xe6\x80\x81\xe9\x87\x8c\xe7\xa8\x8b");
                break;
            case MENU_PAGE_PARAM:
                ips200pro_label_show_string(wHdr,
                    "           \xe5\x8f\x82\xe6\x95\xb0\xe8\xae\xbe\xe7\xbd\xae");
                break;
            default: break;
        }
        s_pp = snap.current_page;
    }

    switch (snap.current_page) {
        case MENU_PAGE_HOME:       pg_home(&snap);        break;
        case MENU_PAGE_STATUS:     pg_status();            break;
        case MENU_PAGE_MOTION:     pg_motion(&snap);       break;
        case MENU_PAGE_AXIS_TEST:  pg_axis_test(&snap);    break;
        case MENU_PAGE_WHEEL_TEST: pg_wheel_test(&snap);   break;
        case MENU_PAGE_POSE:       pg_pose();              break;
        case MENU_PAGE_PARAM:      pg_param(&snap);        break;
        default: break;
    }

    /* ===== 统一页脚: 遥控连接状�?+ 帧统�?===== */
    {
        const proto_velocity_t *vel = protocol_get_velocity();
        const proto_ctx_t      *ctx = protocol_get_ctx();
        char dbg[64];

        /* Debug: show real-time IMU yaw on footer during axis test */
        if (snap.current_page == MENU_PAGE_AXIS_TEST && motion_manager_is_move_active()) {
            imu_state_t ft_imu = imu_get_state();
            snprintf(dbg, sizeof(dbg), "YAW:%.1f TGT:%.0f CUR:%.1f",
                     ft_imu.yaw_deg,
                     motion_manager_get_move_target_value(),
                     motion_manager_get_move_current_value());
            ips200pro_set_color(wFt, COLOR_FOREGROUND, 0xFFFF00);
            ips200pro_label_show_string(wFt, dbg);
            return;
        }

        if (!vel->failsafe) {
            /* UTF-8: 已连�?�?N �?N */
            snprintf(dbg, sizeof(dbg),
                     " \xe5\xb7\xb2\xe8\xbf\x9e\xe6\x8e\xa5 \xe6\x88\x90:%lu \xe8\xb4\xa5:%lu",
                     (unsigned long)ctx->rx_good,
                     (unsigned long)ctx->rx_bad);
            ips200pro_set_color(wFt, COLOR_FOREGROUND, W_GREEN);
        } else {
            /* UTF-8: 等待�?. �?N �?N */
            snprintf(dbg, sizeof(dbg),
                     " \xe7\xad\x89\xe5\xbe\x85\xe4\xb8\xad.. \xe6\x88\x90:%lu \xe8\xb4\xa5:%lu",
                     (unsigned long)ctx->rx_good,
                     (unsigned long)ctx->rx_bad);
            ips200pro_set_color(wFt, COLOR_FOREGROUND, W_DIM);
        }
        ips200pro_label_show_string(wFt, dbg);
    }
}
