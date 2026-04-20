#ifndef CMD_DISPATCHER_H
#define CMD_DISPATCHER_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * 文件作用:
 * 1. 将协议层解析到的帧数据分发到业务逻辑（运动管理器、参数表等）。
 * 2. 包含遥控摇杆 ±1000 → 物理量 (mm/s, deg/s) 的可配置映射。
 * 3. 负责构建遥测帧并定期回传。
 */

/* ---- 摇杆映射增益 ---- */
#define CMD_VX_GAIN      (3.00f)   /* 摇杆 ±1000 → ±500 mm/s          */
#define CMD_VY_GAIN      (3.00f)   /* 摇杆 ±1000 → ±500 mm/s          */
#define CMD_WZ_GAIN      (1.20f)   /* 摇杆 ±1000 → ±120 deg/s         */

/* ---- 极性修正 (正数表示不取反) ---- */
#define CMD_VX_SIGN      ( 1.0f)   /* vx 不取反                       */
#define CMD_VY_SIGN      ( 1.0f)   /* vy 取反: 车体前方 = 运动学 -Y   */
#define CMD_WZ_SIGN      (-1.0f)   /* wz 取反: 用户推右→顺时针       */

/* 初始化命令分发器，向 protocol 注册所有命令回调。 */
void cmd_dispatcher_init(void);

/* 构建并发送遥测帧，由主循环按 200ms 周期调用。 */
void cmd_dispatcher_send_telemetry(void);

/* 构建并发送心跳帧，由主循环按 200ms 周期调用。 */
void cmd_dispatcher_send_heartbeat(void);

#endif /* CMD_DISPATCHER_H */
