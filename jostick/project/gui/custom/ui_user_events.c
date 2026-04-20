/*
 * Copyright 2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include "ui_user_events.h"
#include "board_input.h"
#include "custom.h"

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_coord_t s_left_limit_radius = 0;
static lv_coord_t s_right_limit_radius = 0;

/**********************
 *  STATIC FUNCTIONS
 **********************/

static lv_coord_t ui_user_joystick_get_limit_radius(lv_obj_t *stick_area, lv_obj_t *stick_knob)
{
    lv_coord_t content_w;
    lv_coord_t content_h;
    lv_coord_t knob_w;
    lv_coord_t knob_h;
    lv_coord_t radius_x;
    lv_coord_t radius_y;

    if ((stick_area == NULL) || (stick_knob == NULL)) {
        return 0;
    }

    lv_obj_update_layout(stick_area);
    lv_obj_update_layout(stick_knob);

    content_w = lv_obj_get_content_width(stick_area);
    content_h = lv_obj_get_content_height(stick_area);
    knob_w = lv_obj_get_width(stick_knob);
    knob_h = lv_obj_get_height(stick_knob);

    radius_x = (content_w / 2) - (knob_w / 2);
    radius_y = (content_h / 2) - (knob_h / 2);

    return LV_MAX(0, LV_MIN(radius_x, radius_y));
}

static void ui_user_joystick_limit_circle_offset(int32_t *offset_x, int32_t *offset_y,
                                                 lv_coord_t limit_radius)
{
    int32_t x;
    int32_t y;
    uint32_t limit_sq;
    uint32_t length_sq;
    lv_sqrt_res_t sqrt_res;
    uint32_t length;

    if ((offset_x == NULL) || (offset_y == NULL)) {
        return;
    }

    x = *offset_x;
    y = *offset_y;

    if (limit_radius <= 0) {
        *offset_x = 0;
        *offset_y = 0;
        return;
    }

    length_sq = (uint32_t)((x * x) + (y * y));
    limit_sq = (uint32_t)(limit_radius * limit_radius);
    if ((length_sq == 0U) || (length_sq <= limit_sq)) {
        return;
    }

    lv_sqrt(length_sq, &sqrt_res, 0x800);
    length = sqrt_res.i;
    if (length == 0U) {
        *offset_x = 0;
        *offset_y = 0;
        return;
    }

    *offset_x = (x * limit_radius) / (int32_t)length;
    *offset_y = (y * limit_radius) / (int32_t)length;
}

static bool ui_user_joystick_set_offset_cached(lv_obj_t *stick_area, lv_obj_t *stick_knob,
                                               int16_t offset_x, int16_t offset_y,
                                               lv_coord_t limit_radius)
{
    int32_t limited_x;
    int32_t limited_y;

    if ((stick_area == NULL) || (stick_knob == NULL)) {
        return false;
    }

    limited_x = offset_x;
    limited_y = offset_y;
    ui_user_joystick_limit_circle_offset(&limited_x, &limited_y, limit_radius);

    lv_obj_align(stick_knob, LV_ALIGN_CENTER, (lv_coord_t)limited_x, (lv_coord_t)limited_y);
    return true;
}

static bool ui_user_joystick_set_percent_cached(lv_obj_t *stick_area, lv_obj_t *stick_knob,
                                                int16_t percent_x, int16_t percent_y,
                                                lv_coord_t limit_radius)
{
    int32_t limited_percent_x;
    int32_t limited_percent_y;
    int32_t offset_x;
    int32_t offset_y;

    if ((stick_area == NULL) || (stick_knob == NULL)) {
        return false;
    }

    limited_percent_x = LV_CLAMP(-1000, percent_x, 1000);
    limited_percent_y = LV_CLAMP(-1000, percent_y, 1000);

    offset_x = (limit_radius * limited_percent_x) / 1000;
    offset_y = (limit_radius * limited_percent_y) / 1000;

    return ui_user_joystick_set_offset_cached(stick_area, stick_knob,
                                              (int16_t)offset_x, (int16_t)offset_y,
                                              limit_radius);
}

