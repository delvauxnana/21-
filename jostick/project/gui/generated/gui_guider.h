#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define MENU_CARD_COUNT      3
#define PID_TUNE_ITEM_COUNT  10

typedef struct
{
    lv_obj_t *lockscreen;
    bool lockscreen_del;
    lv_obj_t *lockscreen_lockscreen_bg_img;
    lv_obj_t *lockscreen_lockscreen_title_label;
    lv_obj_t *lockscreen_lockscreen_author_label;
    lv_obj_t *lockscreen_lockscreen_hint_container;
    lv_obj_t *lockscreen_lockscreen_hint_label;

    lv_obj_t *menu;
    bool menu_del;
    lv_obj_t *menu_menu_bg_img;
    lv_obj_t *menu_menu_status_bar;
    lv_obj_t *menu_menu_status_value_label;
    lv_obj_t *menu_menu_status_title_label;
    lv_obj_t *menu_menu_battery_label;
    lv_obj_t *menu_menu_card[MENU_CARD_COUNT];
    lv_obj_t *menu_menu_card_label[MENU_CARD_COUNT];
    lv_obj_t *menu_menu_card_arrow[MENU_CARD_COUNT];
    lv_obj_t *menu_menu_footer;
    lv_obj_t *menu_menu_footer_label;

    lv_obj_t *joystick_mode;
    bool joystick_mode_del;
    lv_obj_t *joystick_mode_joystick_mode_bg_img;
    lv_obj_t *joystick_mode_joystick_mode_right_stick_cross_v_line;
    lv_obj_t *joystick_mode_joystick_mode_right_stick_cross_h_line;
    lv_obj_t *joystick_mode_joystick_mode_left_stick_cross_v_line;
    lv_obj_t *joystick_mode_joystick_mode_left_stick_cross_h_line;
    lv_obj_t *joystick_mode_joystick_mode_right_stick_area;
    lv_obj_t *joystick_mode_joystick_mode_right_stick_knob;
    lv_obj_t *joystick_mode_joystick_mode_left_stick_area;
    lv_obj_t *joystick_mode_joystick_mode_left_stick_knob;
    lv_obj_t *joystick_mode_joystick_mode_status_bar;
    lv_obj_t *joystick_mode_joystick_mode_status_value_label;
    lv_obj_t *joystick_mode_joystick_mode_status_title_label;
    lv_obj_t *joystick_mode_joystick_mode_battery_label;
    lv_obj_t *joystick_mode_joystick_mode_speed_panel;
    lv_obj_t *joystick_mode_joystick_mode_speed_title_label;
    lv_obj_t *joystick_mode_joystick_mode_axis_x_name_label;
    lv_obj_t *joystick_mode_joystick_mode_axis_y_name_label;
    lv_obj_t *joystick_mode_joystick_mode_axis_z_name_label;
    lv_obj_t *joystick_mode_joystick_mode_axis_z_value_label;
    lv_obj_t *joystick_mode_joystick_mode_axis_y_value_label;
    lv_obj_t *joystick_mode_joystick_mode_axis_x_value_label;
    lv_obj_t *joystick_mode_joystick_mode_left_stick_title_label;
    lv_obj_t *joystick_mode_joystick_mode_right_stick_title_label;
    lv_obj_t *joystick_mode_joystick_mode_left_stick_value_label;
    lv_obj_t *joystick_mode_joystick_mode_right_stick_value_label;

    lv_obj_t *pid_tune;
    bool pid_tune_del;
    lv_obj_t *pid_tune_bg_img;
    lv_obj_t *pid_tune_status_bar;
    lv_obj_t *pid_tune_status_value_label;
    lv_obj_t *pid_tune_status_title_label;
    lv_obj_t *pid_tune_battery_label;
    lv_obj_t *pid_tune_title_label;
    lv_obj_t *pid_tune_mode_label;
    lv_obj_t *pid_tune_hint_label;
    lv_obj_t *pid_tune_row[PID_TUNE_ITEM_COUNT];
    lv_obj_t *pid_tune_row_name_label[PID_TUNE_ITEM_COUNT];
    lv_obj_t *pid_tune_row_value_label[PID_TUNE_ITEM_COUNT];
} lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui *ui);

void ui_init_style(lv_style_t *style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t **new_scr, bool new_scr_del, bool *old_scr_del,
                           ui_setup_scr_t setup_scr, lv_scr_load_anim_t anim_type, uint32_t time,
                           uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void *var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value,
                  lv_anim_path_cb_t path_cb, uint16_t repeat_cnt, uint32_t repeat_delay,
                  uint32_t playback_time, uint32_t playback_delay, lv_anim_exec_xcb_t exec_cb,
                  lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb,
                  lv_anim_deleted_cb_t deleted_cb);

void init_scr_del_flag(lv_ui *ui);
void setup_ui(lv_ui *ui);
void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;

void setup_scr_lockscreen(lv_ui *ui);
void setup_scr_menu(lv_ui *ui);
void setup_scr_joystick_mode(lv_ui *ui);
void setup_scr_pid_tune(lv_ui *ui);

LV_IMG_DECLARE(_back_alpha_240x320);

LV_FONT_DECLARE(lv_font_ALMM_48)
LV_FONT_DECLARE(lv_font_ALMM_32)
LV_FONT_DECLARE(lv_font_ALMM_16)
LV_FONT_DECLARE(lv_font_ALMM_23)

#ifdef __cplusplus
}
#endif

#endif /* GUI_GUIDER_H */
