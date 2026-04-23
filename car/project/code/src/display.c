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
#include "vision_map.h"
#include <stdio.h>
#include <string.h>

/*
 * IPS200 Pro - Cyberpunk Theme Overhaul
 *
 * Background: Deep Synthwave Black/Purple
 * Accents: Neon Cyan (#00FFFF) & Neon Pink (#FF00FF)
 * Layout: Refined 5-row cards with improved spacing.
 */

/* ===== 赛博朋克配色 (RGB565) ===== */
#define W_BG         ((uint16)0x0000)   /* #000000 纯黑 */
#define W_CARD       ((uint16)0x1004)   /* #110022 深紫色卡片 */
#define W_TEXT       ((uint16)0xFFFF)   /* #FFFFFF 纯白主文字 */
#define W_TEXT2      ((uint16)0x867F)   /* #88CCFF 浅蓝色副文字 */
#define W_DIM        ((uint16)0x3186)   /* #333333 暗灰色占位 */
#define W_ACCENT     ((uint16)0x07FF)   /* #00FFFF 霓虹青强调 */
#define W_ACCENT_LT  ((uint16)0x0186)   /* #003333 深青色高亮背景 */
#define W_ACCENT_BG  ((uint16)0xF81F)   /* #FF00FF 霓虹粉(编辑模式) */
#define W_WHITE      ((uint16)0x0000)   /* 编辑态文字设为黑，增加对比 */
#define W_GREEN      ((uint16)0x3FE2)   /* #39FF14 霓虹绿 */
#define W_RED        ((uint16)0xF807)   /* #FF003C 鲜红色 */
#define W_AMBER      ((uint16)0xFD00)   /* #FF9800 橙色 */
#define W_DIVIDER    ((uint16)0x2104)   /* #222222 分割线 */
#define W_FOOTER     ((uint16)0x0802)   /* #080010 极深紫色页脚 */

/* ===== 布局 (精细化卡片) ===== */
#define ROWS      5
#define HDR_H     48
#define ROW_Y0    52
#define ROW_H     42
#define ROW_GAP   4
#define NM_X      8
#define NM_W      106
#define VL_X      118
#define VL_W      114
#define FT_Y      284
#define FT_H      36
#define MAP_INFO_ROWS       3
#define MAP_INFO_Y0         52
#define MAP_INFO_H          18
#define MAP_INFO_GAP        2
#define MAP_LEFT            8
#define MAP_TOP             112
#define MAP_ROW_H           14
#define MAP_ROW_W           224
#define MAP_ROW_TEXT_LEN    (GRID_MAP_W * 2U)

/* ===== 控件 ===== */
static uint16_t wHdr;
static uint16_t wNm[ROWS];
static uint16_t wVl[ROWS];
static uint16_t wFt;
static bool     s_ok = false;
static menu_page_t s_pp = (menu_page_t)0xFF;
static uint8_t  s_lock_blink = 0;
static uint16_t wImg;
static bool     s_img_created = false;
static uint16_t wMapRows[GRID_MAP_H];
static uint16_t wMapInfo[MAP_INFO_ROWS];
static bool     s_map_visible = false;
static bool     s_map_row_cache_valid[GRID_MAP_H];
static char     s_map_row_cache[GRID_MAP_H][MAP_ROW_TEXT_LEN];
typedef struct
{
    bool     has_refresh_result;
    bool     last_refresh_ok;
    uint8_t  optimize_status;
    uint32_t refresh_fail_count;
} map_text_diag_t;
static map_text_diag_t s_map_diag = { false, true, 0, 0 };

static int16_t ry(uint8_t i) { return (int16_t)(ROW_Y0 + i * (ROW_H + ROW_GAP)); }
static int16_t my(uint8_t i) { return (int16_t)(MAP_INFO_Y0 + i * (MAP_INFO_H + MAP_INFO_GAP)); }
static int16_t mry(uint8_t i) { return (int16_t)(MAP_TOP + i * MAP_ROW_H); }

