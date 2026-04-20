#ifndef BOARD_INPUT_H
#define BOARD_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zf_common_typedef.h"
#include "lvgl.h"
#include "gui_guider.h"

/*===========================================================================
 * 按键引脚分配 — 手柄布局（以 PCB 实物为准）
 *
 *        [C14]              [D1]  [D0]
 *          ↑                 Y     X
 *    [C15]← →[C13]
 *          ↓               [D3]  [D2]
 *        [C12]               B     A
 *===========================================================================*/

/* 左侧 D-Pad（方向键）
 * 注意：以实际测试结果为准 — C15 按下触发"上"，C14 按下触发"下"。 */
#define KEY_PIN_UP          (C15)    /* ↑ 向上（实测 C15 为上键） */
#define KEY_PIN_DOWN        (C14)    /* ↓ 向下（实测 C14 为下键） */
#define KEY_PIN_LEFT        (C12)    /* ← 向左 */
#define KEY_PIN_RIGHT       (C13)    /* → 向右 */

/* 右侧 ABXY（功能键） */
#define KEY_PIN_A           (D3)     /* A = 确认/进入 */
#define KEY_PIN_B           (D1)     /* B = 返回/取消 */
#define KEY_PIN_X           (D2)     /* X = 功能1（预留） */
#define KEY_PIN_Y           (D0)     /* Y = 功能2（预留） */

/* 按键总数 */
#define KEY_COUNT           (8)

/*===========================================================================
 * 摇杆 ADC 通道分配
 *===========================================================================*/
#define STICK_L_X_CH        (ADC1_CH3_B14)  /* 左摇杆 X 轴 */
#define STICK_L_Y_CH        (ADC1_CH4_B15)  /* 左摇杆 Y 轴 */
#define STICK_R_X_CH        (ADC1_CH1_B12)  /* 右摇杆 X 轴 */
#define STICK_R_Y_CH        (ADC1_CH2_B13)  /* 右摇杆 Y 轴 */

/* ADC 12-bit 中位值 */
#define STICK_ADC_CENTER    (2048)
/* 死区阈值（ADC 原始值），消除摇杆中位抖动 */
#define STICK_ADC_DEADZONE  (120)

/*===========================================================================
 * 电池电压 ADC
 *===========================================================================*/
#define BATTERY_ADC_CH      (ADC1_CH9_B20)  /* 电池电压检测引脚 */
#define BATTERY_VREF        (3.3f)           /* ADC 参考电压 */
#define BATTERY_DIVIDER     (2.0f)           /* 分压比（实际电压 = ADC电压 × 分压比）*/
#define BATTERY_ADC_MAX     (4096.0f)        /* 12-bit ADC 满量程 */

/*===========================================================================
 * 按键 → LVGL 键值映射（修改此处即可重映射菜单控制）
 *
 * 注意：LVGL 8 的焦点组导航使用 LV_KEY_NEXT / LV_KEY_PREV，
 *       LV_KEY_UP / LV_KEY_DOWN 仅发送给当前聚焦控件，不会切换焦点。
 *===========================================================================*/
#define KEY_MAP_UP          LV_KEY_PREV     /* ↑ 向上 → 上一项 */
#define KEY_MAP_DOWN        LV_KEY_NEXT     /* ↓ 向下 → 下一项 */
#define KEY_MAP_LEFT        LV_KEY_PREV     /* ← 向左 → 上一项 */
#define KEY_MAP_RIGHT       LV_KEY_NEXT     /* → 向右 → 下一项 */
#define KEY_MAP_A           LV_KEY_ENTER    /* A 确认 → LVGL ENTER*/
#define KEY_MAP_B           LV_KEY_ESC      /* B 返回 → LVGL ESC  */
#define KEY_MAP_X           LV_KEY_NEXT     /* X 预留 → LVGL NEXT */
#define KEY_MAP_Y           LV_KEY_PREV     /* Y 预留 → LVGL PREV */

/*===========================================================================
 * 左摇杆 → 菜单导航键映射（修改此处可改变摇杆操控方向）
 *===========================================================================*/
#define STICK_NAV_NEG_X_KEY LV_KEY_PREV     /* 左摇杆 X-（左推）→ 上一项 */
#define STICK_NAV_POS_X_KEY LV_KEY_NEXT     /* 左摇杆 X+（右推）→ 下一项 */
#define STICK_NAV_NEG_Y_KEY LV_KEY_PREV     /* 左摇杆 Y-（上推）→ 上一项 */
#define STICK_NAV_POS_Y_KEY LV_KEY_NEXT     /* 左摇杆 Y+（下推）→ 下一项 */

/* 摇杆导航阈值 */
#define STICK_NAV_THRESHOLD     (400)   /* 归一化值超过此阈值视为"按下" */
#define STICK_NAV_RELEASE       (250)   /* 回到此阈值以内视为"释放"（滞回）*/

/*===========================================================================
 * 当前活跃页面标识
 *===========================================================================*/
typedef enum {
    PAGE_LOCKSCREEN = 0,
    PAGE_MENU,
    PAGE_JOYSTICK_MODE,
    PAGE_PID_TUNE,
} active_page_t;

/*===========================================================================
 * 摇杆数据结构（ISR 写，主循环读）
 *===========================================================================*/
typedef struct {
    volatile int16_t lx;    /* 左摇杆 X：-1000 ~ +1000 */
    volatile int16_t ly;    /* 左摇杆 Y：-1000 ~ +1000 */
    volatile int16_t rx;    /* 右摇杆 X：-1000 ~ +1000 */
    volatile int16_t ry;    /* 右摇杆 Y：-1000 ~ +1000 */
} stick_data_t;

/*===========================================================================
 * 公共接口
 *===========================================================================*/

/* 初始化所有输入硬件（GPIO 按键 + ADC 摇杆）并注册 LVGL indev */
void board_input_init(lv_ui *ui);

/* 10ms 定时中断服务函数（在 isr.c 的 PIT_CH1 中调用） */
void board_input_isr_10ms(void);

/* 在主循环中调用，读取 ISR 采集的摇杆数据并更新 joystick_mode 页面 */
void board_input_joystick_update(lv_ui *ui);

/* 获取当前焦点组指针（供 ui_user_events 使用） */
lv_group_t *board_input_get_group(void);

/* 设置/获取当前活跃页面（供 ui_user_events 使用） */
void board_input_set_active_page(active_page_t page);
active_page_t board_input_get_active_page(void);

/* 获取当前摇杆数据快照（供 protocol 发送使用） */
void board_input_get_stick_data(int16_t *lx, int16_t *ly, int16_t *rx, int16_t *ry);

/* 在主循环中调用，更新所有非锁屏页面的连接状态栏 */
void board_input_status_update(lv_ui *ui);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_INPUT_H */
