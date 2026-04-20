#include <stdio.h>
#include "lvgl.h"
#include "custom.h"
#include "protocol.h"
#include "ui_user_events.h"

lv_ui guider_ui;

#define CLR_PRIMARY         0x4A90D9
#define CLR_TEXT_MAIN       0xEAEAEA
#define CLR_TEXT_SUB        0xA0A0A0
#define CLR_STATUS_ON       0x5BE7A9
#define CLR_STATUS_OFF      0xFF6B6B
#define CLR_EDIT            0xF5B041

#define PID_TUNE_ROW_X      8
#define PID_TUNE_ROW_W      224
#define PID_TUNE_ROW_H      18
#define PID_TUNE_ROW_GAP    3
#define PID_TUNE_ROW_Y0     62

typedef enum
{
    PID_TUNE_ITEM_SEND_ALL = 0,
    PID_TUNE_ITEM_WHEEL_KP,
    PID_TUNE_ITEM_WHEEL_KI,
    PID_TUNE_ITEM_WHEEL_KD,
    PID_TUNE_ITEM_WHEEL_OUTPUT_LIMIT,
    PID_TUNE_ITEM_SPEED_LIMIT,
    PID_TUNE_ITEM_POSITION_KP,
    PID_TUNE_ITEM_POSITION_KI,
    PID_TUNE_ITEM_POSITION_KD,
    PID_TUNE_ITEM_POSITION_SPEED_LIMIT,
} pid_tune_item_id_t;

typedef struct
{
    const char *name;
    uint8 param_id;
    int32_t value;
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    bool scaled;
} pid_tune_item_t;

static pid_tune_item_t s_pid_items[PID_TUNE_ITEM_COUNT] =
{
    {"全部下发",        0,                               0,    0,    0,   0, false},
    {"轮速 Kp",         PROTO_PARAM_WHEEL_KP,           2200, 0, 6000,  50, true },
    {"轮速 Ki",         PROTO_PARAM_WHEEL_KI,            120, 0, 1200,  10, true },
    {"轮速 Kd",         PROTO_PARAM_WHEEL_KD,              0, 0, 1200,  10, true },
    {"轮速输出限幅",     PROTO_PARAM_WHEEL_OUTPUT_LIMIT, 5200, 0, 9000, 100, false},
    {"整车速度限幅",     PROTO_PARAM_SPEED_LIMIT,         420, 50, 1000,  10, false},
    {"位置 Kp",         PROTO_PARAM_POSITION_KP,         900, 0, 3000,  50, true },
    {"位置 Ki",         PROTO_PARAM_POSITION_KI,           0, 0, 1200,  10, true },
    {"位置 Kd",         PROTO_PARAM_POSITION_KD,           0, 0, 1200,  10, true },
    {"位置速度限幅",     PROTO_PARAM_POSITION_SPEED_LIMIT,320, 50, 1000,  10, false},
};

static uint8_t s_pid_selected = PID_TUNE_ITEM_SEND_ALL;
static bool s_pid_edit_mode = false;
static int32_t s_pid_backup_value = 0;

static int32_t pid_tune_clamp_value(const pid_tune_item_t *item, int32_t value)
{
    if (value < item->min_value) {
        return item->min_value;
    }
    if (value > item->max_value) {
        return item->max_value;
    }
    return value;
}

static void pid_tune_send_item(uint8_t idx)
{
    if ((idx >= PID_TUNE_ITEM_COUNT) || (s_pid_items[idx].param_id == 0U)) {
        return;
    }

    protocol_send_param_set(s_pid_items[idx].param_id, s_pid_items[idx].value);
}

static void pid_tune_send_all(void)
{
    uint8_t i;

    for (i = 1U; i < PID_TUNE_ITEM_COUNT; ++i) {
        pid_tune_send_item(i);
    }
}

static void pid_tune_format_value(const pid_tune_item_t *item, char *buf, uint32_t size)
{
    if ((item == NULL) || (buf == NULL) || (size == 0U)) {
        return;
    }

    if (item->param_id == 0U) {
        snprintf(buf, size, "发送");
    } else if (item->scaled) {
        snprintf(buf, size, "%ld.%03ld",
                 (long)(item->value / 1000),
                 (long)(item->value % 1000));
    } else {
        snprintf(buf, size, "%ld", (long)item->value);
    }
}

