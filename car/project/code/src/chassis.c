#include "chassis.h"
#include "kinematics.h"

static app_velocity_t g_chassis_cmd;
static app_wheel4_t g_wheel_ref;

/* 初始化底盘层状态。 */
void chassis_init(void)
{
    g_chassis_cmd = (app_velocity_t){0.0f, 0.0f, 0.0f};
    g_wheel_ref = (app_wheel4_t){0, 0, 0, 0};
}

/* 更新车体速度命令，并同步计算四轮参考速度。 */
void chassis_set_velocity(const app_velocity_t *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_chassis_cmd = *cmd;
    kinematics_inverse(&g_chassis_cmd, &g_wheel_ref);
}

/* 读取当前车体速度命令。 */
app_velocity_t chassis_get_velocity_command(void)
{
    return g_chassis_cmd;
}

/* 读取当前四轮速度参考值。 */
app_wheel4_t chassis_get_wheel_reference(void)
{
    return g_wheel_ref;
}

/* 停止底盘层输出命令。 */
void chassis_stop(void)
{
    g_chassis_cmd = (app_velocity_t){0.0f, 0.0f, 0.0f}
    g_wheel_ref = (app_wheel4_t){0, 0, 0, 0}
}