static const char *map_dir_name(car_direction_t dir)
{
    switch (dir) {
        case CAR_DIR_UP:    return "UP";
        case CAR_DIR_RIGHT: return "RIGHT";
        case CAR_DIR_DOWN:  return "DOWN";
        case CAR_DIR_LEFT:  return "LEFT";
        default:            return "--";
    }
}

static char map_dir_char(car_direction_t dir)
{
    switch (dir) {
        case CAR_DIR_UP:    return '^';
        case CAR_DIR_RIGHT: return '>';
        case CAR_DIR_DOWN:  return 'v';
        case CAR_DIR_LEFT:  return '<';
        default:            return '?';
    }
}

static char map_cell_char(grid_cell_t cell)
{
    switch (cell) {
        case GRID_CELL_UNKNOWN: return '?';
        case GRID_CELL_EMPTY:   return '.';
        case GRID_CELL_WALL:    return '#';
        case GRID_CELL_BOX:     return 'B';
        case GRID_CELL_GOAL:    return 'G';
        case GRID_CELL_BOMB:    return 'X';
        default:                return '?';
    }
}

static void map_build_row_string(const vision_map_data_t *vmap, uint8_t row, char *buf, size_t buf_size)
{
    uint8_t col;
    uint8_t pos = 0;
    bool has_car = (bool)(vmap->car_x < GRID_MAP_W && vmap->car_y < GRID_MAP_H);

    (void)buf_size;

    for (col = 0; col < GRID_MAP_W; col++) {
        char cell_ch = map_cell_char(vmap->map.cells[row][col]);

        if (has_car && vmap->car_y == row && vmap->car_x == col) {
            cell_ch = map_dir_char(vmap->car_dir);
        }

        buf[pos++] = cell_ch;
        if (col + 1U < GRID_MAP_W) {
            buf[pos++] = ' ';
        }
    }
    buf[pos] = '\0';
}

static void map_refresh_rows(const vision_map_data_t *vmap)
{
    char row_buf[MAP_ROW_TEXT_LEN];
    uint8_t row;
    bool refresh_ok = true;

    for (row = 0; row < GRID_MAP_H; row++) {
        map_build_row_string(vmap, row, row_buf, sizeof(row_buf));

        if (s_map_row_cache_valid[row] && 0 == strcmp(row_buf, s_map_row_cache[row])) {
            continue;
        }

        if (wMapRows[row] == 0 || 0 != ips200pro_label_show_string(wMapRows[row], row_buf)) {
            refresh_ok = false;
            s_map_diag.refresh_fail_count++;
            continue;
        }

        strcpy(s_map_row_cache[row], row_buf);
        s_map_row_cache_valid[row] = true;
    }

    s_map_diag.has_refresh_result = true;
    s_map_diag.last_refresh_ok = refresh_ok;
}

static void map_update_footer(const vision_map_data_t *vmap)
{
    char buf[64];
    char txt_buf[24];
    uint16_t color = vmap->connected ? W_GREEN : W_RED;
    const char *link_state = vmap->connected ? "ONLINE" : "OFFLINE";

    if (!s_map_diag.has_refresh_result) {
        snprintf(txt_buf, sizeof(txt_buf), "TXT --");
    } else if (s_map_diag.last_refresh_ok) {
        snprintf(txt_buf, sizeof(txt_buf), "TXT OK");
    } else {
        snprintf(txt_buf, sizeof(txt_buf), "TXT ERR:%lu", (unsigned long)s_map_diag.refresh_fail_count);
        color = W_RED;
    }

    if (0 != s_map_diag.optimize_status) {
        snprintf(buf, sizeof(buf), "UART4 MAP %s OPT ERR:%u %s",
                 link_state,
                 (unsigned)s_map_diag.optimize_status,
                 txt_buf);
        if (W_RED != color) {
            color = W_AMBER;
        }
    } else {
        snprintf(buf, sizeof(buf), "UART4 MAP %s %s", link_state, txt_buf);
    }

    ips200pro_set_color(wFt, COLOR_FOREGROUND, color);
    ips200pro_label_show_string(wFt, buf);
}

