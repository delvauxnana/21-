#ifndef APP_TYPES_H
#define APP_TYPES_H

#include "zf_common_headfile.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * 文件作用:
 * 1. 存放全工程共用的数据结构和枚举。
 * 2. 当前速度环相关数据优先改为整形，减少高频控制路径中的浮点运算。
 */

typedef struct
{
    float x_mm;       /* 全局坐标系下的 X 位置，单位 mm */
    float y_mm;       /* 全局坐标系下的 Y 位置，单位 mm */
    float yaw_deg;    /* 车体朝向角，单位 deg */
} app_pose_t;

typedef struct
{
    float vx;         /* 车体坐标系下的 X 向速度 */
    float vy;         /* 车体坐标系下的 Y 向速度 */
    float wz;         /* 车体绕 Z 轴的角速度 */
} app_velocity_t;

typedef struct
{
    int32_t lf;       /* 左前轮数据；速度相关接口中单位为 mm/s，位移相关接口中单位为 0.01mm */
    int32_t rf;       /* 右前轮数据；速度相关接口中单位为 mm/s，位移相关接口中单位为 0.01mm */
    int32_t lb;       /* 左后轮数据；速度相关接口中单位为 mm/s，位移相关接口中单位为 0.01mm */
    int32_t rb;       /* 右后轮数据；速度相关接口中单位为 mm/s，位移相关接口中单位为 0.01mm */
} app_wheel4_t;

typedef enum
{
    APP_MODE_MONITOR = 0,   /* 监视模式，只观察状态不下发控制 */
    APP_MODE_MANUAL,        /* 手动模式，由遥控或调试端直接控制 */
    APP_MODE_AUTO,          /* 自动模式，运行完整比赛流程 */
    APP_MODE_DEBUG_TEST,    /* 调试测试模式，用于单项联调 */
    APP_MODE_PARAM_EDIT     /* 参数编辑模式，用于现场调参 */
} app_run_mode_t;

typedef enum
{
    MISSION_STAGE_IDLE = 0, /* 未开始比赛阶段 */
    MISSION_STAGE_STAGE1,   /* 第一阶段：基础推箱模式 */
    MISSION_STAGE_STAGE2,   /* 第二阶段：分类推箱模式 */
    MISSION_STAGE_STAGE3    /* 第三阶段：炸弹策略模式 */
} mission_stage_t;

typedef enum
{
    MISSION_STATE_IDLE = 0,    /* 状态机空闲 */
    MISSION_STATE_WAIT_START,  /* 等待发车或开始信号 */
    MISSION_STATE_BUILD_MAP,   /* 构建地图与环境信息 */
    MISSION_STATE_SCAN_TARGET, /* 扫描箱子、目标点或识别内容 */
    MISSION_STATE_PLAN_PATH,   /* 进行路径规划或推箱求解 */
    MISSION_STATE_RUN_PATH,    /* 执行已求得的路径 */
    MISSION_STATE_FINISH,      /* 当前阶段或整局任务完成 */
    MISSION_STATE_FAULT        /* 进入故障处理状态 */
} mission_state_t;

typedef struct
{
    bool valid;          /* 当前识别结果是否有效 */
    uint8_t class_id;    /* 识别出的类别编号 */
    uint8_t confidence;  /* 识别置信度或评分 */
} classify_result_t;

#endif /* APP_TYPES_H */
