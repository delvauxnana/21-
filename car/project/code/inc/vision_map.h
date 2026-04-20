#ifndef VISION_MAP_H
#define VISION_MAP_H

#include "zf_common_headfile.h"
#include "grid_map.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * 文件作用:
 * 1. 通过 UART4 接收视觉模块发送的 16×12 地图帧。
 * 2. 使用状态机逐字节解析协议帧，写入 grid_map_t。
 * 3. 向上层提供最新解析结果和连接状态查询接口。
 *
 * 协议规范详见: doc/vision_protocol.md
 */

/*===========================================================================
 * 协议常量
 *===========================================================================*/
#define VISION_HEADER_0          (0xA5U)
#define VISION_HEADER_1          (0x5AU)

#define VISION_CMD_MAP           (0x01U)     /* 地图帧（视觉 → 车端）      */
#define VISION_CMD_MAP_REQ       (0x02U)     /* 地图请求（车端 → 视觉）    */
#define VISION_CMD_HEARTBEAT     (0x03U)     /* 心跳/ACK（双向）           */

#define VISION_MAP_PAYLOAD_LEN   (195U)      /* 3B 车信息 + 192B 地图数据  */
#define VISION_PAYLOAD_MAX       (200U)      /* 接收缓冲区上限             */
#define VISION_TIMEOUT_MS        (1000U)     /* 超时判定阈值               */

/*===========================================================================
 * 数据结构
 *===========================================================================*/

/* 视觉地图帧解析结果 */
typedef struct
{
    bool            valid;          /* 至少收到过一帧有效地图数据     */
    bool            connected;      /* 链路是否在线                  */
    uint32_t        update_tick;    /* 最近一次成功解析的系统 tick    */
    uint32_t        rx_good;        /* 校验通过的帧数                */
    uint32_t        rx_bad;         /* 校验失败或格式错误的帧数      */

    /* 最新地图数据 */
    grid_map_t      map;            /* 16×12 栅格地图                */
    uint8_t         car_x;          /* 车模 X 坐标 (0~15)            */
    uint8_t         car_y;          /* 车模 Y 坐标 (0~11)            */
    car_direction_t car_dir;        /* 车模朝向                      */
} vision_map_data_t;

/*===========================================================================
 * 公共接口
 *===========================================================================*/

/* 初始化 UART4 硬件和内部状态 */
void vision_map_init(void);

/* 在主循环中调用，从 UART 接收缓冲区逐字节解析帧 */
void vision_map_poll(void);

/* ISR 中每 5ms 调用一次，驱动超时检测 */
void vision_map_tick(void);

/* UART4 接收中断回调（在 LPUART4_IRQHandler 中调用） */
void vision_map_uart_isr(void);

/* 主动请求视觉模块发送一帧地图 */
void vision_map_request(void);

/* 发送心跳/ACK */
void vision_map_send_heartbeat(uint8_t status);

/* 获取最新解析结果（只读指针） */
const vision_map_data_t *vision_map_get_data(void);

/* 解析结果是否有效且未超时 */
bool vision_map_is_online(void);

#endif /* VISION_MAP_H */