/* ===== 行样式 ===== */
static void rs(uint8_t i, bool sel, bool ed)
{
    if (ed) {
        /* 编辑态: 霓虹粉背景 + 黑色文字 */
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_ACCENT_BG);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_ACCENT_BG);
    } else if (sel) {
        /* 选中: 深青色背景 + 霓虹青文字 */
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_ACCENT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_ACCENT_LT);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_ACCENT);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_ACCENT_LT);
    } else {
        /* 普通: 深紫色背景 + 纯白/浅蓝文字 */
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_TEXT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_CARD);
        ips200pro_set_color(wVl[i], COLOR_FOREGROUND, W_TEXT2);
        ips200pro_set_color(wVl[i], COLOR_BACKGROUND, W_CARD);
    }
}

/* ===== 初始化 ===== */
void display_init(void)
{
    uint8_t i;
    system_delay_ms(200);

    ips200pro_init("", IPS200PRO_TITLE_LEFT, 0);
    s_map_diag.optimize_status = ips200pro_set_optimize(1);
    system_delay_ms(100);

    ips200pro_set_format(IPS200PRO_FORMAT_UTF8);
    system_delay_ms(20);

    /* 标题栏: 霓虹青背景 */
    wHdr = ips200pro_label_create(0, 0, 240, HDR_H);
    ips200pro_set_font(wHdr, FONT_SIZE_16);
    ips200pro_set_color(wHdr, COLOR_FOREGROUND, W_WHITE);
    ips200pro_set_color(wHdr, COLOR_BACKGROUND, W_ACCENT);
    /* UTF-8: 特种部队 */
    ips200pro_label_show_string(wHdr, "           \xe7\x89\xb9\xe7\xa7\x8d\xe9\x83\xa8\xe9\x98\x9f");
    system_delay_ms(15);

    /* 数据行: 深紫色卡片 */
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

    /* lockscreen: full-screen image widget */
    wImg = ips200pro_image_create(0, 0, 240, 320);
    if (wImg != 0) {
        s_img_created = true;
        /* black background */
        ips200pro_image_draw_rectangle(wImg, 1, 0, 0, 240, 320, W_BG);
        /* Cyberpunk accent lines */
        ips200pro_image_draw_rectangle(wImg, 2, 60, 55, 120, 3, W_ACCENT);
        ips200pro_image_draw_rectangle(wImg, 3, 60, 205, 120, 3, W_ACCENT);
        ips200pro_image_draw_rectangle(wImg, 4, 40, 145, 160, 2, W_ACCENT_BG);
        ips200pro_image_display(wImg, NULL, 0, 0, IMAGE_NULL, 0);
    }
    system_delay_ms(15);

    for (i = 0; i < GRID_MAP_H; i++)
    {
        wMapRows[i] = ips200pro_label_create(MAP_LEFT, mry(i), MAP_ROW_W, MAP_ROW_H);
        if (wMapRows[i] != 0) {
            ips200pro_set_font(wMapRows[i], FONT_SIZE_12);
            ips200pro_label_mode(wMapRows[i], LABEL_CLIP);
            ips200pro_set_color(wMapRows[i], COLOR_FOREGROUND, W_TEXT);
            ips200pro_set_color(wMapRows[i], COLOR_BACKGROUND, W_BG);
            ips200pro_set_hidden(wMapRows[i], 1);
        }
        system_delay_ms(10);
    }

    for (i = 0; i < MAP_INFO_ROWS; i++)
    {
        wMapInfo[i] = ips200pro_label_create(0, my(i), 240, MAP_INFO_H);
        if (wMapInfo[i] != 0) {
            ips200pro_set_font(wMapInfo[i], FONT_SIZE_12);
            ips200pro_set_color(wMapInfo[i], COLOR_FOREGROUND, W_TEXT2);
            ips200pro_set_color(wMapInfo[i], COLOR_BACKGROUND, W_BG);
            ips200pro_set_hidden(wMapInfo[i], 1);
        }
        system_delay_ms(10);
    }

    s_ok = true;
}