static void pid_tune_refresh(lv_ui *ui)
{
    uint8_t i;
    char value_buf[24];

    if ((ui == NULL) || (ui->pid_tune == NULL)) {
        return;
    }

    if (ui->pid_tune_title_label != NULL) {
        lv_label_set_text(ui->pid_tune_title_label, "PID 参数调整");
    }

    if (ui->pid_tune_mode_label != NULL) {
        if (s_pid_edit_mode) {
            lv_label_set_text(ui->pid_tune_mode_label, "编辑");
            lv_obj_set_style_text_color(ui->pid_tune_mode_label, lv_color_hex(CLR_EDIT),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(ui->pid_tune_mode_label, "选择");
            lv_obj_set_style_text_color(ui->pid_tune_mode_label, lv_color_hex(CLR_STATUS_ON),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    if (ui->pid_tune_hint_label != NULL) {
        if (s_pid_edit_mode) {
            lv_label_set_text(ui->pid_tune_hint_label, "上下调值  A发送  B取消");
        } else if (s_pid_selected == PID_TUNE_ITEM_SEND_ALL) {
            lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A全部发送  B返回");
        } else {
            lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A编辑  B返回");
        }
    }

    for (i = 0U; i < PID_TUNE_ITEM_COUNT; ++i) {
        lv_color_t line_color;

        if (ui->pid_tune_row_name_label[i] != NULL) {
            lv_label_set_text(ui->pid_tune_row_name_label[i], s_pid_items[i].name);
        }

        if (ui->pid_tune_row_value_label[i] != NULL) {
            pid_tune_format_value(&s_pid_items[i], value_buf, sizeof(value_buf));
            lv_label_set_text(ui->pid_tune_row_value_label[i], value_buf);
        }

        if (ui->pid_tune_row[i] == NULL) {
            continue;
        }

        if (i == s_pid_selected) {
            line_color = s_pid_edit_mode ? lv_color_hex(CLR_EDIT) : lv_color_hex(CLR_PRIMARY);
            lv_obj_set_style_bg_opa(ui->pid_tune_row[i], 110, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(ui->pid_tune_row[i], lv_color_hex(0x111111),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(ui->pid_tune_row[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(ui->pid_tune_row[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(ui->pid_tune_row[i], line_color,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            line_color = lv_color_hex(CLR_TEXT_MAIN);
            lv_obj_set_style_bg_opa(ui->pid_tune_row[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(ui->pid_tune_row[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (ui->pid_tune_row_name_label[i] != NULL) {
            lv_obj_set_style_text_color(ui->pid_tune_row_name_label[i], line_color,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (ui->pid_tune_row_value_label[i] != NULL) {
            lv_obj_set_style_text_color(ui->pid_tune_row_value_label[i], line_color,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

static void create_pid_tune_row(lv_ui *ui, uint8_t idx, lv_coord_t y)
{
    lv_obj_t *row;
    lv_obj_t *name_label;
    lv_obj_t *value_label;

    row = lv_obj_create(ui->pid_tune);
    lv_obj_set_pos(row, PID_TUNE_ROW_X, y);
    lv_obj_set_size(row, PID_TUNE_ROW_W, PID_TUNE_ROW_H);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(row, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->pid_tune_row[idx] = row;

    name_label = lv_label_create(row);
    lv_obj_set_pos(name_label, 8, 1);
    lv_obj_set_size(name_label, 118, 16);
    lv_obj_set_style_text_font(name_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(name_label, lv_color_hex(CLR_TEXT_MAIN),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->pid_tune_row_name_label[idx] = name_label;

    value_label = lv_label_create(row);
    lv_obj_set_pos(value_label, 132, 1);
    lv_obj_set_size(value_label, 82, 16);
    lv_obj_set_style_text_font(value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(value_label, lv_color_hex(CLR_TEXT_MAIN),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->pid_tune_row_value_label[idx] = value_label;
}

void setup_scr_pid_tune(lv_ui *ui)
{
    uint8_t i;

    ui->pid_tune = lv_obj_create(NULL);
    lv_obj_set_size(ui->pid_tune, 240, 320);
    lv_obj_set_scrollbar_mode(ui->pid_tune, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->pid_tune, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_bg_img = lv_img_create(ui->pid_tune);
    lv_obj_add_flag(ui->pid_tune_bg_img, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->pid_tune_bg_img, &_back_alpha_240x320);
    lv_obj_set_pos(ui->pid_tune_bg_img, 0, 0);
    lv_obj_set_size(ui->pid_tune_bg_img, 240, 320);
    lv_obj_set_style_img_opa(ui->pid_tune_bg_img, 181, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_status_bar = lv_obj_create(ui->pid_tune);
    lv_obj_set_pos(ui->pid_tune_status_bar, 8, 8);
    lv_obj_set_size(ui->pid_tune_status_bar, 224, 32);
    lv_obj_clear_flag(ui->pid_tune_status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui->pid_tune_status_bar, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->pid_tune_status_bar, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->pid_tune_status_bar, lv_color_hex(0x000000),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->pid_tune_status_bar, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->pid_tune_status_bar, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->pid_tune_status_bar, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->pid_tune_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->pid_tune_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_battery_label = lv_label_create(ui->pid_tune_status_bar);
    lv_label_set_text(ui->pid_tune_battery_label, "--");
    lv_obj_set_pos(ui->pid_tune_battery_label, 14, 8);
    lv_obj_set_size(ui->pid_tune_battery_label, 60, 16);
    lv_obj_set_style_text_color(ui->pid_tune_battery_label, lv_color_hex(CLR_STATUS_ON),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->pid_tune_battery_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_status_title_label = lv_label_create(ui->pid_tune_status_bar);
    lv_label_set_text_static(ui->pid_tune_status_title_label, "连接:");
    lv_obj_set_pos(ui->pid_tune_status_title_label, 104, 8);
    lv_obj_set_size(ui->pid_tune_status_title_label, 40, 16);
    lv_obj_set_style_text_color(ui->pid_tune_status_title_label, lv_color_hex(CLR_TEXT_SUB),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->pid_tune_status_title_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_status_value_label = lv_label_create(ui->pid_tune_status_bar);
    lv_label_set_text(ui->pid_tune_status_value_label, "离线");
    lv_obj_set_pos(ui->pid_tune_status_value_label, 150, 8);
    lv_obj_set_size(ui->pid_tune_status_value_label, 56, 16);
    lv_obj_set_style_text_color(ui->pid_tune_status_value_label, lv_color_hex(CLR_STATUS_OFF),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->pid_tune_status_value_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->pid_tune_status_value_label, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_title_label = lv_label_create(ui->pid_tune);
    lv_label_set_text(ui->pid_tune_title_label, "PID 参数调整");
    lv_obj_set_pos(ui->pid_tune_title_label, 12, 44);
    lv_obj_set_size(ui->pid_tune_title_label, 144, 16);
    lv_obj_set_style_text_font(ui->pid_tune_title_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->pid_tune_title_label, lv_color_hex(CLR_TEXT_MAIN),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_mode_label = lv_label_create(ui->pid_tune);
    lv_label_set_text(ui->pid_tune_mode_label, "选择");
    lv_obj_set_pos(ui->pid_tune_mode_label, 166, 44);
    lv_obj_set_size(ui->pid_tune_mode_label, 60, 16);
    lv_obj_set_style_text_font(ui->pid_tune_mode_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->pid_tune_mode_label, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    for (i = 0U; i < PID_TUNE_ITEM_COUNT; ++i) {
        create_pid_tune_row(ui, i, (lv_coord_t)(PID_TUNE_ROW_Y0 + i * (PID_TUNE_ROW_H + PID_TUNE_ROW_GAP)));
    }

    ui->pid_tune_hint_label = lv_label_create(ui->pid_tune);
    lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A编辑  B返回");
    lv_obj_set_pos(ui->pid_tune_hint_label, 10, 286);
    lv_obj_set_size(ui->pid_tune_hint_label, 220, 24);
    lv_obj_set_style_text_font(ui->pid_tune_hint_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->pid_tune_hint_label, lv_color_hex(CLR_TEXT_SUB),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->pid_tune_hint_label, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_update_layout(ui->pid_tune);
    ui_user_hook_pid_tune(ui);
    custom_pid_tune_on_show(ui);
}

void custom_init(lv_ui *ui)
{
    LV_UNUSED(ui);
}

void custom_pid_tune_on_show(lv_ui *ui)
{
    s_pid_edit_mode = false;
    if (s_pid_selected >= PID_TUNE_ITEM_COUNT) {
        s_pid_selected = PID_TUNE_ITEM_SEND_ALL;
    }
    pid_tune_refresh(ui);
}

bool custom_pid_tune_handle_key(lv_ui *ui, uint32_t key, bool *request_back)
{
    pid_tune_item_t *item;

    if (request_back != NULL) {
        *request_back = false;
    }

    if (ui == NULL) {
        return false;
    }

    item = &s_pid_items[s_pid_selected];

    switch (key) {
        case LV_KEY_ENTER:
            if (!s_pid_edit_mode) {
                if (s_pid_selected == PID_TUNE_ITEM_SEND_ALL) {
                    pid_tune_send_all();
                } else {
                    s_pid_backup_value = item->value;
                    s_pid_edit_mode = true;
                }
            } else {
                pid_tune_send_item(s_pid_selected);
                s_pid_edit_mode = false;
            }
            pid_tune_refresh(ui);
            return true;

        case LV_KEY_ESC:
            if (s_pid_edit_mode) {
                item->value = s_pid_backup_value;
                s_pid_edit_mode = false;
                pid_tune_refresh(ui);
            } else if (request_back != NULL) {
                *request_back = true;
            }
            return true;

        case LV_KEY_PREV:
        case LV_KEY_UP:
            if (s_pid_edit_mode && (item->param_id != 0U)) {
                item->value = pid_tune_clamp_value(item, item->value - item->step);
            } else if (!s_pid_edit_mode) {
                s_pid_selected = (s_pid_selected == 0U) ? (PID_TUNE_ITEM_COUNT - 1U)
                                                        : (uint8_t)(s_pid_selected - 1U);
            }
            pid_tune_refresh(ui);
            return true;

        case LV_KEY_NEXT:
        case LV_KEY_DOWN:
            if (s_pid_edit_mode && (item->param_id != 0U)) {
                item->value = pid_tune_clamp_value(item, item->value + item->step);
            } else if (!s_pid_edit_mode) {
                s_pid_selected = (uint8_t)((s_pid_selected + 1U) % PID_TUNE_ITEM_COUNT);
            }
            pid_tune_refresh(ui);
            return true;

        default:
            return false;
    }
}
