/*
 * Copyright 2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "ui_user_events.h"

/*===========================================================================
 * 色彩常量（与 setup_scr_menu.c 保持一致）
 *===========================================================================*/
#define CLR_PRIMARY         0x4A90D9
#define CLR_ACCENT          0x5BE7A9
#define CLR_STATUS_OFF      0xFF6B6B
#define CLR_TEXT_MAIN       0xEAEAEA
#define CLR_TEXT_SUB        0xA0A0A0
#define CLR_GLASS_BORDER    0xFFFFFF
#define CLR_KNOB_COLOR      0x4A90D9
#define CLR_CROSS_LINE      0x4A90D9

void setup_scr_joystick_mode(lv_ui *ui)
{
    /*========== 屏幕根对象 ==========*/
    ui->joystick_mode = lv_obj_create(NULL);
    lv_obj_set_size(ui->joystick_mode, 240, 320);
    lv_obj_set_scrollbar_mode(ui->joystick_mode, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->joystick_mode, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 背景图 ==========*/
    ui->joystick_mode_joystick_mode_bg_img = lv_img_create(ui->joystick_mode);
    lv_obj_add_flag(ui->joystick_mode_joystick_mode_bg_img, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->joystick_mode_joystick_mode_bg_img, &_back_alpha_240x320);
    lv_img_set_pivot(ui->joystick_mode_joystick_mode_bg_img, 50, 50);
    lv_img_set_angle(ui->joystick_mode_joystick_mode_bg_img, 0);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_bg_img, 0, 0);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_bg_img, 240, 320);
    lv_obj_set_style_img_recolor_opa(ui->joystick_mode_joystick_mode_bg_img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->joystick_mode_joystick_mode_bg_img, 181, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_bg_img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->joystick_mode_joystick_mode_bg_img, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 顶部状态栏（与菜单页一致的暗色毛玻璃） ==========*/
    ui->joystick_mode_joystick_mode_status_bar = lv_obj_create(ui->joystick_mode);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_status_bar, 8, 8);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_status_bar, 224, 32);
    lv_obj_set_scrollbar_mode(ui->joystick_mode_joystick_mode_status_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_status_bar, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_status_bar, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->joystick_mode_joystick_mode_status_bar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->joystick_mode_joystick_mode_status_bar, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->joystick_mode_joystick_mode_status_bar, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->joystick_mode_joystick_mode_status_bar, lv_color_hex(CLR_GLASS_BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->joystick_mode_joystick_mode_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->joystick_mode_joystick_mode_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 电池电压 */
    ui->joystick_mode_joystick_mode_battery_label = lv_label_create(ui->joystick_mode_joystick_mode_status_bar);
    lv_label_set_text(ui->joystick_mode_joystick_mode_battery_label, "--");
    lv_label_set_long_mode(ui->joystick_mode_joystick_mode_battery_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_battery_label, 14, 8);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_battery_label, 60, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_battery_label, lv_color_hex(CLR_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_battery_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_battery_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_battery_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_battery_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 状态标题 */
    ui->joystick_mode_joystick_mode_status_title_label = lv_label_create(ui->joystick_mode_joystick_mode_status_bar);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_status_title_label, "连接:");
    lv_label_set_long_mode(ui->joystick_mode_joystick_mode_status_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_status_title_label, 100, 8);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_status_title_label, 55, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_status_title_label, lv_color_hex(CLR_TEXT_SUB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_status_title_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_status_title_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_status_title_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_status_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 状态值 */
    ui->joystick_mode_joystick_mode_status_value_label = lv_label_create(ui->joystick_mode_joystick_mode_status_bar);
    lv_label_set_text(ui->joystick_mode_joystick_mode_status_value_label, "离线");
    lv_label_set_long_mode(ui->joystick_mode_joystick_mode_status_value_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_status_value_label, 155, 8);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_status_value_label, 60, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_status_value_label, lv_color_hex(CLR_STATUS_OFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_status_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_status_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_status_value_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_status_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 速度面板（毛玻璃暗底 + 1px 边框） ==========*/
    ui->joystick_mode_joystick_mode_speed_panel = lv_obj_create(ui->joystick_mode);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_speed_panel, 15, 50);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_speed_panel, 210, 120);
    lv_obj_set_scrollbar_mode(ui->joystick_mode_joystick_mode_speed_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_speed_panel, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_speed_panel, 140, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->joystick_mode_joystick_mode_speed_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->joystick_mode_joystick_mode_speed_panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->joystick_mode_joystick_mode_speed_panel, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->joystick_mode_joystick_mode_speed_panel, lv_color_hex(CLR_GLASS_BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->joystick_mode_joystick_mode_speed_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->joystick_mode_joystick_mode_speed_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 速度面板标题 */
    ui->joystick_mode_joystick_mode_speed_title_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_speed_title_label, "速度监控");
    lv_label_set_long_mode(ui->joystick_mode_joystick_mode_speed_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_speed_title_label, 45, 8);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_speed_title_label, 120, 18);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_speed_title_label, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_speed_title_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_speed_title_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_speed_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_speed_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*---------- 速度轴标签 + 数值 ----------*/

    /* Vx 名称 */
    ui->joystick_mode_joystick_mode_axis_x_name_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_axis_x_name_label, "前进:");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_axis_x_name_label, 14, 38);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_axis_x_name_label, 55, 17);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_axis_x_name_label, lv_color_hex(CLR_TEXT_SUB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_axis_x_name_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_axis_x_name_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_axis_x_name_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_axis_x_name_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Vx 值 */
    ui->joystick_mode_joystick_mode_axis_x_value_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text(ui->joystick_mode_joystick_mode_axis_x_value_label, "0");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_axis_x_value_label, 75, 38);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_axis_x_value_label, 120, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_axis_x_value_label, lv_color_hex(CLR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_axis_x_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_axis_x_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_axis_x_value_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_axis_x_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Vy 名称 */
    ui->joystick_mode_joystick_mode_axis_y_name_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_axis_y_name_label, "横移:");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_axis_y_name_label, 14, 64);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_axis_y_name_label, 55, 17);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_axis_y_name_label, lv_color_hex(CLR_TEXT_SUB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_axis_y_name_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_axis_y_name_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_axis_y_name_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_axis_y_name_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Vy 值 */
    ui->joystick_mode_joystick_mode_axis_y_value_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text(ui->joystick_mode_joystick_mode_axis_y_value_label, "0");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_axis_y_value_label, 75, 64);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_axis_y_value_label, 120, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_axis_y_value_label, lv_color_hex(CLR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_axis_y_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_axis_y_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_axis_y_value_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_axis_y_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Wz 名称 */
    ui->joystick_mode_joystick_mode_axis_z_name_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_axis_z_name_label, "旋转:");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_axis_z_name_label, 14, 90);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_axis_z_name_label, 55, 17);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_axis_z_name_label, lv_color_hex(CLR_TEXT_SUB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_axis_z_name_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_axis_z_name_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_axis_z_name_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_axis_z_name_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Wz 值 */
    ui->joystick_mode_joystick_mode_axis_z_value_label = lv_label_create(ui->joystick_mode_joystick_mode_speed_panel);
    lv_label_set_text(ui->joystick_mode_joystick_mode_axis_z_value_label, "0");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_axis_z_value_label, 75, 90);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_axis_z_value_label, 120, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_axis_z_value_label, lv_color_hex(CLR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_axis_z_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_axis_z_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_axis_z_value_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_axis_z_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 摇杆标题标签 ==========*/

    /* L-Stick 标题 */
    ui->joystick_mode_joystick_mode_left_stick_title_label = lv_label_create(ui->joystick_mode);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_left_stick_title_label, "左摇杆");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_left_stick_title_label, 5, 177);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_left_stick_title_label, 110, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_left_stick_title_label, lv_color_hex(CLR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_left_stick_title_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_left_stick_title_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_left_stick_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_left_stick_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* R-Stick 标题 */
    ui->joystick_mode_joystick_mode_right_stick_title_label = lv_label_create(ui->joystick_mode);
    lv_label_set_text_static(ui->joystick_mode_joystick_mode_right_stick_title_label, "右摇杆");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_right_stick_title_label, 125, 177);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_right_stick_title_label, 110, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_right_stick_title_label, lv_color_hex(CLR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_right_stick_title_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_right_stick_title_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_right_stick_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_right_stick_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 摇杆数值标签 ==========*/

    /* L-Stick 值 */
    ui->joystick_mode_joystick_mode_left_stick_value_label = lv_label_create(ui->joystick_mode);
    lv_label_set_text(ui->joystick_mode_joystick_mode_left_stick_value_label, "0, 0");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_left_stick_value_label, 5, 193);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_left_stick_value_label, 110, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_left_stick_value_label, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_left_stick_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_left_stick_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_left_stick_value_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_left_stick_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* R-Stick 值 */
    ui->joystick_mode_joystick_mode_right_stick_value_label = lv_label_create(ui->joystick_mode);
    lv_label_set_text(ui->joystick_mode_joystick_mode_right_stick_value_label, "0, 0");
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_right_stick_value_label, 125, 193);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_right_stick_value_label, 110, 16);
    lv_obj_set_style_text_color(ui->joystick_mode_joystick_mode_right_stick_value_label, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->joystick_mode_joystick_mode_right_stick_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->joystick_mode_joystick_mode_right_stick_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->joystick_mode_joystick_mode_right_stick_value_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_right_stick_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 左摇杆十字线 ==========*/
    ui->joystick_mode_joystick_mode_left_stick_cross_v_line = lv_line_create(ui->joystick_mode);
    static lv_point_t left_cross_v[] = {{52, 0}, {52, 105}};
    lv_line_set_points(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, left_cross_v, 2);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, 8, 213);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, 105, 105);
    lv_obj_set_style_line_width(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, lv_color_hex(CLR_CROSS_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->joystick_mode_joystick_mode_left_stick_cross_v_line, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->joystick_mode_joystick_mode_left_stick_cross_h_line = lv_line_create(ui->joystick_mode);
    static lv_point_t left_cross_h[] = {{0, 51}, {105, 52}};
    lv_line_set_points(ui->joystick_mode_joystick_mode_left_stick_cross_h_line, left_cross_h, 2);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_left_stick_cross_h_line, 8, 213);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_left_stick_cross_h_line, 105, 105);
    lv_obj_set_style_line_width(ui->joystick_mode_joystick_mode_left_stick_cross_h_line, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->joystick_mode_joystick_mode_left_stick_cross_h_line, lv_color_hex(CLR_CROSS_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->joystick_mode_joystick_mode_left_stick_cross_h_line, 100, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 右摇杆十字线 ==========*/
    ui->joystick_mode_joystick_mode_right_stick_cross_v_line = lv_line_create(ui->joystick_mode);
    static lv_point_t right_cross_v[] = {{52, 0}, {52, 105}};
    lv_line_set_points(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, right_cross_v, 2);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, 127, 213);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, 105, 105);
    lv_obj_set_style_line_width(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, lv_color_hex(CLR_CROSS_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->joystick_mode_joystick_mode_right_stick_cross_v_line, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->joystick_mode_joystick_mode_right_stick_cross_h_line = lv_line_create(ui->joystick_mode);
    static lv_point_t right_cross_h[] = {{0, 51}, {105, 52}};
    lv_line_set_points(ui->joystick_mode_joystick_mode_right_stick_cross_h_line, right_cross_h, 2);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_right_stick_cross_h_line, 127, 213);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_right_stick_cross_h_line, 105, 105);
    lv_obj_set_style_line_width(ui->joystick_mode_joystick_mode_right_stick_cross_h_line, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->joystick_mode_joystick_mode_right_stick_cross_h_line, lv_color_hex(CLR_CROSS_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->joystick_mode_joystick_mode_right_stick_cross_h_line, 100, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 左摇杆圆盘 + 帽 ==========*/
    ui->joystick_mode_joystick_mode_left_stick_area = lv_obj_create(ui->joystick_mode);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_left_stick_area, 5, 210);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_left_stick_area, 110, 110);
    lv_obj_set_scrollbar_mode(ui->joystick_mode_joystick_mode_left_stick_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_left_stick_area, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_left_stick_area, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->joystick_mode_joystick_mode_left_stick_area, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->joystick_mode_joystick_mode_left_stick_area, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->joystick_mode_joystick_mode_left_stick_area, 180, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->joystick_mode_joystick_mode_left_stick_area, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->joystick_mode_joystick_mode_left_stick_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->joystick_mode_joystick_mode_left_stick_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 左摇杆帽 */
    ui->joystick_mode_joystick_mode_left_stick_knob = lv_obj_create(ui->joystick_mode_joystick_mode_left_stick_area);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_left_stick_knob, 45, 44);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_left_stick_knob, 16, 16);
    lv_obj_set_scrollbar_mode(ui->joystick_mode_joystick_mode_left_stick_knob, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_left_stick_knob, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_left_stick_knob, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->joystick_mode_joystick_mode_left_stick_knob, lv_color_hex(CLR_KNOB_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->joystick_mode_joystick_mode_left_stick_knob, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->joystick_mode_joystick_mode_left_stick_knob, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->joystick_mode_joystick_mode_left_stick_knob, lv_color_hex(CLR_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->joystick_mode_joystick_mode_left_stick_knob, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->joystick_mode_joystick_mode_left_stick_knob, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 右摇杆圆盘 + 帽 ==========*/
    ui->joystick_mode_joystick_mode_right_stick_area = lv_obj_create(ui->joystick_mode);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_right_stick_area, 125, 210);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_right_stick_area, 110, 110);
    lv_obj_set_scrollbar_mode(ui->joystick_mode_joystick_mode_right_stick_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_right_stick_area, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_right_stick_area, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->joystick_mode_joystick_mode_right_stick_area, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->joystick_mode_joystick_mode_right_stick_area, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->joystick_mode_joystick_mode_right_stick_area, 180, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->joystick_mode_joystick_mode_right_stick_area, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->joystick_mode_joystick_mode_right_stick_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->joystick_mode_joystick_mode_right_stick_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 右摇杆帽 */
    ui->joystick_mode_joystick_mode_right_stick_knob = lv_obj_create(ui->joystick_mode_joystick_mode_right_stick_area);
    lv_obj_set_pos(ui->joystick_mode_joystick_mode_right_stick_knob, 44, 44);
    lv_obj_set_size(ui->joystick_mode_joystick_mode_right_stick_knob, 16, 16);
    lv_obj_set_scrollbar_mode(ui->joystick_mode_joystick_mode_right_stick_knob, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(ui->joystick_mode_joystick_mode_right_stick_knob, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->joystick_mode_joystick_mode_right_stick_knob, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->joystick_mode_joystick_mode_right_stick_knob, lv_color_hex(CLR_KNOB_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->joystick_mode_joystick_mode_right_stick_knob, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->joystick_mode_joystick_mode_right_stick_knob, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->joystick_mode_joystick_mode_right_stick_knob, lv_color_hex(CLR_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->joystick_mode_joystick_mode_right_stick_knob, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->joystick_mode_joystick_mode_right_stick_knob, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 收尾 ==========*/
    lv_obj_update_layout(ui->joystick_mode);
    ui_user_hook_joystick_mode(ui);
}
