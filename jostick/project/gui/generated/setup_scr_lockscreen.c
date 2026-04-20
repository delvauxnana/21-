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

/*===========================================================================
 * 呼吸动画辅助 — 循环改变透明度
 *===========================================================================*/
static void hint_opa_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void setup_scr_lockscreen(lv_ui *ui)
{
    /*========== 屏幕根对象 ==========*/
    ui->lockscreen = lv_obj_create(NULL);
    lv_obj_set_size(ui->lockscreen, 240, 320);
    lv_obj_set_scrollbar_mode(ui->lockscreen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->lockscreen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 背景图 ==========*/
    ui->lockscreen_lockscreen_bg_img = lv_img_create(ui->lockscreen);
    lv_obj_add_flag(ui->lockscreen_lockscreen_bg_img, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->lockscreen_lockscreen_bg_img, &_back_alpha_240x320);
    lv_img_set_pivot(ui->lockscreen_lockscreen_bg_img, 50, 50);
    lv_img_set_angle(ui->lockscreen_lockscreen_bg_img, 0);
    lv_obj_set_pos(ui->lockscreen_lockscreen_bg_img, 0, 0);
    lv_obj_set_size(ui->lockscreen_lockscreen_bg_img, 240, 320);
    lv_obj_set_style_img_recolor_opa(ui->lockscreen_lockscreen_bg_img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->lockscreen_lockscreen_bg_img, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->lockscreen_lockscreen_bg_img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->lockscreen_lockscreen_bg_img, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 主标题 — "RT1064 Controller" ==========*/
    ui->lockscreen_lockscreen_title_label = lv_label_create(ui->lockscreen);
    lv_label_set_text_static(ui->lockscreen_lockscreen_title_label, "RT1064\n控制器");
    lv_label_set_long_mode(ui->lockscreen_lockscreen_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->lockscreen_lockscreen_title_label, 20, 55);
    lv_obj_set_size(ui->lockscreen_lockscreen_title_label, 200, 120);
    /* 样式 */
    lv_obj_set_style_border_width(ui->lockscreen_lockscreen_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->lockscreen_lockscreen_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->lockscreen_lockscreen_title_label, lv_color_hex(0xEAEAEA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->lockscreen_lockscreen_title_label, &lv_font_ALMM_48, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->lockscreen_lockscreen_title_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->lockscreen_lockscreen_title_label, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->lockscreen_lockscreen_title_label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->lockscreen_lockscreen_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->lockscreen_lockscreen_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->lockscreen_lockscreen_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->lockscreen_lockscreen_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 呼吸提示胶囊 — "Press A to start" ==========*/

    /* 毛玻璃胶囊容器 */
    ui->lockscreen_lockscreen_hint_container = lv_obj_create(ui->lockscreen);
    lv_obj_set_pos(ui->lockscreen_lockscreen_hint_container, 40, 220);
    lv_obj_set_size(ui->lockscreen_lockscreen_hint_container, 160, 34);
    lv_obj_set_scrollbar_mode(ui->lockscreen_lockscreen_hint_container, LV_SCROLLBAR_MODE_OFF);
    /* 毛玻璃样式 */
    lv_obj_set_style_radius(ui->lockscreen_lockscreen_hint_container, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->lockscreen_lockscreen_hint_container, 40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->lockscreen_lockscreen_hint_container, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->lockscreen_lockscreen_hint_container, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->lockscreen_lockscreen_hint_container, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->lockscreen_lockscreen_hint_container, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->lockscreen_lockscreen_hint_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->lockscreen_lockscreen_hint_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 提示文字 */
    ui->lockscreen_lockscreen_hint_label = lv_label_create(ui->lockscreen_lockscreen_hint_container);
    lv_label_set_text_static(ui->lockscreen_lockscreen_hint_label, "按A键开始");
    lv_obj_center(ui->lockscreen_lockscreen_hint_label);
    lv_obj_set_style_text_color(ui->lockscreen_lockscreen_hint_label, lv_color_hex(0xEAEAEA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->lockscreen_lockscreen_hint_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->lockscreen_lockscreen_hint_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->lockscreen_lockscreen_hint_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->lockscreen_lockscreen_hint_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 呼吸动画：胶囊容器整体透明度 80↔255, 1500ms 循环, 带回弹 */
    ui_animation(ui->lockscreen_lockscreen_hint_container,
                 1500,          /* duration ms */
                 500,           /* delay ms */
                 80,            /* start opa */
                 255,           /* end opa */
                 lv_anim_path_ease_in_out,
                 LV_ANIM_REPEAT_INFINITE,
                 0,             /* repeat delay */
                 1500,          /* playback time */
                 200,           /* playback delay */
                 hint_opa_anim_cb,
                 NULL, NULL, NULL);

    /*========== 底部作者标签 ==========*/
    ui->lockscreen_lockscreen_author_label = lv_label_create(ui->lockscreen);
    lv_label_set_text_static(ui->lockscreen_lockscreen_author_label, "作者: LJX");
    lv_label_set_long_mode(ui->lockscreen_lockscreen_author_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->lockscreen_lockscreen_author_label, 10, 295);
    lv_obj_set_size(ui->lockscreen_lockscreen_author_label, 100, 17);
    /* 样式 */
    lv_obj_set_style_border_width(ui->lockscreen_lockscreen_author_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->lockscreen_lockscreen_author_label, lv_color_hex(0xA0A0A0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->lockscreen_lockscreen_author_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->lockscreen_lockscreen_author_label, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->lockscreen_lockscreen_author_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->lockscreen_lockscreen_author_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->lockscreen_lockscreen_author_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->lockscreen_lockscreen_author_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 收尾 ==========*/
    lv_obj_update_layout(ui->lockscreen);
    events_init_lockscreen(ui);
}
