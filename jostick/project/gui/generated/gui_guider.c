#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"
#include "widgets_init.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "gg_external_data.h"
#endif

void ui_init_style(lv_style_t *style)
{
    if (style->prop_cnt > 1) {
        lv_style_reset(style);
    } else {
        lv_style_init(style);
    }
}

static void ui_nullify_screen_ptrs(lv_ui *ui, lv_obj_t *scr)
{
    int i;

    if ((ui == NULL) || (scr == NULL)) {
        return;
    }

    if (scr == ui->lockscreen) {
        ui->lockscreen = NULL;
        ui->lockscreen_lockscreen_bg_img = NULL;
        ui->lockscreen_lockscreen_title_label = NULL;
        ui->lockscreen_lockscreen_author_label = NULL;
        ui->lockscreen_lockscreen_hint_container = NULL;
        ui->lockscreen_lockscreen_hint_label = NULL;
        return;
    }

    if (scr == ui->menu) {
        ui->menu = NULL;
        ui->menu_menu_bg_img = NULL;
        ui->menu_menu_status_bar = NULL;
        ui->menu_menu_status_value_label = NULL;
        ui->menu_menu_status_title_label = NULL;
        ui->menu_menu_battery_label = NULL;
        for (i = 0; i < MENU_CARD_COUNT; ++i) {
            ui->menu_menu_card[i] = NULL;
            ui->menu_menu_card_label[i] = NULL;
            ui->menu_menu_card_arrow[i] = NULL;
        }
        ui->menu_menu_footer = NULL;
        ui->menu_menu_footer_label = NULL;
        return;
    }

    if (scr == ui->joystick_mode) {
        ui->joystick_mode = NULL;
        ui->joystick_mode_joystick_mode_bg_img = NULL;
        ui->joystick_mode_joystick_mode_right_stick_cross_v_line = NULL;
        ui->joystick_mode_joystick_mode_right_stick_cross_h_line = NULL;
        ui->joystick_mode_joystick_mode_left_stick_cross_v_line = NULL;
        ui->joystick_mode_joystick_mode_left_stick_cross_h_line = NULL;
        ui->joystick_mode_joystick_mode_right_stick_area = NULL;
        ui->joystick_mode_joystick_mode_right_stick_knob = NULL;
        ui->joystick_mode_joystick_mode_left_stick_area = NULL;
        ui->joystick_mode_joystick_mode_left_stick_knob = NULL;
        ui->joystick_mode_joystick_mode_status_bar = NULL;
        ui->joystick_mode_joystick_mode_status_value_label = NULL;
        ui->joystick_mode_joystick_mode_status_title_label = NULL;
        ui->joystick_mode_joystick_mode_battery_label = NULL;
        ui->joystick_mode_joystick_mode_speed_panel = NULL;
        ui->joystick_mode_joystick_mode_speed_title_label = NULL;
        ui->joystick_mode_joystick_mode_axis_x_name_label = NULL;
        ui->joystick_mode_joystick_mode_axis_y_name_label = NULL;
        ui->joystick_mode_joystick_mode_axis_z_name_label = NULL;
        ui->joystick_mode_joystick_mode_axis_z_value_label = NULL;
        ui->joystick_mode_joystick_mode_axis_y_value_label = NULL;
        ui->joystick_mode_joystick_mode_axis_x_value_label = NULL;
        ui->joystick_mode_joystick_mode_left_stick_title_label = NULL;
        ui->joystick_mode_joystick_mode_right_stick_title_label = NULL;
        ui->joystick_mode_joystick_mode_left_stick_value_label = NULL;
        ui->joystick_mode_joystick_mode_right_stick_value_label = NULL;
        return;
    }

    if (scr == ui->pid_tune) {
        ui->pid_tune = NULL;
        ui->pid_tune_bg_img = NULL;
        ui->pid_tune_status_bar = NULL;
        ui->pid_tune_status_value_label = NULL;
        ui->pid_tune_status_title_label = NULL;
        ui->pid_tune_battery_label = NULL;
        ui->pid_tune_title_label = NULL;
        ui->pid_tune_mode_label = NULL;
        ui->pid_tune_hint_label = NULL;
        for (i = 0; i < PID_TUNE_ITEM_COUNT; ++i) {
            ui->pid_tune_row[i] = NULL;
            ui->pid_tune_row_name_label[i] = NULL;
            ui->pid_tune_row_value_label[i] = NULL;
        }
    }
}

void ui_load_scr_animation(lv_ui *ui, lv_obj_t **new_scr, bool new_scr_del, bool *old_scr_del,
                           ui_setup_scr_t setup_scr, lv_scr_load_anim_t anim_type, uint32_t time,
                           uint32_t delay, bool is_clean, bool auto_del)
{
    lv_obj_t *act_scr = lv_scr_act();

    LV_UNUSED(anim_type);
    LV_UNUSED(time);
    LV_UNUSED(delay);

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
    if (auto_del) {
        gg_edata_task_clear(act_scr);
    }
#endif

    if (auto_del && is_clean) {
        lv_obj_clean(act_scr);
    }

    if (auto_del) {
        ui_nullify_screen_ptrs(ui, act_scr);
    }

    if (new_scr_del) {
        setup_scr(ui);
    }

    lv_scr_load_anim(*new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    if (auto_del) {
        lv_obj_del_async(act_scr);
    }

    *old_scr_del = auto_del;
}

void ui_animation(void *var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value,
                  lv_anim_path_cb_t path_cb, uint16_t repeat_cnt, uint32_t repeat_delay,
                  uint32_t playback_time, uint32_t playback_delay, lv_anim_exec_xcb_t exec_cb,
                  lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb,
                  lv_anim_deleted_cb_t deleted_cb)
{
    lv_anim_t anim;

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, var);
    lv_anim_set_exec_cb(&anim, exec_cb);
    lv_anim_set_values(&anim, start_value, end_value);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_delay(&anim, delay);
    lv_anim_set_path_cb(&anim, path_cb);
    lv_anim_set_repeat_count(&anim, repeat_cnt);
    lv_anim_set_repeat_delay(&anim, repeat_delay);
    lv_anim_set_playback_time(&anim, playback_time);
    lv_anim_set_playback_delay(&anim, playback_delay);

    if (start_cb != NULL) {
        lv_anim_set_start_cb(&anim, start_cb);
    }
    if (ready_cb != NULL) {
        lv_anim_set_ready_cb(&anim, ready_cb);
    }
    if (deleted_cb != NULL) {
        lv_anim_set_deleted_cb(&anim, deleted_cb);
    }

    lv_anim_start(&anim);
}

void init_scr_del_flag(lv_ui *ui)
{
    ui->lockscreen_del = true;
    ui->menu_del = true;
    ui->joystick_mode_del = true;
    ui->pid_tune_del = true;
}

void setup_ui(lv_ui *ui)
{
    init_scr_del_flag(ui);
    init_keyboard(ui);
    setup_scr_lockscreen(ui);
    lv_scr_load(ui->lockscreen);
}

void init_keyboard(lv_ui *ui)
{
    LV_UNUSED(ui);
}
