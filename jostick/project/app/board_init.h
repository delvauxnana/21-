#ifndef BOARD_INIT_H
#define BOARD_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zf_common_typedef.h"
#include "zf_common_font.h"
#include "lvgl.h"

/* 初始化板级 UI 端口，包括显示端口和输入端口 */
void board_init(void);

/* 初始化 LVGL 显示移植层，将 LVGL 刷新回调绑定到 IPS200 屏幕 */
void lv_port_disp_init(void);

/* 初始化 LVGL 输入移植层，当前版本先保留为空实现入口 */
void lv_port_indev_init(void);

#ifdef __cplusplus
}
#endif

#endif
