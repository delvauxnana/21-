#include "zf_common_headfile.h"
#include "board_init.h"

/* 局部刷新缓冲行数，平衡刷新性能和 RAM 占用 */
#define UI_LVGL_DRAW_BUFFER_LINES   (10U)

/* UI 默认使用 IPS200 SPI 屏幕，竖屏方向 */
#define UI_IPS200_DISPLAY_TYPE      (IPS200_TYPE_SPI)
#define UI_IPS200_DISPLAY_DIR       (IPS200_PORTAIT)

/* LVGL 局部刷新缓冲区 */
static lv_disp_draw_buf_t s_ui_lvgl_draw_buf;
/* LVGL 显示驱动结构体，使用静态对象避免函数返回后失效 */
static lv_disp_drv_t s_ui_lvgl_disp_drv;
static lv_color_t s_ui_lvgl_draw_buf_1[LV_HOR_RES_MAX * UI_LVGL_DRAW_BUFFER_LINES];

/* 将 LVGL 给出的局部区域刷新到 IPS200 屏幕 */
static void ui_lvgl_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint16 width  = (uint16)(area->x2 - area->x1 + 1);
    uint16 height = (uint16)(area->y2 - area->y1 + 1);

    ips200_show_rgb565_image((uint16)area->x1,
                             (uint16)area->y1,
                             (const uint16 *)color_p,
                             width,
                             height,
                             width,
                             height,
                             1);

    lv_disp_flush_ready(disp_drv);
}

/* 初始化 LVGL 显示移植层，并注册 IPS200 的刷新回调 */
void lv_port_disp_init(void)
{
    /* 先设置屏幕方向，再初始化硬件 */
    ips200_set_dir(UI_IPS200_DISPLAY_DIR);
    ips200_init(UI_IPS200_DISPLAY_TYPE);
    ips200_clear();

    /* GuiGuider 工程按 240x320 竖屏导出，做静态约束检查 */
    zf_assert(240 == LV_HOR_RES_MAX);
    zf_assert(320 == LV_VER_RES_MAX);

    lv_disp_draw_buf_init(&s_ui_lvgl_draw_buf,
                          s_ui_lvgl_draw_buf_1,
                          NULL,
                          LV_HOR_RES_MAX * UI_LVGL_DRAW_BUFFER_LINES);

    /* 注册静态驱动对象，避免函数返回后指针失效导致 HardFault */
    lv_disp_drv_init(&s_ui_lvgl_disp_drv);
    s_ui_lvgl_disp_drv.hor_res   = LV_HOR_RES_MAX;
    s_ui_lvgl_disp_drv.ver_res   = LV_VER_RES_MAX;
    s_ui_lvgl_disp_drv.flush_cb  = ui_lvgl_disp_flush;
    s_ui_lvgl_disp_drv.draw_buf  = &s_ui_lvgl_draw_buf;
    lv_disp_drv_register(&s_ui_lvgl_disp_drv);
}

/* 初始化 LVGL 输入移植层，当前由应用层自行扫描按键摇杆，保留空入口 */
void lv_port_indev_init(void)
{
}

/* 统一初始化 UI 所需的板级端口 */
void board_init(void)
{
    lv_port_disp_init();
    lv_port_indev_init();
}