static void pg_lockscreen(void)
{
    if (!s_img_created) return;
    s_lock_blink++;
    if (s_lock_blink & 0x01) {
        ips200pro_image_draw_rectangle(wImg, 5, 70, 250, 100, 2, W_ACCENT);
    } else {
        ips200pro_image_draw_rectangle(wImg, 5, 70, 250, 100, 2, W_ACCENT_LT);
    }
    ips200pro_image_display(wImg, NULL, 0, 0, IMAGE_NULL, 0);
}

static void pg_home(const menu_snapshot_t *s)
{
    static const char *items[] = {
        "  \xe7\x8a\xb6\xe6\x80\x81\xe6\x80\xbb\xe8\xa7\x88",
        "  \xe8\xbf\x90\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95",
        "  \xe5\xa7\xbf\xe6\x80\x81\xe9\x87\x8c\xe7\xa8\x8b",
        "  \xe5\x8f\x82\xe6\x95\xb0\xe8\xae\xbe\xe7\xbd\xae",
        "  \xe5\x9c\xb0\xe5\x9b\xbe\xe6\x98\xbe\xe7\xa4\xba"
    };
    uint8_t i;
    for (i = 0; i < ROWS; i++) {
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

static void pg_status(void)
{
    char buf[24];
    system_state_t st = system_state_get_snapshot();
    imu_state_t imu = imu_get_state();
    uint8_t i;
    for (i = 0; i < ROWS; i++) rs(i, false, false);

    ips200pro_label_show_string(wNm[0], "  \xe5\xbd\x93\xe5\x89\x8d\xe6\xa8\xa1\xe5\xbc\x8f");
    switch (st.run_mode) {
        case APP_MODE_AUTO:       ips200pro_label_show_string(wVl[0], "\xe8\x87\xaa\xe5\x8a\xa8"); break;
        case APP_MODE_DEBUG_TEST: ips200pro_label_show_string(wVl[0], "\xe8\xb0\x83\xe8\xaf\x95"); break;
        case APP_MODE_PARAM_EDIT: ips200pro_label_show_string(wVl[0], "\xe7\xbc\x96\xe8\xbe\x91"); break;
        case APP_MODE_MANUAL:     ips200pro_label_show_string(wVl[0], "\xe6\x89\x8b\xe5\x8a\xa8"); break;
        default:                  ips200pro_label_show_string(wVl[0], "\xe7\xa9\xba\xe9\x97\xb2"); break;
    }

    ips200pro_label_show_string(wNm[1], "  \xe8\xbf\x90\xe8\xa1\x8c\xe7\x8a\xb6\xe6\x80\x81");
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

    {
        app_velocity_t vel = control_get_body_velocity_feedback();
        ips200pro_label_show_string(wNm[2], "  \xe7\xbc\x96\xe7\xa0\x81\xe9\x80\x9f\xe5\xba\xa6");
        snprintf(buf, sizeof(buf), "%.0f %.0f %.1f", vel.vx, vel.vy, vel.wz);
        ips200pro_label_show_string(wVl[2], buf);
    }

    ips200pro_label_show_string(wNm[3], "  \xe5\x9b\x9b\xe5\x85\x83\xe6\x95\xb0 Z");
    if (imu.ready) snprintf(buf, sizeof(buf), "%.4f", imu.quat_z);
    else           snprintf(buf, sizeof(buf), "\xe6\x97\xa0");
    ips200pro_label_show_string(wVl[3], buf);

    ips200pro_label_show_string(wNm[4], "  \xe5\x81\x8f\xe8\x88\xaa\xe8\xa7\x92");
    if (imu.ready) snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", imu.yaw_deg);
    else           snprintf(buf, sizeof(buf), "\xe6\x97\xa0");
    ips200pro_label_show_string(wVl[4], buf);
}

static void pg_motion(const menu_snapshot_t *s)
{
    static const char *items[] = {
        "  \xe8\xbd\xb4\xe7\xa7\xbb\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95",
        "  \xe5\x8d\x95\xe8\xbd\xae\xe6\xb5\x8b\xe8\xaf\x95"
    };
    uint8_t i;
    for (i = 0; i < ROWS; i++) {
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

static void pg_axis_test(const menu_snapshot_t *s)
{
    char buf[24];
    parameter_set_t *p = parameter_get();
    app_wheel4_t out = control_get_wheel_output();
    app_wheel4_t enc = encoder_get_speed();
    uint8_t vi;

    for (vi = 0; vi < ROWS; vi++) {
        uint8_t idx = (uint8_t)(s->list_offset + vi);
        bool sel = (s->cursor_index == idx);
        bool editing = s->edit_mode && sel;
        rs(vi, sel, editing);

        if (idx >= MENU_AXIS_TEST_ITEM_COUNT) {
            ips200pro_label_show_string(wNm[vi], "");
            ips200pro_label_show_string(wVl[vi], "");
            continue;
        }

        switch (idx) {
            case 0U:
                ips200pro_label_show_string(wNm[vi], "  \xe5\x90\xaf\xe5\x8a\xa8/\xe5\x81\x9c\xe6\xad\xa2");
                if (motion_manager_is_move_active()) {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_RED);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x81\x9c\xe6\xad\xa2");
                } else {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_GREEN);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x90\xaf\xe5\x8a\xa8");
                }
                break;
            case 1U:
                ips200pro_label_show_string(wNm[vi], "  \xe6\xb5\x8b\xe8\xaf\x95\xe8\xbd\xb4\xe5\x90\x91");
                ips200pro_label_show_string(wVl[vi], s->move_axis == MOTION_TEST_AXIS_X ? "< X >" : s->move_axis == MOTION_TEST_AXIS_Z ? "< Z >" : "< Y >");
                break;
            case 2U:
                ips200pro_label_show_string(wNm[vi], "  \xe7\x9b\xae\xe6\xa0\x87\xe6\x95\xb0\xe5\x80\xbc");
                snprintf(buf, sizeof(buf), "%ld", (long)s->move_target_value);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
            case 3U:
                ips200pro_label_show_string(wNm[vi], "  Spd Lim");
                if (s->move_axis == MOTION_TEST_AXIS_Z) snprintf(buf, sizeof(buf), "%.0f d/s", p->yaw_recover_limit_dps);
                else snprintf(buf, sizeof(buf), "%ld mm/s", (long)p->position_speed_limit);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
            case 4U:
                ips200pro_label_show_string(wNm[vi], "  \xe5\x8a\xa0\xe9\x80\x9f\xe6\x96\x9c\xe7\x8e\x87");
                snprintf(buf, sizeof(buf), "%ld", (long)p->position_ramp_step_mm_s);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
            case 5U:
                ips200pro_label_show_string(wNm[vi], "  \xe8\xbe\x93\xe5\x87\xba");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld", (long)out.lf, (long)out.rf, (long)out.lb, (long)out.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
            case 6U:
                ips200pro_label_show_string(wNm[vi], "  \xe7\xbc\x96\xe7\xa0\x81");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld", (long)enc.lf, (long)enc.rf, (long)enc.lb, (long)enc.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
        }
    }
}

static void pg_wheel_test(const menu_snapshot_t *s)
{
    char buf[24];
    static const char *motor_names[] = { "< \xe5\xb7\xa6\xe5\x89\x8d >", "< \xe5\x8f\x83\xe5\x89\x8d >", "< \xe5\xb7\xa6\xe5\x90\x8e >", "< \xe5\x8f\x83\xe5\x90\x8e >" };
    app_wheel4_t out = control_get_wheel_output();
    app_wheel4_t enc = encoder_get_speed();
    uint8_t vi;

    for (vi = 0; vi < ROWS; vi++) {
        bool sel = (s->cursor_index == vi);
        bool editing = s->edit_mode && sel;
        rs(vi, sel, editing);

        switch (vi) {
            case 0U:
                ips200pro_label_show_string(wNm[vi], "  \xe5\x90\xaf\xe5\x8a\xa8/\xe5\x81\x9c\xe6\xad\xa2");
                if (s->wheel_test_active) {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_RED);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x81\x9c\xe6\xad\xa2");
                } else {
                    ips200pro_set_color(wVl[vi], COLOR_FOREGROUND, W_AMBER);
                    ips200pro_label_show_string(wVl[vi], "\xe5\x90\xaf\xe5\x8a\xa8");
                }
                break;
            case 1U:
                ips200pro_label_show_string(wNm[vi], "  \xe7\x94\xb5\xe6\x9c\xba\xe9\x80\x89\xe6\x8b\xa9");
                ips200pro_label_show_string(wVl[vi], motor_names[s->wheel_test_motor & 0x03]);
                break;
            case 2U:
                ips200pro_label_show_string(wNm[vi], "  \xe7\x9b\xae\xe6\xa0\x87\xe9\x80\x9f\xe5\xba\xa6");
                snprintf(buf, sizeof(buf), "%ld mm/s", (long)s->wheel_test_speed);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
            case 3U:
                ips200pro_label_show_string(wNm[vi], "  \xe8\xbe\x93\xe5\x87\xba");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld", (long)out.lf, (long)out.rf, (long)out.lb, (long)out.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
            case 4U:
                ips200pro_label_show_string(wNm[vi], "  \xe7\xbc\x96\xe7\xa0\x81");
                snprintf(buf, sizeof(buf), "%ld %ld %ld %ld", (long)enc.lf, (long)enc.rf, (long)enc.lb, (long)enc.rb);
                ips200pro_label_show_string(wVl[vi], buf);
                break;
        }
    }
}

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

    ips200pro_label_show_string(wNm[2], "  \xe5\x81\x8f\xe8\x88\xaa\xe8\xa7\x92");
    snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", pose.yaw_deg);
    ips200pro_label_show_string(wVl[2], buf);

    ips200pro_label_show_string(wNm[3], "  \xe5\x89\x8d\xe5\x90\x91\xe9\x80\x9f\xe5\xba\xa6");
    snprintf(buf, sizeof(buf), "%.0f mm/s", vel.vy);
    ips200pro_label_show_string(wVl[3], buf);

    ips200pro_label_show_string(wNm[4], "  \xe8\xa7\x92\xe9\x80\x9f\xe5\xba\xa6 Z");
    snprintf(buf, sizeof(buf), "%.1f\xc2\xb0/s", imu.gyro_z_dps);
    ips200pro_label_show_string(wVl[4], buf);
}

