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
 * 内部常量 — 菜单卡片布局参数
 *===========================================================================*/

/* 颜色体系 */
#define CLR_PRIMARY         0x4A90D9    /* 主色 */
#define CLR_TEXT_MAIN       0xEAEAEA    /* 文字主色 */
#define CLR_TEXT_SUB        0xA0A0A0    /* 文字副色 */
#define CLR_GLASS_BG        0x000000    /* 毛玻璃深色背景 */
#define CLR_GLASS_BORDER    0xFFFFFF    /* 毛玻璃边框 */
#define CLR_STATUS_ON       0x5BE7A9    /* 在线状态 */
#define CLR_STATUS_OFF      0xFF6B6B    /* 离线状态 */

/* 布局参数（240×320 竖屏） */
#define CARD_X              16          /* 卡片 X 位置 */
#define CARD_W              208         /* 卡片宽度 */
#define CARD_H              58          /* 卡片高度 */
#define CARD_GAP            10          /* 卡片间距 */
#define CARD_Y_START        54          /* 第一张卡片 Y 位置 */
#define CARD_RADIUS         14          /* 卡片圆角 */

/* 菜单项文本（中文） */
static const char * const s_card_texts[MENU_CARD_COUNT] = {
    "开始比赛",
    "遥控模式",
    "参数调整",
};

/*===========================================================================
 * 辅助：创建单张菜单卡片
 *===========================================================================*/