static void ui_user_clear_group(void)
{
    lv_group_t *grp = board_input_get_group();

    if (grp != NULL) {
        lv_group_remove_all_objs(grp);
    }
}

static void ui_user_switch_back_to_menu(lv_ui *ui, bool *src_del)
{
    if ((ui == NULL) || (src_del == NULL)) {
        return;
    }

    board_input_set_active_page(PAGE_MENU);
    ui_user_clear_group();

    ui_load_scr_animation(ui,
                          &ui->menu,
                          ui->menu_del,
                          src_del,
                          setup_scr_menu,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          200,
                          false,
                          true);
}

static void on_lockscreen_loaded(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    ui_user_set_group_for_lockscreen(ui);
}

static void on_menu_loaded(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    ui_user_set_group_for_menu(ui);
}

static void on_joystick_mode_loaded(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    ui_user_set_group_for_joystick_mode(ui);
}

static void on_pid_tune_loaded(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    ui_user_set_group_for_pid_tune(ui);
    custom_pid_tune_on_show(ui);
}

static void on_joystick_bg_key(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key;
    lv_ui *ui;

    if (code != LV_EVENT_KEY) {
        return;
    }

    key = lv_event_get_key(e);
    if (key != LV_KEY_ESC) {
        return;
    }

    ui = (lv_ui *)lv_event_get_user_data(e);
    ui_user_switch_back_to_menu(ui, &ui->joystick_mode_del);
}

static void on_pid_tune_bg_key(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key;
    lv_ui *ui;
    bool request_back = false;

    if (code != LV_EVENT_KEY) {
        return;
    }

    key = lv_event_get_key(e);
    ui = (lv_ui *)lv_event_get_user_data(e);
    if (ui == NULL) {
        return;
    }

    if (!custom_pid_tune_handle_key(ui, key, &request_back)) {
        return;
    }

    if (request_back) {
        ui_user_switch_back_to_menu(ui, &ui->pid_tune_del);
    }
}

/**********************
 *  PUBLIC FUNCTIONS
 **********************/

void ui_user_menu_switch_to_joystick_mode(lv_ui *ui)
{
    if (ui == NULL) {
        return;
    }

    ui_user_clear_group();
    s_left_limit_radius = 0;
    s_right_limit_radius = 0;

    ui_load_scr_animation(ui,
                          &ui->joystick_mode,
                          ui->joystick_mode_del,
                          &ui->menu_del,
                          setup_scr_joystick_mode,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          200,
                          false,
                          true);
}

void ui_user_menu_switch_to_pid_tune(lv_ui *ui)
{
    if (ui == NULL) {
        return;
    }

    ui_user_clear_group();

    ui_load_scr_animation(ui,
                          &ui->pid_tune,
                          ui->pid_tune_del,
                          &ui->menu_del,
                          setup_scr_pid_tune,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          200,
                          200,
                          false,
                          true);
}

bool ui_user_menu_card_handle_id(lv_ui *ui, uint16_t card_id)
{
    if (ui == NULL) {
        return false;
    }

    switch (card_id) {
    case UI_USER_MENU_CARD_ID_REMOTE_MODE:
        ui_user_menu_switch_to_joystick_mode(ui);
        return true;

    case UI_USER_MENU_CARD_ID_PID_AUTO_TUNE:
        ui_user_menu_switch_to_pid_tune(ui);
        return true;

    case UI_USER_MENU_CARD_ID_START_MATCH:
    default:
        return false;
    }
}

