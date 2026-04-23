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

#define PID_TUNE_ROW_X      16
#define PID_TUNE_ROW_W      208
#define PID_TUNE_ROW_H      38

typedef enum {
    PID_STATE_MENU = 0,
    PID_STATE_PARAMS
} pid_tune_state_t;

typedef enum {
    PID_MENU_WHEEL = 0,
    PID_MENU_POSITION,
    PID_MENU_OTHERS,
    PID_MENU_COUNT
} pid_menu_id_t;

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

static pid_tune_item_t s_wheel_items[] =
{
    {"全部下发",        0,                               0,    0,    0,   0, false},
    {"轮速 Kp",         PROTO_PARAM_WHEEL_KP,           2200, 0, 6000,  50, true },
    {"轮速 Ki",         PROTO_PARAM_WHEEL_KI,            120, 0, 1200,  10, true },
    {"轮速 Kd",         PROTO_PARAM_WHEEL_KD,              0, 0, 1200,  10, true },
    {"轮速输出限幅",     PROTO_PARAM_WHEEL_OUTPUT_LIMIT, 5200, 0, 9000, 100, false},
};

static pid_tune_item_t s_pos_items[] =
{
    {"全部下发",        0,                               0,    0,    0,   0, false},
    {"位置 Kp",         PROTO_PARAM_POSITION_KP,         900, 0, 3000,  50, true },
    {"位置 Ki",         PROTO_PARAM_POSITION_KI,           0, 0, 1200,  10, true },
    {"位置 Kd",         PROTO_PARAM_POSITION_KD,           0, 0, 1200,  10, true },
    {"位置速度限幅",     PROTO_PARAM_POSITION_SPEED_LIMIT,320, 50, 1000,  10, false},
};

static pid_tune_item_t s_other_items[] =
{
    {"全部下发",        0,                               0,    0,    0,   0, false},
    {"整车速度限幅",     PROTO_PARAM_SPEED_LIMIT,         420, 50, 1000,  10, false},
};

static pid_tune_state_t s_pid_state = PID_STATE_MENU;
static uint8_t s_menu_selected = PID_MENU_WHEEL;
static uint8_t s_pid_selected = 0;
static bool s_pid_edit_mode = false;
static int32_t s_pid_backup_value = 0;

static lv_obj_t *s_row_arrows[PID_TUNE_ITEM_COUNT];

static pid_tune_item_t* get_current_items(uint8_t *count)
{
    if (s_menu_selected == PID_MENU_WHEEL) {
        *count = sizeof(s_wheel_items) / sizeof(s_wheel_items[0]);
        return s_wheel_items;
    } else if (s_menu_selected == PID_MENU_POSITION) {
        *count = sizeof(s_pos_items) / sizeof(s_pos_items[0]);
        return s_pos_items;
    } else {
        *count = sizeof(s_other_items) / sizeof(s_other_items[0]);
        return s_other_items;
    }
}

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
    uint8_t count = 0;
    pid_tune_item_t *items = get_current_items(&count);

    if ((idx >= count) || (items[idx].param_id == 0U)) {
        return;
    }

    protocol_send_param_set(items[idx].param_id, items[idx].value);
}

