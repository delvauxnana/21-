#ifndef MENU_H
#define MENU_H

#include "zf_common_headfile.h"

#define MENU_HOME_ITEM_COUNT        (4U)
#define MENU_MOTION_ITEM_COUNT      (2U)   /* 轴测试 / 单轮测试 两个子入口 */
#define MENU_AXIS_TEST_ITEM_COUNT   (7U)   /* 启停+轴向+目标+限速+斜率+电机输出+编码器 */
#define MENU_WHEEL_TEST_ITEM_COUNT  (5U)   /* 启停+电机选择+速度+电机输出+编码器 */
#define MENU_PARAM_ITEM_COUNT       (10U)
#define MENU_LIST_VISIBLE_COUNT     (5U)

typedef enum
{
    MENU_PAGE_LOCKSCREEN = 0,
    MENU_PAGE_HOME,
    MENU_PAGE_STATUS,
    MENU_PAGE_MOTION,      /* 运动测试子菜单选择页 */
    MENU_PAGE_POSE,
    MENU_PAGE_PARAM,
    MENU_PAGE_AXIS_TEST,   /* 轴移动测试子页 */
    MENU_PAGE_WHEEL_TEST   /* 单轮测试子页 */
} menu_page_t;

typedef struct
{
    menu_page_t current_page;    /* 当前显示页 */
    uint8_t cursor_index;        /* 当前页中的全局光标索引 */
    uint8_t list_offset;         /* 列表页顶部可见项偏移 */
    bool edit_mode;              /* 是否处于参数编辑模式 */
    uint8_t move_axis;           /* 当前运动测试轴向 */
    int32_t move_target_value;   /* 运动测试目标值，X/Y 为 mm，Z 为 deg */
    uint8_t wheel_test_motor;    /* 单轮测试选中电机 0=LF 1=RF 2=LB 3=RB */
    int32_t wheel_test_speed;    /* 单轮测试速度目标 mm/s */
    bool    wheel_test_active;   /* 单轮测试是否正在运行 */
} menu_snapshot_t;

void menu_init(void);
bool menu_update_20ms(void);
menu_snapshot_t menu_get_snapshot(void);

#endif /* MENU_H */
