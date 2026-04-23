/*********************************************************************************************************************
* RT1064DVL6A Opensourec Library
* Copyright (c) 2022 SEEKFREE
*********************************************************************************************************************/

#include "zf_common_headfile.h"
#include "board_config.h"
#include "chassis.h"
#include "cmd_dispatcher.h"
#include "control.h"
#include "display.h"
#include "encoder.h"
#include "imu.h"
#include "key.h"
#include "kinematics.h"
#include "menu.h"
#include "motion_manager.h"
#include "motor.h"
#include "odometry.h"
#include "parameter.h"
#include "protocol.h"
#include "scheduler.h"
#include "system_state.h"
#include "vision_map.h"
#include "ws2812.h"


typedef enum
{
    STATUS_LED_BOOT = 0,
    STATUS_LED_REMOTE,
    STATUS_LED_MATCH,
} status_led_state_t;

static void status_led_apply(status_led_state_t state)
{
    uint8_t i;
    uint8_t r = 0U;
    uint8_t g = 0U;
    uint8_t b = 0U;

    switch (state)
    {
        case STATUS_LED_MATCH:
            r = 255U;
            break;

        case STATUS_LED_REMOTE:
            r = 255U;
            g = 255U;
            break;

        case STATUS_LED_BOOT:
        default:
            g = 255U;
            break;
    }

    for (i = 0U; i < BOARD_WS2812_COUNT; ++i)
    {
        ws2812_set_state(i,
                         WS2812_PRIORITY_HIGH,
                         WS2812_MODE_CONSTANT,
                         r, g, b,
                         0U, 0U);
    }
}

static void status_led_update(void)
{
    static status_led_state_t s_last_state = (status_led_state_t)0xFF;
    const proto_velocity_t *velocity = protocol_get_velocity();
    status_led_state_t state = STATUS_LED_BOOT;

    if (motion_manager_get_mode() == MOTION_MODE_AUTO)
    {
        state = STATUS_LED_MATCH;
    }
    else if (velocity->valid && !velocity->failsafe)
    {
        state = STATUS_LED_REMOTE;
    }

    if (state != s_last_state)
    {
        status_led_apply(state);
        s_last_state = state;
    }
}

int main(void)
{
    uint16_t menu_tick_ms      = 0U;
    uint16_t display_tick_ms   = 0U;
    uint16_t heartbeat_tick_ms = 0U;
    uint16_t ws2812_tick_ms    = 0U;

    clock_init(SYSTEM_CLOCK_600M);
    debug_init();

    display_init();

    wireless_uart_init();
    protocol_init();
    cmd_dispatcher_init();
    vision_map_init();

    parameter_load_default();
    system_state_init();
    system_state_set_mode(APP_MODE_DEBUG_TEST);

    motor_init();
    encoder_init();
    imu_init();
    imu_calibrate_zero();
    imu_reset_yaw();

    odometry_init();
    odometry_reset();
    chassis_init();
    control_init();
    motion_manager_init();
    ws2812_init();
    status_led_update();
    ws2812_task();

    app_key_init();
    menu_init();
    display_update_100ms();

    pit_ms_init(BOARD_CONTROL_PIT_CH, BOARD_IMU_PERIOD_MS);
    interrupt_set_priority(BOARD_CONTROL_PIT_IRQn, 0);
    interrupt_global_enable(0);

    scheduler_init();

    while (1)
    {
        protocol_poll();
        vision_map_poll();

        system_delay_ms(1);
        scheduler_tick_1ms();

        menu_tick_ms      += 1U;
        display_tick_ms   += 1U;
        heartbeat_tick_ms += 1U;
        ws2812_tick_ms    += 1U;

        if (menu_tick_ms >= BOARD_MENU_PERIOD_MS)
        {
            menu_tick_ms = 0U;
            app_key_update_20ms();
            (void)menu_update_20ms();
        }

        if (display_tick_ms >= 500U)
        {
            display_tick_ms = 0U;
            display_update_100ms();
        }

        if (heartbeat_tick_ms >= 200U)
        {
            heartbeat_tick_ms = 0U;
            cmd_dispatcher_send_heartbeat();
            cmd_dispatcher_send_telemetry();
        }

        if (ws2812_tick_ms >= BOARD_WS2812_TASK_PERIOD_MS)
        {
            ws2812_tick_ms = 0U;
            status_led_update();
            ws2812_task();
        }
    }
}
