#ifndef KEY_H
#define KEY_H

#include "zf_common_headfile.h"
#include <stdint.h>

/*
 * 文件作用:
 * 1. 统一管理按键扫描和按键事件。
 * 2. 给菜单和任务控制提供输入事件。
 */

typedef enum
{
    KEY_EVENT_NONE = 0,      /* 无按键事件 */
    KEY_EVENT_SHORT_PRESS,   /* 短按事件 */
    KEY_EVENT_LONG_PRESS,    /* 长按首次触发 */
    KEY_EVENT_LONG_REPEAT    /* 长按后的快速连发事件 */
} key_event_t;

typedef enum
{
    APP_KEY_UP = 0,          /* 菜单上移 */
    APP_KEY_DOWN,            /* 菜单下移 */
    APP_KEY_OK,              /* 确认/进入 */
    APP_KEY_BACK,            /* 返回/退出 */
    APP_KEY_COUNT
} app_key_id_t;

void app_key_init(void);
void app_key_update_20ms(void);
key_event_t app_key_get_event(uint8_t key_id);

#endif /* KEY_H */
