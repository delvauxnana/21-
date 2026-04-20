#ifndef WS2812_H
#define WS2812_H

#include "zf_common_headfile.h"

typedef enum
{
    WS2812_PRIORITY_LOW = 0,
    WS2812_PRIORITY_HIGH = 1,
} ws2812_priority_e;

typedef enum
{
    WS2812_MODE_CONSTANT = 0,
    WS2812_MODE_BLINK = 1,
} ws2812_mode_e;

typedef struct
{
    uint8 r;
    uint8 g;
    uint8 b;
} ws2812_color_t;

typedef struct
{
    uint8 active;
    ws2812_mode_e mode;
    ws2812_color_t color_on;
    ws2812_color_t color_off;
    uint32 period_ms;
    uint32 duration_ms;
    uint32 start_ms;
} ws2812_state_t;

extern uint8 ws2812_red;
extern uint8 ws2812_green;
extern uint8 ws2812_blue;

void ws2812_init(void);
void ws2812_show_array(uint8 grb_array[][3], uint16 count);
void ws2812_set_state(uint8 led_index,
                      ws2812_priority_e priority,
                      ws2812_mode_e mode,
                      uint8 r, uint8 g, uint8 b,
                      uint32 period_ms,
                      uint32 duration_ms);
void ws2812_clear_state(uint8 led_index, ws2812_priority_e priority);
void ws2812_task(void);

#endif /* WS2812_H */
