#include "menu.h"
#include "board_config.h"
#include "control.h"
#include "key.h"
#include "motor.h"
#include "motion_manager.h"
#include "parameter.h"
#include "system_state.h"

static menu_snapshot_t g_menu_snapshot;
static int32_t g_motion_target_cache[(uint8_t)MOTION_TEST_AXIS_Z + 1U];

static int32_t menu_limit_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    return func_limit_ab(value, min_value, max_value);
}

static float menu_limit_f(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static bool menu_is_step_event(key_event_t event)
{
    return ((KEY_EVENT_SHORT_PRESS == event)
         || (KEY_EVENT_LONG_PRESS == event)
         || (KEY_EVENT_LONG_REPEAT == event));
}

static bool menu_is_fast_event(key_event_t event)
{
    return ((KEY_EVENT_LONG_PRESS == event) || (KEY_EVENT_LONG_REPEAT == event));
}

static uint8_t menu_get_item_count(menu_page_t page)
{
    switch (page)
    {
        case MENU_PAGE_HOME:       return MENU_HOME_ITEM_COUNT;
        case MENU_PAGE_MOTION:     return MENU_MOTION_ITEM_COUNT;
        case MENU_PAGE_AXIS_TEST:  return MENU_AXIS_TEST_ITEM_COUNT;
        case MENU_PAGE_WHEEL_TEST: return MENU_WHEEL_TEST_ITEM_COUNT;
        case MENU_PAGE_PARAM:      return MENU_PARAM_ITEM_COUNT;
        default:                   return 1U;
    }
}

static void menu_sync_list_offset(void)
{
    uint8_t item_count = menu_get_item_count(g_menu_snapshot.current_page);

    if (0U == item_count)
    {
        g_menu_snapshot.cursor_index = 0U;
        g_menu_snapshot.list_offset = 0U;
        return;
    }

    if (g_menu_snapshot.cursor_index >= item_count)
    {
        g_menu_snapshot.cursor_index = (uint8_t)(item_count - 1U);
    }

    if (item_count <= MENU_LIST_VISIBLE_COUNT)
    {
        g_menu_snapshot.list_offset = 0U;
        return;
    }

    if (g_menu_snapshot.cursor_index < g_menu_snapshot.list_offset)
    {
        g_menu_snapshot.list_offset = g_menu_snapshot.cursor_index;
    }
    else if (g_menu_snapshot.cursor_index >= (g_menu_snapshot.list_offset + MENU_LIST_VISIBLE_COUNT))
    {
        g_menu_snapshot.list_offset = (uint8_t)(g_menu_snapshot.cursor_index - MENU_LIST_VISIBLE_COUNT + 1U);
    }
}

static void menu_enter_home(void)
{
    g_menu_snapshot.current_page = MENU_PAGE_HOME;
    g_menu_snapshot.cursor_index = 0U;
    g_menu_snapshot.list_offset = 0U;
    g_menu_snapshot.edit_mode = false;
    system_state_set_mode(motion_manager_is_move_active() ? APP_MODE_AUTO : APP_MODE_DEBUG_TEST);
}

static void menu_enter_page(menu_page_t page)
{
    g_menu_snapshot.current_page = page;
    g_menu_snapshot.cursor_index = 0U;
    g_menu_snapshot.list_offset = 0U;
    g_menu_snapshot.edit_mode = false;
}

static void menu_move_cursor(int8_t direction)
{
    uint8_t item_count = menu_get_item_count(g_menu_snapshot.current_page);

    if (item_count <= 1U)
    {
        g_menu_snapshot.cursor_index = 0U;
        g_menu_snapshot.list_offset = 0U;
        return;
    }

    if ((direction > 0) && ((g_menu_snapshot.cursor_index + 1U) < item_count))
    {
        g_menu_snapshot.cursor_index ++;
    }
    else if ((direction < 0) && (g_menu_snapshot.cursor_index > 0U))
    {
        g_menu_snapshot.cursor_index --;
    }

    menu_sync_list_offset();
}

static void menu_cycle_axis(int8_t direction)
{
    int8_t axis = (int8_t)g_menu_snapshot.move_axis;

    g_motion_target_cache[g_menu_snapshot.move_axis] = g_menu_snapshot.move_target_value;
    axis += (direction > 0) ? 1 : -1;
    if (axis > (int8_t)MOTION_TEST_AXIS_Z)
    {
        axis = (int8_t)MOTION_TEST_AXIS_X;
    }
    else if (axis < (int8_t)MOTION_TEST_AXIS_X)
    {
        axis = (int8_t)MOTION_TEST_AXIS_Z;
    }

    g_menu_snapshot.move_axis = (uint8_t)axis;
    g_menu_snapshot.move_target_value = g_motion_target_cache[g_menu_snapshot.move_axis];
}

static void menu_cycle_motor(int8_t direction)
{
    int8_t m = (int8_t)g_menu_snapshot.wheel_test_motor;
    m += (direction > 0) ? 1 : -1;
    if (m > (int8_t)MOTOR_RB)  m = (int8_t)MOTOR_LF;
    if (m < (int8_t)MOTOR_LF)  m = (int8_t)MOTOR_RB;
    g_menu_snapshot.wheel_test_motor = (uint8_t)m;
}

static void menu_apply_wheel_test(void)
{
    app_wheel4_t spd = {0, 0, 0, 0};
    int32_t s = g_menu_snapshot.wheel_test_speed;
    switch (g_menu_snapshot.wheel_test_motor)
    {
        case MOTOR_LF: spd.lf = s; break;
        case MOTOR_RF: spd.rf = s; break;
        case MOTOR_LB: spd.lb = s; break;
        case MOTOR_RB: spd.rb = s; break;
        default: break;
    }
    motion_manager_set_test_wheel_speed(&spd);
}

static void menu_stop_wheel_test(void)
{
    app_wheel4_t zero = {0, 0, 0, 0};
    motion_manager_set_test_wheel_speed(&zero);
    motion_manager_set_mode(MOTION_MODE_IDLE);
    g_menu_snapshot.wheel_test_active = false;
}

/* ================================================================
 *  编辑模式调参
 * ================================================================ */

static void menu_adjust_axis_test_item(int8_t direction, bool fast)
{
    parameter_set_t *parameter = parameter_get();
    int32_t linear_step = fast ? 200 : 50;
    int32_t yaw_step = fast ? 15 : 5;
    int32_t speed_step = fast ? 30 : 10;
    int32_t ramp_step = fast ? 8 : 2;
    float yaw_limit_step = fast ? 15.0f : 5.0f;

    switch (g_menu_snapshot.cursor_index)
    {
        case 1U:
            menu_cycle_axis(direction);
            break;

        case 2U:
            if ((uint8_t)MOTION_TEST_AXIS_Z == g_menu_snapshot.move_axis)
            {
                g_menu_snapshot.move_target_value += (direction > 0) ? yaw_step : (-yaw_step);
                g_menu_snapshot.move_target_value = menu_limit_i32(g_menu_snapshot.move_target_value, -180, 180);
            }
            else
            {
                g_menu_snapshot.move_target_value += (direction > 0) ? linear_step : (-linear_step);
                g_menu_snapshot.move_target_value = menu_limit_i32(g_menu_snapshot.move_target_value, -3000, 3000);
            }
            g_motion_target_cache[g_menu_snapshot.move_axis] = g_menu_snapshot.move_target_value;
            break;

        case 3U:
            if ((uint8_t)MOTION_TEST_AXIS_Z == g_menu_snapshot.move_axis)
            {
                parameter->yaw_recover_limit_dps += (direction > 0) ? yaw_limit_step : (-yaw_limit_step);
                parameter->yaw_recover_limit_dps = menu_limit_f(parameter->yaw_recover_limit_dps, 20.0f, 200.0f);
            }
            else
            {
                parameter->position_speed_limit += (direction > 0) ? speed_step : (-speed_step);
                parameter->position_speed_limit = menu_limit_i32(parameter->position_speed_limit, 40, 600);
                parameter->position_pid.output_limit = parameter->position_speed_limit;
            }
            break;

        case 4U:
            parameter->position_ramp_step_mm_s += (direction > 0) ? ramp_step : (-ramp_step);
            parameter->position_ramp_step_mm_s = menu_limit_i32(parameter->position_ramp_step_mm_s, 2, 120);
            break;

        default:
            break;
    }
}

static void menu_adjust_wheel_test_item(int8_t direction, bool fast)
{
    switch (g_menu_snapshot.cursor_index)
    {
        case 1U:
            menu_cycle_motor(direction);
            break;

        case 2U:
        {
            int32_t wheel_step = fast ? 50 : 10;
            g_menu_snapshot.wheel_test_speed += (direction > 0) ? wheel_step : (-wheel_step);
            g_menu_snapshot.wheel_test_speed = menu_limit_i32(g_menu_snapshot.wheel_test_speed, -600, 600);
            if (g_menu_snapshot.wheel_test_active)
            {
                menu_apply_wheel_test();
            }
            break;
        }

        default:
            break;
    }
}

static void menu_adjust_param_item(int8_t direction, bool fast)
{
    parameter_set_t *parameter = parameter_get();
    int32_t pos_p_step = fast ? 150 : 50;
    int32_t pos_i_step = fast ? 30 : 10;
    int32_t pos_d_step = fast ? 30 : 10;
    int32_t speed_p_step = fast ? 300 : 100;
    int32_t speed_i_step = fast ? 30 : 10;
    int32_t speed_d_step = fast ? 30 : 10;
    float yaw_kp_step = fast ? 0.3f : 0.1f;
    float yaw_deadband_step = fast ? 0.3f : 0.1f;
    float yaw_recover_enter_step = fast ? 2.0f : 1.0f;
    float yaw_recover_limit_step = fast ? 15.0f : 5.0f;

    switch (g_menu_snapshot.cursor_index)
    {
        case 0U:
            parameter->position_pid.kp += (direction > 0) ? pos_p_step : (-pos_p_step);
            parameter->position_pid.kp = menu_limit_i32(parameter->position_pid.kp, 0, 4000);
            break;

        case 1U:
            parameter->position_pid.ki += (direction > 0) ? pos_i_step : (-pos_i_step);
            parameter->position_pid.ki = menu_limit_i32(parameter->position_pid.ki, 0, 2000);
            break;

        case 2U:
            parameter->position_pid.kd += (direction > 0) ? pos_d_step : (-pos_d_step);
            parameter->position_pid.kd = menu_limit_i32(parameter->position_pid.kd, 0, 2000);
            break;

        case 3U:
            parameter->wheel_pid.kp += (direction > 0) ? speed_p_step : (-speed_p_step);
            parameter->wheel_pid.kp = menu_limit_i32(parameter->wheel_pid.kp, 0, 6000);
            break;

        case 4U:
            parameter->wheel_pid.ki += (direction > 0) ? speed_i_step : (-speed_i_step);
            parameter->wheel_pid.ki = menu_limit_i32(parameter->wheel_pid.ki, 0, 2000);
            break;

        case 5U:
            parameter->wheel_pid.kd += (direction > 0) ? speed_d_step : (-speed_d_step);
            parameter->wheel_pid.kd = menu_limit_i32(parameter->wheel_pid.kd, 0, 2000);
            break;

        case 6U:
            parameter->yaw_hold_kp += (direction > 0) ? yaw_kp_step : (-yaw_kp_step);
            parameter->yaw_hold_kp = menu_limit_f(parameter->yaw_hold_kp, 0.2f, 12.0f);
            break;

        case 7U:
            parameter->yaw_hold_deadband_deg += (direction > 0) ? yaw_deadband_step : (-yaw_deadband_step);
            parameter->yaw_hold_deadband_deg = menu_limit_f(parameter->yaw_hold_deadband_deg, 0.5f, 12.0f);
            break;

        case 8U:
            parameter->yaw_recover_enter_deg += (direction > 0) ? yaw_recover_enter_step : (-yaw_recover_enter_step);
            parameter->yaw_recover_enter_deg = menu_limit_f(parameter->yaw_recover_enter_deg, 4.0f, 40.0f);
            if (parameter->yaw_recover_exit_deg >= parameter->yaw_recover_enter_deg)
            {
                parameter->yaw_recover_exit_deg = parameter->yaw_recover_enter_deg - 1.0f;
            }
            break;

        case 9U:
            parameter->yaw_recover_limit_dps += (direction > 0) ? yaw_recover_limit_step : (-yaw_recover_limit_step);
            parameter->yaw_recover_limit_dps = menu_limit_f(parameter->yaw_recover_limit_dps, 20.0f, 200.0f);
            break;

        default:
            break;
    }
}

/* ================================================================
 *  编辑模式统一处理
 * ================================================================ */

static bool menu_handle_edit_mode(key_event_t up_event, key_event_t down_event, key_event_t ok_event, key_event_t back_event)
{
    bool changed = false;

    if (menu_is_step_event(up_event))
    {
        if (MENU_PAGE_AXIS_TEST == g_menu_snapshot.current_page)
        {
            menu_adjust_axis_test_item(-1, menu_is_fast_event(up_event));
            changed = true;
        }
        else if (MENU_PAGE_WHEEL_TEST == g_menu_snapshot.current_page)
        {
            menu_adjust_wheel_test_item(-1, menu_is_fast_event(up_event));
            changed = true;
        }
        else if (MENU_PAGE_PARAM == g_menu_snapshot.current_page)
        {
            menu_adjust_param_item(-1, menu_is_fast_event(up_event));
            changed = true;
        }
    }

    if (menu_is_step_event(down_event))
    {
        if (MENU_PAGE_AXIS_TEST == g_menu_snapshot.current_page)
        {
            menu_adjust_axis_test_item(1, menu_is_fast_event(down_event));
            changed = true;
        }
        else if (MENU_PAGE_WHEEL_TEST == g_menu_snapshot.current_page)
        {
            menu_adjust_wheel_test_item(1, menu_is_fast_event(down_event));
            changed = true;
        }
        else if (MENU_PAGE_PARAM == g_menu_snapshot.current_page)
        {
            menu_adjust_param_item(1, menu_is_fast_event(down_event));
            changed = true;
        }
    }

    if ((KEY_EVENT_SHORT_PRESS == ok_event) || (KEY_EVENT_SHORT_PRESS == back_event))
    {
        g_menu_snapshot.edit_mode = false;
        system_state_set_mode(motion_manager_is_move_active() ? APP_MODE_AUTO : APP_MODE_DEBUG_TEST);
        changed = true;
    }

    return changed;
}

/* ================================================================
 *  各页面按键处理
 * ================================================================ */

static bool menu_handle_home_page(key_event_t up_event, key_event_t down_event, key_event_t ok_event)
{
    bool changed = false;

    if (KEY_EVENT_SHORT_PRESS == up_event)
    {
        menu_move_cursor(-1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == down_event)
    {
        menu_move_cursor(1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == ok_event)
    {
        g_menu_snapshot.current_page = (menu_page_t)(g_menu_snapshot.cursor_index + (uint8_t)MENU_PAGE_HOME + 1U);
        g_menu_snapshot.cursor_index = 0U;
        g_menu_snapshot.list_offset = 0U;
        g_menu_snapshot.edit_mode = false;
        changed = true;
    }

    return changed;
}

/* 运动测试子菜单选择页（2 项入口） */
static bool menu_handle_motion_page(key_event_t up_event, key_event_t down_event, key_event_t ok_event, key_event_t back_event)
{
    bool changed = false;

    if (KEY_EVENT_SHORT_PRESS == back_event)
    {
        menu_enter_home();
        return true;
    }

    if (KEY_EVENT_SHORT_PRESS == up_event)
    {
        menu_move_cursor(-1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == down_event)
    {
        menu_move_cursor(1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == ok_event)
    {
        if (0U == g_menu_snapshot.cursor_index)
        {
            menu_enter_page(MENU_PAGE_AXIS_TEST);
        }
        else
        {
            menu_enter_page(MENU_PAGE_WHEEL_TEST);
        }
        changed = true;
    }

    return changed;
}

/* 轴移动测试子页 */
static bool menu_handle_axis_test_page(key_event_t up_event, key_event_t down_event, key_event_t ok_event, key_event_t back_event)
{
    bool changed = false;

    if (KEY_EVENT_SHORT_PRESS == back_event)
    {
        /* 返回运动测试子菜单 */
        menu_enter_page(MENU_PAGE_MOTION);
        return true;
    }

    if (KEY_EVENT_SHORT_PRESS == up_event)
    {
        menu_move_cursor(-1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == down_event)
    {
        menu_move_cursor(1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == ok_event)
    {
        if (0U == g_menu_snapshot.cursor_index)
        {
            /* 启动/停止 距离移动测试 */
            if (g_menu_snapshot.wheel_test_active)
            {
                menu_stop_wheel_test();
            }
            if (motion_manager_is_move_active())
            {
                motion_manager_stop_test_move();
                system_state_set_mode(APP_MODE_DEBUG_TEST);
            }
            else
            {
                motion_manager_start_test_move((motion_test_axis_t)g_menu_snapshot.move_axis,
                                               (float)g_menu_snapshot.move_target_value);
                system_state_set_mode(APP_MODE_AUTO);
            }
        }
        else if (g_menu_snapshot.cursor_index <= 4U)
        {
            /* 可编辑项（1~4）进入编辑模式 */
            g_menu_snapshot.edit_mode = true;
            system_state_set_mode(APP_MODE_PARAM_EDIT);
        }
        /* 5~6 是只读数据行，按 OK 无效 */
        changed = true;
    }

    return changed;
}

/* 单轮测试子页 */
static bool menu_handle_wheel_test_page(key_event_t up_event, key_event_t down_event, key_event_t ok_event, key_event_t back_event)
{
    bool changed = false;

    if (KEY_EVENT_SHORT_PRESS == back_event)
    {
        /* 返回前先停止 */
        if (g_menu_snapshot.wheel_test_active)
        {
            menu_stop_wheel_test();
        }
        menu_enter_page(MENU_PAGE_MOTION);
        return true;
    }

    if (KEY_EVENT_SHORT_PRESS == up_event)
    {
        menu_move_cursor(-1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == down_event)
    {
        menu_move_cursor(1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == ok_event)
    {
        if (0U == g_menu_snapshot.cursor_index)
        {
            /* 启动/停止 单轮测试 */
            if (motion_manager_is_move_active())
            {
                motion_manager_stop_test_move();
            }
            if (g_menu_snapshot.wheel_test_active)
            {
                menu_stop_wheel_test();
            }
            else
            {
                menu_apply_wheel_test();
                g_menu_snapshot.wheel_test_active = true;
                system_state_set_mode(APP_MODE_DEBUG_TEST);
            }
        }
        else if (g_menu_snapshot.cursor_index <= 2U)
        {
            /* 可编辑项（1~2）进入编辑模式 */
            g_menu_snapshot.edit_mode = true;
            system_state_set_mode(APP_MODE_PARAM_EDIT);
        }
        /* 3~4 是只读数据行 */
        changed = true;
    }

    return changed;
}

static bool menu_handle_readonly_page(key_event_t back_event)
{
    if (KEY_EVENT_SHORT_PRESS == back_event)
    {
        menu_enter_home();
        return true;
    }

    return false;
}

static bool menu_handle_param_page(key_event_t up_event, key_event_t down_event, key_event_t ok_event, key_event_t back_event)
{
    bool changed = false;

    if (KEY_EVENT_SHORT_PRESS == back_event)
    {
        menu_enter_home();
        return true;
    }

    if (KEY_EVENT_SHORT_PRESS == up_event)
    {
        menu_move_cursor(-1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == down_event)
    {
        menu_move_cursor(1);
        changed = true;
    }

    if (KEY_EVENT_SHORT_PRESS == ok_event)
    {
        g_menu_snapshot.edit_mode = true;
        system_state_set_mode(APP_MODE_PARAM_EDIT);
        changed = true;
    }

    return changed;
}

/* ================================================================
 *  初始化 & 主更新
 * ================================================================ */

void menu_init(void)
{
    g_motion_target_cache[MOTION_TEST_AXIS_X] = (int32_t)BOARD_MOVE_TEST_DISTANCE_MM;
    g_motion_target_cache[MOTION_TEST_AXIS_Y] = (int32_t)BOARD_MOVE_TEST_DISTANCE_MM;
    g_motion_target_cache[MOTION_TEST_AXIS_Z] = 90;

    g_menu_snapshot.current_page = MENU_PAGE_LOCKSCREEN;
    g_menu_snapshot.cursor_index = 0U;
    g_menu_snapshot.list_offset = 0U;
    g_menu_snapshot.edit_mode = false;
    g_menu_snapshot.move_axis = (uint8_t)MOTION_TEST_AXIS_Y;
    g_menu_snapshot.move_target_value = g_motion_target_cache[MOTION_TEST_AXIS_Y];
    g_menu_snapshot.wheel_test_motor = (uint8_t)MOTOR_LF;
    g_menu_snapshot.wheel_test_speed = 100;
    g_menu_snapshot.wheel_test_active = false;
}

bool menu_update_20ms(void)
{
    key_event_t up_event = app_key_get_event(APP_KEY_UP);
    key_event_t down_event = app_key_get_event(APP_KEY_DOWN);
    key_event_t ok_event = app_key_get_event(APP_KEY_OK);
    key_event_t back_event = app_key_get_event(APP_KEY_BACK);

    if (g_menu_snapshot.edit_mode)
    {
        return menu_handle_edit_mode(up_event, down_event, ok_event, back_event);
    }

    switch (g_menu_snapshot.current_page)
    {
        case MENU_PAGE_LOCKSCREEN:
        {
            /* 锁屏页: 仅响应 OK 短按进入主菜单 */
            if (KEY_EVENT_SHORT_PRESS == ok_event)
            {
                menu_enter_home();
                return true;
            }
            return false;
        }

        case MENU_PAGE_HOME:
            return menu_handle_home_page(up_event, down_event, ok_event);

        case MENU_PAGE_STATUS:
            return menu_handle_readonly_page(back_event);

        case MENU_PAGE_MOTION:
            return menu_handle_motion_page(up_event, down_event, ok_event, back_event);

        case MENU_PAGE_AXIS_TEST:
            return menu_handle_axis_test_page(up_event, down_event, ok_event, back_event);

        case MENU_PAGE_WHEEL_TEST:
            return menu_handle_wheel_test_page(up_event, down_event, ok_event, back_event);

        case MENU_PAGE_POSE:
            return menu_handle_readonly_page(back_event);

        case MENU_PAGE_PARAM:
            return menu_handle_param_page(up_event, down_event, ok_event, back_event);

        case MENU_PAGE_MAP:
            return menu_handle_readonly_page(back_event);

        default:
            menu_enter_home();
            return true;
    }
}

menu_snapshot_t menu_get_snapshot(void)
{
    return g_menu_snapshot;
}