static void pg_param(const menu_snapshot_t *s)
{
    char buf[24];
    parameter_set_t *p = parameter_get();
    static const char *nm[] = {
        "  \xe4\xbd\x8d\xe7\xbd\xae\xe6\xaf\x94\xe4\xbe\x8b", "  \xe4\xbd\x8d\xe7\xbd\xae\xe7\xa7\xaf\xe5\x88\x86", "  \xe4\xbd\x8d\xe7\xbd\xae\xe5\xbe\xae\xe5\x88\x86",
        "  \xe9\x80\x9f\xe5\xba\xa6\xe6\xaf\x94\xe4\xbe\x8b", "  \xe9\x80\x9f\xe5\xba\xa6\xe7\xa7\xaf\xe5\x88\x86", "  \xe9\x80\x9f\xe5\xba\xa6\xe5\xbe\xae\xe5\x88\x86",
        "  \xe8\x88\xaa\xe5\x90\x91\xe5\xa2\x9e\xe7\x9b\x8a", "  \xe8\x88\xaa\xe5\x90\x91\xe6\xad\xbb\xe5\x8c\xba", "  \xe5\x9b\x9e\xe6\xad\xa3\xe9\x98\x88\xe5\x80\xbc", "  \xe5\x9b\x9e\xe6\xad\xa3\xe8\xa7\x92\xe9\x80\x9f"
    };
    uint8_t vi;

    for (vi = 0; vi < ROWS; vi++) {
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

static void pg_map(void)
{
    char buf[40];
    const vision_map_data_t *vmap = vision_map_get_data();

    if (wMapInfo[0] != 0) {
        snprintf(buf, sizeof(buf), "Conn:%c  Valid:%c  Dir:%s",
                 vmap->connected ? 'Y' : 'N',
                 vmap->valid ? 'Y' : 'N',
                 map_dir_name(vmap->car_dir));
        ips200pro_set_color(wMapInfo[0], COLOR_FOREGROUND, vmap->connected ? W_GREEN : W_RED);
        ips200pro_label_show_string(wMapInfo[0], buf);
    }

    if (wMapInfo[1] != 0) {
        snprintf(buf, sizeof(buf), "Good:%lu  Bad:%lu",
                 (unsigned long)vmap->rx_good,
                 (unsigned long)vmap->rx_bad);
        ips200pro_set_color(wMapInfo[1], COLOR_FOREGROUND, W_TEXT2);
        ips200pro_label_show_string(wMapInfo[1], buf);
    }

    if (wMapInfo[2] != 0) {
        snprintf(buf, sizeof(buf), "Car X:%u  Y:%u",
                 (unsigned)vmap->car_x,
                 (unsigned)vmap->car_y);
        ips200pro_set_color(wMapInfo[2], COLOR_FOREGROUND, W_TEXT);
        ips200pro_label_show_string(wMapInfo[2], buf);
    }

    map_refresh_rows(vmap);
    map_update_footer(vmap);
}

void display_show_page(uint8_t page_id) { (void)page_id; }

void display_update_100ms(void)
{
    menu_snapshot_t snap;
    if (!s_ok) return;
    snap = menu_get_snapshot();

    if (snap.current_page == MENU_PAGE_LOCKSCREEN) {
        pg_lockscreen();
        return;
    }

    if (snap.current_page != s_pp) {
        if (s_img_created) { ips200pro_set_hidden(wImg, 1); s_img_created = false; }
        ips200pro_set_color(wHdr, COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wHdr, COLOR_BACKGROUND, W_ACCENT);

        switch (snap.current_page) {
            case MENU_PAGE_HOME:       ips200pro_label_show_string(wHdr, "           \xe7\x89\xb9\xe7\xa7\x8d\xe9\x83\xa8\xe9\x98\x9f"); break;
            case MENU_PAGE_STATUS:     ips200pro_label_show_string(wHdr, "           \xe7\x8a\xb6\xe6\x80\x81\xe6\x80\xbb\xe8\xa7\x88"); break;
            case MENU_PAGE_MOTION:     ips200pro_label_show_string(wHdr, "           \xe8\xbf\x90\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95"); break;
            case MENU_PAGE_AXIS_TEST:  ips200pro_label_show_string(wHdr, "          \xe8\xbd\xb4\xe7\xa7\xbb\xe5\x8a\xa8\xe6\xb5\x8b\xe8\xaf\x95"); break;
            case MENU_PAGE_WHEEL_TEST: ips200pro_label_show_string(wHdr, "           \xe5\x8d\x95\xe8\xbd\xae\xe6\xb5\x8b\xe8\xaf\x95"); break;
            case MENU_PAGE_POSE:       ips200pro_label_show_string(wHdr, "           \xe5\xa7\xbf\xe6\x80\x81\xe9\x87\x8c\xe7\xa8\x8b"); break;
            case MENU_PAGE_PARAM:      ips200pro_label_show_string(wHdr, "           \xe5\x8f\x82\xe6\x95\xb0\xe8\xae\xbe\xe7\xbd\xae"); break;
            case MENU_PAGE_MAP:        ips200pro_label_show_string(wHdr, "           \xe5\x9c\xb0\xe5\x9b\xbe\xe6\x98\xbe\xe7\xa4\xba"); break;
            default: break;
        }
        s_pp = snap.current_page;
    }

    if (snap.current_page == MENU_PAGE_MAP) {
        if (!s_map_visible) {
            uint8_t i;
            for (i = 0; i < ROWS; i++) {
                ips200pro_set_hidden(wNm[i], 1);
                ips200pro_set_hidden(wVl[i], 1);
            }
            for (i = 0; i < MAP_INFO_ROWS; i++) {
                if (wMapInfo[i] != 0) ips200pro_set_hidden(wMapInfo[i], 0);
            }
            for (i = 0; i < GRID_MAP_H; i++) {
                if (wMapRows[i] != 0) ips200pro_set_hidden(wMapRows[i], 0);
            }
            s_map_visible = true;
        }
    } else {
        if (s_map_visible) {
            uint8_t i;
            for (i = 0; i < ROWS; i++) {
                ips200pro_set_hidden(wNm[i], 0);
                ips200pro_set_hidden(wVl[i], 0);
            }
            for (i = 0; i < MAP_INFO_ROWS; i++) {
                if (wMapInfo[i] != 0) ips200pro_set_hidden(wMapInfo[i], 1);
            }
            for (i = 0; i < GRID_MAP_H; i++) {
                if (wMapRows[i] != 0) ips200pro_set_hidden(wMapRows[i], 1);
            }
            s_map_visible = false;
        }
    }

    switch (snap.current_page) {
        case MENU_PAGE_HOME:       pg_home(&snap);        break;
        case MENU_PAGE_STATUS:     pg_status();           break;
        case MENU_PAGE_MOTION:     pg_motion(&snap);      break;
        case MENU_PAGE_AXIS_TEST:  pg_axis_test(&snap);   break;
        case MENU_PAGE_WHEEL_TEST: pg_wheel_test(&snap);  break;
        case MENU_PAGE_POSE:       pg_pose();             break;
        case MENU_PAGE_PARAM:      pg_param(&snap);       break;
        case MENU_PAGE_MAP:        pg_map();              break;
        default: break;
    }

    {
        const proto_velocity_t *p_vel = protocol_get_velocity();
        const proto_ctx_t      *p_ctx = protocol_get_ctx();
        char dbg[64];

        if (snap.current_page == MENU_PAGE_AXIS_TEST && motion_manager_is_move_active()) {
            imu_state_t ft_imu = imu_get_state();
            snprintf(dbg, sizeof(dbg), "YAW:%.1f TGT:%.0f CUR:%.1f", ft_imu.yaw_deg, motion_manager_get_move_target_value(), motion_manager_get_move_current_value());
            ips200pro_set_color(wFt, COLOR_FOREGROUND, W_ACCENT);
            ips200pro_label_show_string(wFt, dbg);
            return;
        }

        if (snap.current_page == MENU_PAGE_MAP) {
            return;
        }

        if (!p_vel->failsafe) {
            snprintf(dbg, sizeof(dbg), " 已连接 成:%lu 败:%lu", (unsigned long)p_ctx->rx_good, (unsigned long)p_ctx->rx_bad);
            ips200pro_set_color(wFt, COLOR_FOREGROUND, W_GREEN);
        } else {
            snprintf(dbg, sizeof(dbg), " 等待中.. 成:%lu 败:%lu", (unsigned long)p_ctx->rx_good, (unsigned long)p_ctx->rx_bad);
            ips200pro_set_color(wFt, COLOR_FOREGROUND, W_DIM);
        }
        ips200pro_label_show_string(wFt, dbg);
    }
}
