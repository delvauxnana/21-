/*
 * Copyright 2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */

#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

#include "ui_user_events.h"
#include "board_input.h"

/*===========================================================================
 * 锁屏页事件
 *===========================================================================*/

static void lockscreen_lockscreen_bg_img_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        lv_group_t *grp = board_input_get_group();
        if (grp != NULL) {
            lv_group_remove_all_objs(grp);
        }
        ui_load_scr_animation(&guider_ui, &guider_ui.menu, guider_ui.menu_del,
                              &guider_ui.lockscreen_del, setup_scr_menu,
                              LV_SCR_LOAD_ANIM_FADE_ON, 200, 200, false, true);
        break;
    }
    default:
        break;
    }
}

void events_init_lockscreen(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->lockscreen_lockscreen_bg_img,
                        lockscreen_lockscreen_bg_img_event_handler,
                        LV_EVENT_ALL, ui);
}

/*===========================================================================
 * 菜单页事件 — 3 张卡片的 CLICKED + FOCUSED/DEFOCUSED
 *===========================================================================*/

/* 卡片获得焦点时：显示箭头 + 高亮（高亮边框由 LV_STATE_FOCUSED 样式驱动） */
static void menu_card_focus_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    lv_obj_t *card = lv_event_get_target(e);
    if (ui == NULL || card == NULL) return;

    /* 查找当前卡片索引 */
    int idx = -1;
    for (int i = 0; i < MENU_CARD_COUNT; i++) {
        if (ui->menu_menu_card[i] == card) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    if (code == LV_EVENT_FOCUSED) {
        /* 显示箭头 */
        if (ui->menu_menu_card_arrow[idx] != NULL) {
            lv_obj_set_style_text_opa(ui->menu_menu_card_arrow[idx], 255,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        /* 隐藏箭头 */
        if (ui->menu_menu_card_arrow[idx] != NULL) {
            lv_obj_set_style_text_opa(ui->menu_menu_card_arrow[idx], 0,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

/* 卡片点击（ENTER键）事件 */
static void menu_card_click_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    lv_obj_t *card = lv_event_get_target(e);
    if (ui == NULL || card == NULL) return;

    /* 查找卡片索引 */
    for (int i = 0; i < MENU_CARD_COUNT; i++) {
        if (ui->menu_menu_card[i] == card) {
            ui_user_menu_card_handle_id(ui, (uint16_t)i);
            break;
        }
    }
}

void events_init_menu(lv_ui *ui)
{
    for (int i = 0; i < MENU_CARD_COUNT; i++) {
        if (ui->menu_menu_card[i] != NULL) {
            lv_obj_add_event_cb(ui->menu_menu_card[i], menu_card_focus_handler,
                                LV_EVENT_FOCUSED, ui);
            lv_obj_add_event_cb(ui->menu_menu_card[i], menu_card_focus_handler,
                                LV_EVENT_DEFOCUSED, ui);
            lv_obj_add_event_cb(ui->menu_menu_card[i], menu_card_click_handler,
                                LV_EVENT_CLICKED, ui);
        }
    }
}

/*===========================================================================
 * 全局初始化（空，各页面独立初始化）
 *===========================================================================*/

void events_init(lv_ui *ui)
{
    LV_UNUSED(ui);
}
