/*
 * Copyright 2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */

#ifndef UI_USER_EVENTS_H_
#define UI_USER_EVENTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

typedef enum
{
    UI_USER_MENU_CARD_ID_START_MATCH = 0,
    UI_USER_MENU_CARD_ID_REMOTE_MODE = 1,
    UI_USER_MENU_CARD_ID_PID_AUTO_TUNE = 2,
} ui_user_menu_card_id_t;

bool ui_user_menu_card_handle_id(lv_ui *ui, uint16_t card_id);

void ui_user_menu_switch_to_joystick_mode(lv_ui *ui);
void ui_user_menu_switch_to_pid_tune(lv_ui *ui);

bool ui_user_label_set_text_and_color(lv_obj_t *label, const char *text, lv_color_t color);
bool ui_user_joystick_set_offset(lv_obj_t *stick_area, lv_obj_t *stick_knob,
                                 int16_t offset_x, int16_t offset_y);
bool ui_user_joystick_set_percent(lv_obj_t *stick_area, lv_obj_t *stick_knob,
                                  int16_t percent_x, int16_t percent_y);
bool ui_user_joystick_reset(lv_obj_t *stick_area, lv_obj_t *stick_knob);

bool ui_user_left_stick_set_offset(lv_ui *ui, int16_t offset_x, int16_t offset_y);
bool ui_user_left_stick_set_percent(lv_ui *ui, int16_t percent_x, int16_t percent_y);
bool ui_user_left_stick_reset(lv_ui *ui);

bool ui_user_right_stick_set_offset(lv_ui *ui, int16_t offset_x, int16_t offset_y);
bool ui_user_right_stick_set_percent(lv_ui *ui, int16_t percent_x, int16_t percent_y);
bool ui_user_right_stick_reset(lv_ui *ui);

void ui_user_set_group_for_lockscreen(lv_ui *ui);
void ui_user_set_group_for_menu(lv_ui *ui);
void ui_user_set_group_for_joystick_mode(lv_ui *ui);
void ui_user_set_group_for_pid_tune(lv_ui *ui);

void ui_user_hook_menu(lv_ui *ui);
void ui_user_hook_joystick_mode(lv_ui *ui);
void ui_user_hook_pid_tune(lv_ui *ui);

void ui_user_input_events_init(lv_ui *ui);

#ifdef __cplusplus
}
#endif

#endif /* UI_USER_EVENTS_H_ */
