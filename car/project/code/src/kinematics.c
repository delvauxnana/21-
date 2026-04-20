#include "kinematics.h"
#include "board_config.h"

#define KINEMATICS_PI    (3.1415926f)

/* 返回麦轮底盘旋转项对应的等效半径。 */
static float kinematics_get_rotate_lever_arm(void)
{
    return (BOARD_WHEEL_BASE_MM + BOARD_TRACK_WIDTH_MM) * 0.5f;
}

/* 将车体速度目标转换为四轮线速度目标。 */
void kinematics_inverse(const app_velocity_t *body_ref, app_wheel4_t *wheel_ref)
{
    float rotate_component;

    if ((body_ref == 0) || (wheel_ref == 0))
    {
        return;
    }

    rotate_component = body_ref->wz * KINEMATICS_PI / 180.0f * kinematics_get_rotate_lever_arm();

    wheel_ref->lf = (int32_t)(body_ref->vy + body_ref->vx - rotate_component);
    wheel_ref->rf = (int32_t)(body_ref->vy - body_ref->vx + rotate_component);
    wheel_ref->lb = (int32_t)(body_ref->vy - body_ref->vx - rotate_component);
    wheel_ref->rb = (int32_t)(body_ref->vy + body_ref->vx + rotate_component);
}

/* 将四轮线速度反馈转换为车体速度反馈。 */
void kinematics_forward(const app_wheel4_t *wheel_meas, app_velocity_t *body_meas)
{
    float rotate_component;

    if ((wheel_meas == 0) || (body_meas == 0))
    {
        return;
    }

    body_meas->vx = ((float)wheel_meas->lf - (float)wheel_meas->rf - (float)wheel_meas->lb + (float)wheel_meas->rb) * 0.25f;
    body_meas->vy = ((float)wheel_meas->lf + (float)wheel_meas->rf + (float)wheel_meas->lb + (float)wheel_meas->rb) * 0.25f;

    rotate_component = (-(float)wheel_meas->lf + (float)wheel_meas->rf - (float)wheel_meas->lb + (float)wheel_meas->rb) * 0.25f;
    body_meas->wz = rotate_component / kinematics_get_rotate_lever_arm() * 180.0f / KINEMATICS_PI;
}