bool ui_user_label_set_text_and_color(lv_obj_t *label, const char *text, lv_color_t color)
{
    if ((label == NULL) || (text == NULL)) {
        return false;
    }

    if (!lv_obj_check_type(label, &lv_label_class)) {
        return false;
    }

    lv_obj_set_style_text_font(label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    return true;
}

bool ui_user_joystick_set_offset(lv_obj_t *stick_area, lv_obj_t *stick_knob,
                                 int16_t offset_x, int16_t offset_y)
{
    lv_coord_t limit_radius;

    if ((stick_area == NULL) || (stick_knob == NULL)) {
        return false;
    }

    if (lv_obj_get_parent(stick_knob) != stick_area) {
        return false;
    }

    limit_radius = ui_user_joystick_get_limit_radius(stick_area, stick_knob);
    return ui_user_joystick_set_offset_cached(stick_area, stick_knob,
                                              offset_x, offset_y, limit_radius);
}

bool ui_user_joystick_set_percent(lv_obj_t *stick_area, lv_obj_t *stick_knob,
                                  int16_t percent_x, int16_t percent_y)
{
    lv_coord_t limit_radius;

    if ((stick_area == NULL) || (stick_knob == NULL)) {
        return false;
    }

    limit_radius = ui_user_joystick_get_limit_radius(stick_area, stick_knob);
    return ui_user_joystick_set_percent_cached(stick_area, stick_knob,
                                               percent_x, percent_y, limit_radius);
}

bool ui_user_joystick_reset(lv_obj_t *stick_area, lv_obj_t *stick_knob)
{
    return ui_user_joystick_set_offset(stick_area, stick_knob, 0, 0);
}

bool ui_user_left_stick_set_offset(lv_ui *ui, int16_t offset_x, int16_t offset_y)
{
    if (ui == NULL) {
        return false;
    }

    return ui_user_joystick_set_offset(ui->joystick_mode_joystick_mode_left_stick_area,
                                       ui->joystick_mode_joystick_mode_left_stick_knob,
                                       offset_x, offset_y);
}

bool ui_user_left_stick_set_percent(lv_ui *ui, int16_t percent_x, int16_t percent_y)
{
    if (ui == NULL) {
        return false;
    }

    if (s_left_limit_radius == 0) {
        s_left_limit_radius = ui_user_joystick_get_limit_radius(
            ui->joystick_mode_joystick_mode_left_stick_area,
            ui->joystick_mode_joystick_mode_left_stick_knob);
    }

    return ui_user_joystick_set_percent_cached(
        ui->joystick_mode_joystick_mode_left_stick_area,
        ui->joystick_mode_joystick_mode_left_stick_knob,
        percent_x, percent_y, s_left_limit_radius);
}

bool ui_user_left_stick_reset(lv_ui *ui)
{
    if (ui == NULL) {
        return false;
    }

    return ui_user_joystick_reset(ui->joystick_mode_joystick_mode_left_stick_area,
                                  ui->joystick_mode_joystick_mode_left_stick_knob);
}

bool ui_user_right_stick_set_offset(lv_ui *ui, int16_t offset_x, int16_t offset_y)
{
    if (ui == NULL) {
        return false;
    }

    return ui_user_joystick_set_offset(ui->joystick_mode_joystick_mode_right_stick_area,
                                       ui->joystick_mode_joystick_mode_right_stick_knob,
                                       offset_x, offset_y);
}

bool ui_user_right_stick_set_percent(lv_ui *ui, int16_t percent_x, int16_t percent_y)
{
    if (ui == NULL) {
        return false;
    }

    if (s_right_limit_radius == 0) {
        s_right_limit_radius = ui_user_joystick_get_limit_radius(
            ui->joystick_mode_joystick_mode_right_stick_area,
            ui->joystick_mode_joystick_mode_right_stick_knob);
    }

    return ui_user_joystick_set_percent_cached(
        ui->joystick_mode_joystick_mode_right_stick_area,
        ui->joystick_mode_joystick_mode_right_stick_knob,
        percent_x, percent_y, s_right_limit_radius);
}

bool ui_user_right_stick_reset(lv_ui *ui)
{
    if (ui == NULL) {
        return false;
    }

    return ui_user_joystick_reset(ui->joystick_mode_joystick_mode_right_stick_area,
                                  ui->joystick_mode_joystick_mode_right_stick_knob);
}

void ui_user_set_group_for_lockscreen(lv_ui *ui)
{
    lv_group_t *grp = board_input_get_group();

    if ((grp == NULL) || (ui == NULL)) {
        return;
    }

    lv_group_remove_all_objs(grp);
    lv_group_set_editing(grp, false);
    board_input_set_active_page(PAGE_LOCKSCREEN);

    if (ui->lockscreen_lockscreen_bg_img != NULL) {
        lv_group_add_obj(grp, ui->lockscreen_lockscreen_bg_img);
        lv_group_focus_obj(ui->lockscreen_lockscreen_bg_img);
    }
}

void ui_user_set_group_for_menu(lv_ui *ui)
{
    lv_group_t *grp = board_input_get_group();
    int i;

    if ((grp == NULL) || (ui == NULL)) {
        return;
    }

    lv_group_remove_all_objs(grp);
    lv_group_set_editing(grp, false);
    board_input_set_active_page(PAGE_MENU);

    for (i = 0; i < MENU_CARD_COUNT; ++i) {
        if (ui->menu_menu_card[i] != NULL) {
            lv_group_add_obj(grp, ui->menu_menu_card[i]);
        }
    }

    if (ui->menu_menu_card[0] != NULL) {
        lv_group_focus_obj(ui->menu_menu_card[0]);
    }
}

void ui_user_set_group_for_joystick_mode(lv_ui *ui)
{
    lv_group_t *grp = board_input_get_group();

    if ((grp == NULL) || (ui == NULL)) {
        return;
    }

    lv_group_remove_all_objs(grp);
    lv_group_set_editing(grp, true);
    board_input_set_active_page(PAGE_JOYSTICK_MODE);

    if (ui->joystick_mode_joystick_mode_bg_img != NULL) {
        lv_obj_add_flag(ui->joystick_mode_joystick_mode_bg_img, LV_OBJ_FLAG_CLICKABLE);
        lv_group_add_obj(grp, ui->joystick_mode_joystick_mode_bg_img);
        lv_group_focus_obj(ui->joystick_mode_joystick_mode_bg_img);
    }
}

void ui_user_set_group_for_pid_tune(lv_ui *ui)
{
    lv_group_t *grp = board_input_get_group();

    if ((grp == NULL) || (ui == NULL)) {
        return;
    }

    lv_group_remove_all_objs(grp);
    lv_group_set_editing(grp, true);
    board_input_set_active_page(PAGE_PID_TUNE);

    if (ui->pid_tune_bg_img != NULL) {
        lv_obj_add_flag(ui->pid_tune_bg_img, LV_OBJ_FLAG_CLICKABLE);
        lv_group_add_obj(grp, ui->pid_tune_bg_img);
        lv_group_focus_obj(ui->pid_tune_bg_img);
    }
}

void ui_user_hook_menu(lv_ui *ui)
{
    if ((ui == NULL) || (ui->menu == NULL)) {
        return;
    }

    lv_obj_add_event_cb(ui->menu, on_menu_loaded, LV_EVENT_SCREEN_LOADED, ui);
}

void ui_user_hook_joystick_mode(lv_ui *ui)
{
    if ((ui == NULL) || (ui->joystick_mode == NULL)) {
        return;
    }

    lv_obj_add_event_cb(ui->joystick_mode, on_joystick_mode_loaded,
                        LV_EVENT_SCREEN_LOADED, ui);

    if (ui->joystick_mode_joystick_mode_bg_img != NULL) {
        lv_obj_add_event_cb(ui->joystick_mode_joystick_mode_bg_img,
                            on_joystick_bg_key, LV_EVENT_KEY, ui);
    }
}

void ui_user_hook_pid_tune(lv_ui *ui)
{
    if ((ui == NULL) || (ui->pid_tune == NULL)) {
        return;
    }

    lv_obj_add_event_cb(ui->pid_tune, on_pid_tune_loaded, LV_EVENT_SCREEN_LOADED, ui);

    if (ui->pid_tune_bg_img != NULL) {
        lv_obj_add_event_cb(ui->pid_tune_bg_img, on_pid_tune_bg_key, LV_EVENT_KEY, ui);
    }
}

void ui_user_input_events_init(lv_ui *ui)
{
    if ((ui != NULL) && (ui->lockscreen != NULL)) {
        lv_obj_add_event_cb(ui->lockscreen, on_lockscreen_loaded,
                            LV_EVENT_SCREEN_LOADED, ui);
    }

    ui_user_set_group_for_lockscreen(ui);
}
