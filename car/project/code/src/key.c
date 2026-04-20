#include "key.h"
#include "board_config.h"

typedef struct
{
    gpio_pin_enum pin;            /* 该按键对应的 GPIO 引脚 */
    uint8_t stable_level;         /* 消抖后的稳定电平 */
    uint8_t debounce_count;       /* 消抖计数 */
    uint16_t press_ticks;         /* 按下持续拍数 */
    bool long_reported;           /* 本次按下是否已经上报长按 */
    key_event_t pending_event;    /* 等待上层读取的事件 */
} app_key_state_t;

static app_key_state_t g_key_state[APP_KEY_COUNT];

/* 初始化单个按键的运行状态。 */
static void app_key_init_single(app_key_id_t key_id, gpio_pin_enum pin)
{
    g_key_state[key_id].pin = pin;
    g_key_state[key_id].stable_level = BOARD_KEY_RELEASE_LEVEL;
    g_key_state[key_id].debounce_count = 0U;
    g_key_state[key_id].press_ticks = 0U;
    g_key_state[key_id].long_reported = false;
    g_key_state[key_id].pending_event = KEY_EVENT_NONE;
}

/* 对单个按键做消抖和事件更新。 */
static void app_key_scan_single(app_key_state_t *key_state)
{
    uint8_t raw_level;

    if (key_state == 0)
    {
        return;
    }

    raw_level = gpio_get_level(key_state->pin);
    if (raw_level != key_state->stable_level)
    {
        key_state->debounce_count ++;
        if (key_state->debounce_count >= BOARD_KEY_DEBOUNCE_TICKS)
        {
            key_state->stable_level = raw_level;
            key_state->debounce_count = 0U;

            if (raw_level != BOARD_KEY_RELEASE_LEVEL)
            {
                key_state->press_ticks = 0U;
                key_state->long_reported = false;
                key_state->pending_event = KEY_EVENT_SHORT_PRESS;
            }
            else
            {
                key_state->press_ticks = 0U;
                key_state->long_reported = false;
            }
        }
    }
    else
    {
        key_state->debounce_count = 0U;
        if (raw_level != BOARD_KEY_RELEASE_LEVEL)
        {
            if (key_state->press_ticks < 0xFFFFU)
            {
                key_state->press_ticks ++;
            }

            if ((!key_state->long_reported) && (key_state->press_ticks >= BOARD_KEY_LONG_PRESS_TICKS))
            {
                key_state->pending_event = KEY_EVENT_LONG_PRESS;
                key_state->long_reported = true;
            }
            else if (key_state->long_reported
                  && (BOARD_KEY_REPEAT_INTERVAL_TICKS > 0U)
                  && (((uint16_t)(key_state->press_ticks - BOARD_KEY_LONG_PRESS_TICKS)) % BOARD_KEY_REPEAT_INTERVAL_TICKS == 0U))
            {
                key_state->pending_event = KEY_EVENT_LONG_REPEAT;
            }
        }
    }
}

/* 初始化四个本地菜单按键。 */
void app_key_init(void)
{
    gpio_init(BOARD_KEY_UP_PIN, GPI, BOARD_KEY_RELEASE_LEVEL, GPI_PULL_UP);
    gpio_init(BOARD_KEY_DOWN_PIN, GPI, BOARD_KEY_RELEASE_LEVEL, GPI_PULL_UP);
    gpio_init(BOARD_KEY_OK_PIN, GPI, BOARD_KEY_RELEASE_LEVEL, GPI_PULL_UP);
    gpio_init(BOARD_KEY_BACK_PIN, GPI, BOARD_KEY_RELEASE_LEVEL, GPI_PULL_UP);

    app_key_init_single(APP_KEY_UP, BOARD_KEY_UP_PIN);
    app_key_init_single(APP_KEY_DOWN, BOARD_KEY_DOWN_PIN);
    app_key_init_single(APP_KEY_OK, BOARD_KEY_OK_PIN);
    app_key_init_single(APP_KEY_BACK, BOARD_KEY_BACK_PIN);
}

/* 每 20ms 扫描一次全部按键。 */
void app_key_update_20ms(void)
{
    uint8_t key_id;

    for (key_id = 0U; key_id < (uint8_t)APP_KEY_COUNT; ++key_id)
    {
        app_key_scan_single(&g_key_state[key_id]);
    }
}

/* 读取指定按键事件，并在读取后自动清空。 */
key_event_t app_key_get_event(uint8_t key_id)
{
    key_event_t event = KEY_EVENT_NONE;

    if (key_id >= (uint8_t)APP_KEY_COUNT)
    {
        return KEY_EVENT_NONE;
    }

    event = g_key_state[key_id].pending_event;
    g_key_state[key_id].pending_event = KEY_EVENT_NONE;
    return event;
}
