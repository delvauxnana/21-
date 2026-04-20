#include "board_init.h"
#include "motor.h"
#include "encoder.h"
#include "imu.h"
#include "display.h"
#include "vision_map.h"
#include "vision_classify.h"
#include "key.h"

/*
 * 文件作用：
 * 1. 统一组织各模块初始化顺序。
 * 2. 后续在这里补充 GPIO、SPI、UART、PIT、屏幕和外设初始化。
 */

void board_init_clock(void)
{
    /* TODO: 在此补充 RT1064 系统时钟初始化。 */
}

void board_init_peripherals(void)
{
    motor_init();
    encoder_init();
    imu_init();
    display_init();
    vision_map_init();
    vision_classify_init();
    app_key_init();
}

void board_init_all(void)
{
    board_init_clock();
    board_init_peripherals();
}