static void pid_tune_send_all(void)
{
    uint8_t i;
    uint8_t count = 0;
    get_current_items(&count);

    for (i = 1U; i < count; ++i) {
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
    uint8_t active_count = 0;
    pid_tune_item_t *active_items = NULL;

    if ((ui == NULL) || (ui->pid_tune == NULL)) {
        return;
    }

    lv_coord_t y_start;
    lv_coord_t area_h;
    lv_coord_t row_h;
    lv_coord_t gap = 0;
    const lv_font_t *font_to_use;
    lv_coord_t font_h;

    if (s_pid_state == PID_STATE_MENU) {
        if (ui->pid_tune_title_label != NULL) {
            lv_obj_add_flag(ui->pid_tune_title_label, LV_OBJ_FLAG_HIDDEN);
        }

        if (ui->pid_tune_mode_label != NULL) {
            lv_obj_add_flag(ui->pid_tune_mode_label, LV_OBJ_FLAG_HIDDEN);
        }

        if (ui->pid_tune_hint_label != NULL) {
            lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A选择  B返回");
        }

        const char *menu_names[PID_MENU_COUNT] = {"轮速", "位置", "整车"};
        active_count = PID_MENU_COUNT;

        row_h = 58;
        y_start = 54;
        area_h = 228;
        font_to_use = &lv_font_ALMM_23;
        font_h = 23;

        if (active_count > 1) {
            gap = (area_h - (active_count * row_h)) / (active_count - 1);
        } else {
            y_start = y_start + (area_h - row_h) / 2;
        }

        for (i = 0U; i < PID_TUNE_ITEM_COUNT; ++i) {
            if (ui->pid_tune_row[i] == NULL) {
                continue;
            }

            if (i < active_count) {
                lv_obj_clear_flag(ui->pid_tune_row[i], LV_OBJ_FLAG_HIDDEN);
                
                lv_coord_t y_pos = y_start + i * (row_h + gap);
                lv_obj_set_size(ui->pid_tune_row[i], PID_TUNE_ROW_W, row_h);
                lv_obj_set_pos(ui->pid_tune_row[i], PID_TUNE_ROW_X, y_pos);
                
                lv_obj_set_style_text_font(s_row_arrows[i], font_to_use, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_pos(s_row_arrows[i], 14, (row_h - font_h) / 2);
                lv_obj_set_size(s_row_arrows[i], 20, font_h);

                if (ui->pid_tune_row_name_label[i] != NULL) {
                    lv_label_set_text(ui->pid_tune_row_name_label[i], menu_names[i]);
                    lv_obj_set_style_text_font(ui->pid_tune_row_name_label[i], font_to_use, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_pos(ui->pid_tune_row_name_label[i], 38, (row_h - font_h) / 2);
                    lv_obj_set_size(ui->pid_tune_row_name_label[i], 140, font_h);
                }
                if (ui->pid_tune_row_value_label[i] != NULL) {
                    lv_label_set_text(ui->pid_tune_row_value_label[i], ""); /* 菜单层不显示值 */
                    lv_obj_set_style_text_font(ui->pid_tune_row_value_label[i], font_to_use, LV_PART_MAIN | LV_STATE_DEFAULT);
                }

                lv_color_t line_color;
                if (i == s_menu_selected) {
                    line_color = lv_color_hex(CLR_PRIMARY);
                    lv_obj_set_style_bg_opa(ui->pid_tune_row[i], 160, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(ui->pid_tune_row[i], lv_color_hex(0x000000),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(ui->pid_tune_row[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(ui->pid_tune_row[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(ui->pid_tune_row[i], line_color,
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    
                    lv_obj_set_style_text_opa(s_row_arrows[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(s_row_arrows[i], line_color, LV_PART_MAIN | LV_STATE_DEFAULT);
                } else {
                    line_color = lv_color_hex(CLR_TEXT_MAIN);
                    lv_obj_set_style_bg_opa(ui->pid_tune_row[i], 120, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(ui->pid_tune_row[i], lv_color_hex(0x000000),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(ui->pid_tune_row[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(ui->pid_tune_row[i], 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(ui->pid_tune_row[i], lv_color_hex(0xFFFFFF),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                                                  
                    lv_obj_set_style_text_opa(s_row_arrows[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                }

                if (ui->pid_tune_row_name_label[i] != NULL) {
                    lv_obj_set_style_text_color(ui->pid_tune_row_name_label[i], line_color,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                if (ui->pid_tune_row_value_label[i] != NULL) {
                    lv_obj_set_style_text_color(ui->pid_tune_row_value_label[i], line_color,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            } else {
                lv_obj_add_flag(ui->pid_tune_row[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else { // PID_STATE_PARAMS
        active_items = get_current_items(&active_count);
        
        if (ui->pid_tune_title_label != NULL) {
            lv_obj_clear_flag(ui->pid_tune_title_label, LV_OBJ_FLAG_HIDDEN);
            if (s_menu_selected == PID_MENU_WHEEL) {
                lv_label_set_text(ui->pid_tune_title_label, "轮速");
            } else if (s_menu_selected == PID_MENU_POSITION) {
                lv_label_set_text(ui->pid_tune_title_label, "位置");
            } else {
                lv_label_set_text(ui->pid_tune_title_label, "整车");
            }
        }

        if (ui->pid_tune_mode_label != NULL) {
            lv_obj_clear_flag(ui->pid_tune_mode_label, LV_OBJ_FLAG_HIDDEN);
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
            } else if (s_pid_selected == 0) {
                lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A全部下发  B返回");
            } else {
                lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A编辑  B返回");
            }
        }

        y_start = 64;
        area_h = 216;

        if (active_count <= 3) {
            row_h = 58;
            font_to_use = &lv_font_ALMM_23;
            font_h = 23;
        } else {
            row_h = 38;
            font_to_use = &lv_font_ALMM_16;
            font_h = 16;
        }

        if (active_count > 1) {
            gap = (area_h - (active_count * row_h)) / (active_count - 1);
        } else {
            y_start = y_start + (area_h - row_h) / 2;
        }

        for (i = 0U; i < PID_TUNE_ITEM_COUNT; ++i) {
            if (ui->pid_tune_row[i] == NULL) {
                continue;
            }

            if (i < active_count) {
                lv_obj_clear_flag(ui->pid_tune_row[i], LV_OBJ_FLAG_HIDDEN);
                
                lv_coord_t y_pos = y_start + i * (row_h + gap);
                lv_obj_set_size(ui->pid_tune_row[i], PID_TUNE_ROW_W, row_h);
                lv_obj_set_pos(ui->pid_tune_row[i], PID_TUNE_ROW_X, y_pos);

                lv_obj_set_style_text_font(s_row_arrows[i], font_to_use, LV_PART_MAIN | LV_STATE_DEFAULT);
                if (font_h == 23) {
                    lv_obj_set_pos(s_row_arrows[i], 14, (row_h - font_h) / 2);
                    lv_obj_set_size(s_row_arrows[i], 20, font_h);
                } else {
                    lv_obj_set_pos(s_row_arrows[i], 12, (row_h - font_h) / 2);
                    lv_obj_set_size(s_row_arrows[i], 16, font_h);
                }

                if (ui->pid_tune_row_name_label[i] != NULL) {
                    lv_label_set_text(ui->pid_tune_row_name_label[i], active_items[i].name);
                    lv_obj_set_style_text_font(ui->pid_tune_row_name_label[i], font_to_use, LV_PART_MAIN | LV_STATE_DEFAULT);
                    
                    if (font_h == 23) {
                        lv_obj_set_pos(ui->pid_tune_row_name_label[i], 34, (row_h - font_h) / 2);
                        lv_obj_set_size(ui->pid_tune_row_name_label[i], 100, font_h);
                    } else {
                        lv_obj_set_pos(ui->pid_tune_row_name_label[i], 28, (row_h - font_h) / 2);
                        lv_obj_set_size(ui->pid_tune_row_name_label[i], 110, font_h);
                    }
                }

                if (ui->pid_tune_row_value_label[i] != NULL) {
                    pid_tune_format_value(&active_items[i], value_buf, sizeof(value_buf));
                    lv_label_set_text(ui->pid_tune_row_value_label[i], value_buf);
                    lv_obj_set_style_text_font(ui->pid_tune_row_value_label[i], font_to_use, LV_PART_MAIN | LV_STATE_DEFAULT);
                    
                    if (font_h == 23) {
                        lv_obj_set_pos(ui->pid_tune_row_value_label[i], 134, (row_h - font_h) / 2);
                        lv_obj_set_size(ui->pid_tune_row_value_label[i], 64, font_h);
                    } else {
                        lv_obj_set_pos(ui->pid_tune_row_value_label[i], 138, (row_h - font_h) / 2);
                        lv_obj_set_size(ui->pid_tune_row_value_label[i], 60, font_h);
                    }
                }

                lv_color_t line_color;
                if (i == s_pid_selected) {
                    line_color = s_pid_edit_mode ? lv_color_hex(CLR_EDIT) : lv_color_hex(CLR_PRIMARY);
                    lv_obj_set_style_bg_opa(ui->pid_tune_row[i], 160, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(ui->pid_tune_row[i], lv_color_hex(0x000000),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(ui->pid_tune_row[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(ui->pid_tune_row[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(ui->pid_tune_row[i], line_color,
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                                                  
                    lv_obj_set_style_text_opa(s_row_arrows[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(s_row_arrows[i], line_color, LV_PART_MAIN | LV_STATE_DEFAULT);
                } else {
                    line_color = lv_color_hex(CLR_TEXT_MAIN);
                    lv_obj_set_style_bg_opa(ui->pid_tune_row[i], 120, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(ui->pid_tune_row[i], lv_color_hex(0x000000),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(ui->pid_tune_row[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_opa(ui->pid_tune_row[i], 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(ui->pid_tune_row[i], lv_color_hex(0xFFFFFF),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                                                  
                    lv_obj_set_style_text_opa(s_row_arrows[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                }

                if (ui->pid_tune_row_name_label[i] != NULL) {
                    lv_obj_set_style_text_color(ui->pid_tune_row_name_label[i], line_color,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                }

                if (ui->pid_tune_row_value_label[i] != NULL) {
                    lv_obj_set_style_text_color(ui->pid_tune_row_value_label[i], line_color,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            } else {
                lv_obj_add_flag(ui->pid_tune_row[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void create_pid_tune_row(lv_ui *ui, uint8_t idx, lv_coord_t y)
{
    lv_obj_t *row;
    lv_obj_t *name_label;
    lv_obj_t *value_label;
    lv_obj_t *arrow_label;

    row = lv_obj_create(ui->pid_tune);
    lv_obj_set_pos(row, PID_TUNE_ROW_X, y);
    lv_obj_set_size(row, PID_TUNE_ROW_W, PID_TUNE_ROW_H);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(row, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(row, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(row, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->pid_tune_row[idx] = row;

    arrow_label = lv_label_create(row);
    lv_label_set_text_static(arrow_label, ">");
    lv_obj_set_pos(arrow_label, 12, (PID_TUNE_ROW_H - 16) / 2);
    lv_obj_set_size(arrow_label, 16, 16);
    lv_obj_set_style_text_font(arrow_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(arrow_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    s_row_arrows[idx] = arrow_label;

    name_label = lv_label_create(row);
    lv_obj_set_pos(name_label, 32, (PID_TUNE_ROW_H - 16) / 2);
    lv_obj_set_size(name_label, 100, 16);
    lv_obj_set_style_text_font(name_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(name_label, lv_color_hex(CLR_TEXT_MAIN),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->pid_tune_row_name_label[idx] = name_label;

    value_label = lv_label_create(row);
    lv_obj_set_pos(value_label, 116, (PID_TUNE_ROW_H - 16) / 2);
    lv_obj_set_size(value_label, 78, 16);
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
    lv_label_set_text(ui->pid_tune_title_label, "参数调整");
    lv_obj_set_pos(ui->pid_tune_title_label, 16, 42);
    lv_obj_set_size(ui->pid_tune_title_label, 144, 16);
    lv_obj_set_style_text_font(ui->pid_tune_title_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->pid_tune_title_label, lv_color_hex(CLR_TEXT_MAIN),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->pid_tune_mode_label = lv_label_create(ui->pid_tune);
    lv_label_set_text(ui->pid_tune_mode_label, "选择");
    lv_obj_set_pos(ui->pid_tune_mode_label, 164, 42);
    lv_obj_set_size(ui->pid_tune_mode_label, 60, 16);
    lv_obj_set_style_text_font(ui->pid_tune_mode_label, &lv_font_ALMM_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->pid_tune_mode_label, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    for (i = 0U; i < PID_TUNE_ITEM_COUNT; ++i) {
        /* 初始化时先不分配准确的 Y 坐标，refresh 时会动态分配间隔 */
        create_pid_tune_row(ui, i, 0);
    }

    ui->pid_tune_hint_label = lv_label_create(ui->pid_tune);
    lv_label_set_text(ui->pid_tune_hint_label, "上下选择  A选择  B返回");
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
    s_pid_state = PID_STATE_MENU;
    s_pid_edit_mode = false;
    s_menu_selected = PID_MENU_WHEEL;
    s_pid_selected = 0;
    pid_tune_refresh(ui);
}

bool custom_pid_tune_handle_key(lv_ui *ui, uint32_t key, bool *request_back)
{
    pid_tune_item_t *item;
    uint8_t count = 0;
    pid_tune_item_t *active_items = get_current_items(&count);

    if (request_back != NULL) {
        *request_back = false;
    }

    if (ui == NULL) {
        return false;
    }

    if (s_pid_state == PID_STATE_MENU) {
        switch (key) {
            case LV_KEY_ENTER:
                s_pid_state = PID_STATE_PARAMS;
                s_pid_selected = 0;
                s_pid_edit_mode = false;
                pid_tune_refresh(ui);
                return true;

            case LV_KEY_ESC:
                if (request_back != NULL) {
                    *request_back = true;
                }
                return true;

            case LV_KEY_PREV:
            case LV_KEY_UP:
                s_menu_selected = (s_menu_selected == 0U) ? (uint8_t)(PID_MENU_COUNT - 1U) : (uint8_t)(s_menu_selected - 1U);
                pid_tune_refresh(ui);
                return true;

            case LV_KEY_NEXT:
            case LV_KEY_DOWN:
                s_menu_selected = (uint8_t)((s_menu_selected + 1U) % PID_MENU_COUNT);
                pid_tune_refresh(ui);
                return true;

            default:
                return false;
        }
    } else {
        if (s_pid_selected >= count) {
            s_pid_selected = 0;
        }
        item = &active_items[s_pid_selected];

        switch (key) {
            case LV_KEY_ENTER:
                if (!s_pid_edit_mode) {
                    if (s_pid_selected == 0U) {
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
                } else {
                    s_pid_state = PID_STATE_MENU;
                    pid_tune_refresh(ui);
                }
                return true;

            case LV_KEY_PREV:
            case LV_KEY_UP:
                if (s_pid_edit_mode && (item->param_id != 0U)) {
                    item->value = pid_tune_clamp_value(item, item->value - item->step);
                } else if (!s_pid_edit_mode) {
                    s_pid_selected = (s_pid_selected == 0U) ? (uint8_t)(count - 1U) : (uint8_t)(s_pid_selected - 1U);
                }
                pid_tune_refresh(ui);
                return true;

            case LV_KEY_NEXT:
            case LV_KEY_DOWN:
                if (s_pid_edit_mode && (item->param_id != 0U)) {
                    item->value = pid_tune_clamp_value(item, item->value + item->step);
                } else if (!s_pid_edit_mode) {
                    s_pid_selected = (uint8_t)((s_pid_selected + 1U) % count);
                }
                pid_tune_refresh(ui);
                return true;

            default:
                return false;
        }
    }
}