static void create_menu_card(lv_ui *ui, int idx, lv_obj_t *parent, lv_coord_t y)
{
    /* ---- 卡片容器 ---- */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, CARD_X, y);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 默认（未选中）样式 — 暗色毛玻璃 */
    lv_obj_set_style_radius(card, CARD_RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(CLR_GLASS_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(card, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, lv_color_hex(CLR_GLASS_BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 选中态（焦点获得时）— Primary 色高亮边框 */
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(card, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(card, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(card, 160, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui->menu_menu_card[idx] = card;

    /* ---- 选中指示箭头 ">" ---- */
    lv_obj_t *arrow = lv_label_create(card);
    lv_label_set_text_static(arrow, ">");
    lv_obj_set_pos(arrow, 14, (CARD_H - 23) / 2);
    lv_obj_set_size(arrow, 20, 23);
    lv_obj_set_style_text_color(arrow, lv_color_hex(CLR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(arrow, &lv_font_ALMM_23, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(arrow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   /* 默认隐藏 */
    lv_obj_set_style_bg_opa(arrow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->menu_menu_card_arrow[idx] = arrow;

    /* ---- 菜单文字 ---- */
    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text_static(label, s_card_texts[idx]);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(label, 38, (CARD_H - 23) / 2);
    lv_obj_set_size(label, CARD_W - 52, 23);
    lv_obj_set_style_text_color(label, lv_color_hex(CLR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_ALMM_23, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->menu_menu_card_label[idx] = label;
}

/*===========================================================================
 * 菜单页面构建
 *===========================================================================*/
void setup_scr_menu(lv_ui *ui)
{
    /*========== 屏幕根对象 ==========*/
    ui->menu = lv_obj_create(NULL);
    lv_obj_set_size(ui->menu, 240, 320);
    lv_obj_set_scrollbar_mode(ui->menu, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->menu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 背景图 ==========*/
    ui->menu_menu_bg_img = lv_img_create(ui->menu);
    lv_obj_add_flag(ui->menu_menu_bg_img, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->menu_menu_bg_img, &_back_alpha_240x320);
    lv_img_set_pivot(ui->menu_menu_bg_img, 50, 50);
    lv_img_set_angle(ui->menu_menu_bg_img, 0);
    lv_obj_set_pos(ui->menu_menu_bg_img, 0, 0);
    lv_obj_set_size(ui->menu_menu_bg_img, 240, 320);
    lv_obj_set_style_img_recolor_opa(ui->menu_menu_bg_img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->menu_menu_bg_img, 181, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->menu_menu_bg_img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->menu_menu_bg_img, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 顶部状态栏 ==========*/
    ui->menu_menu_status_bar = lv_obj_create(ui->menu);
    lv_obj_set_pos(ui->menu_menu_status_bar, 8, 8);
    lv_obj_set_size(ui->menu_menu_status_bar, 224, 32);
    lv_obj_set_scrollbar_mode(ui->menu_menu_status_bar, LV_SCROLLBAR_MODE_OFF);
    /* 毛玻璃样式 */
    lv_obj_set_style_radius(ui->menu_menu_status_bar, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->menu_menu_status_bar, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->menu_menu_status_bar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->menu_menu_status_bar, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->menu_menu_status_bar, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->menu_menu_status_bar, lv_color_hex(CLR_GLASS_BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->menu_menu_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->menu_menu_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 电池电压 */
    ui->menu_menu_battery_label = lv_label_create(ui->menu_menu_status_bar);
    lv_label_set_text(ui->menu_menu_battery_label, "--");
    lv_label_set_long_mode(ui->menu_menu_battery_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(ui->menu_menu_battery_label, 14, 8);
    lv_obj_set_size(ui->menu_menu_battery_label, 60, 16);
    lv_obj_set_style_text_color(ui->menu_menu_battery_label, lv_color_hex(CLR_STATUS_ON), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->menu_menu_battery_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->menu_menu_battery_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->menu_menu_battery_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->menu_menu_battery_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 状态标题 */
    ui->menu_menu_status_title_label = lv_label_create(ui->menu_menu_status_bar);
    lv_label_set_text_static(ui->menu_menu_status_title_label, "连接:");
    lv_label_set_long_mode(ui->menu_menu_status_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->menu_menu_status_title_label, 100, 8);
    lv_obj_set_size(ui->menu_menu_status_title_label, 55, 16);
    lv_obj_set_style_text_color(ui->menu_menu_status_title_label, lv_color_hex(CLR_TEXT_SUB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->menu_menu_status_title_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->menu_menu_status_title_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->menu_menu_status_title_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->menu_menu_status_title_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 状态值 */
    ui->menu_menu_status_value_label = lv_label_create(ui->menu_menu_status_bar);
    lv_label_set_text(ui->menu_menu_status_value_label, "离线");
    lv_label_set_long_mode(ui->menu_menu_status_value_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->menu_menu_status_value_label, 155, 8);
    lv_obj_set_size(ui->menu_menu_status_value_label, 60, 16);
    lv_obj_set_style_text_color(ui->menu_menu_status_value_label, lv_color_hex(CLR_STATUS_OFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->menu_menu_status_value_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->menu_menu_status_value_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->menu_menu_status_value_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->menu_menu_status_value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 3 张菜单卡片 ==========*/
    for (int i = 0; i < MENU_CARD_COUNT; i++) {
        lv_coord_t card_y = CARD_Y_START + i * (CARD_H + CARD_GAP);
        create_menu_card(ui, i, ui->menu, card_y);
    }

    /*========== 底部信息栏 ==========*/
    ui->menu_menu_footer = lv_obj_create(ui->menu);
    lv_obj_set_pos(ui->menu_menu_footer, 8, 282);
    lv_obj_set_size(ui->menu_menu_footer, 224, 30);
    lv_obj_set_scrollbar_mode(ui->menu_menu_footer, LV_SCROLLBAR_MODE_OFF);
    /* 毛玻璃样式 */
    lv_obj_set_style_radius(ui->menu_menu_footer, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->menu_menu_footer, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->menu_menu_footer, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->menu_menu_footer, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->menu_menu_footer, 40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->menu_menu_footer, lv_color_hex(CLR_GLASS_BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->menu_menu_footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->menu_menu_footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 底部文字 */
    ui->menu_menu_footer_label = lv_label_create(ui->menu_menu_footer);
    lv_label_set_text_static(ui->menu_menu_footer_label, "V1.0    作者: LJX");
    lv_obj_center(ui->menu_menu_footer_label);
    lv_obj_set_style_text_color(ui->menu_menu_footer_label, lv_color_hex(CLR_TEXT_SUB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->menu_menu_footer_label, &lv_font_ALMM_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->menu_menu_footer_label, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->menu_menu_footer_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->menu_menu_footer_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*========== 收尾 ==========*/
    lv_obj_update_layout(ui->menu);
    events_init_menu(ui);
    ui_user_hook_menu(ui);
}
