#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "zf_common_headfile.h"

/*
 * 文件作用:
 * 1. 存放整车工程的编译期开关。
 * 2. 存放各周期任务的推荐执行周期。
 * 3. 当前阶段仅提供框架骨架，后续按实际硬件继续细化。
 */

#define APP_USE_IMU                 (1U)    /* 是否启用 IMU 模块 */
#define APP_USE_WIRELESS_DEBUG      (1U)    /* 是否启用无线调试模块 */
#define APP_USE_VISION_MAP          (1U)    /* 是否启用地图视觉链路 */
#define APP_USE_VISION_CLASSIFY     (1U)    /* 是否启用分类视觉链路 */

#define TASK_PERIOD_1MS             (1U)    /* 1ms 基准节拍，用于软件定时 */
#define TASK_PERIOD_5MS             (5U)    /* 5ms 快速任务周期 */
#define TASK_PERIOD_20MS            (20U)   /* 20ms 控制与状态机任务周期 */
#define TASK_PERIOD_50MS            (50U)   /* 50ms 遥测上传任务周期 */
#define TASK_PERIOD_100MS           (100U)  /* 100ms 显示与低速调试任务周期 */

#endif /* APP_CONFIG_H */
