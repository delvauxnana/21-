#include "vision_map.h"
#include "board_config.h"
#include <string.h>

/*
 * 文件作用：
 * 1. 初始化 UART4 硬件，用于与视觉模块通信。
 * 2. ISR 中将接收字节写入软件环形缓冲区，避免硬件 FIFO 溢出。
 * 3. 主循环 poll 中从环形缓冲区逐字节解析协议帧。
 * 4. 解析地图帧 (CMD=0x01) 后写入 grid_map_t 和车辆位置/朝向。
 */

/*===========================================================================
 * 环形接收缓冲区（ISR 写，主循环读）
 *===========================================================================*/
#define VISION_RX_BUF_SIZE      (512U)   /* 必须是 2 的幂 */
#define VISION_RX_BUF_MASK      (VISION_RX_BUF_SIZE - 1U)

static volatile uint8_t  s_rx_buf[VISION_RX_BUF_SIZE];
static volatile uint16_t s_rx_wr = 0;   /* ISR 写指针 */
static volatile uint16_t s_rx_rd = 0;   /* 主循环读指针 */

/*===========================================================================
 * 状态机定义
 *===========================================================================*/

typedef enum
{
    PARSE_WAIT_HEADER0 = 0,     /* 等待第一个帧头字节 0xA5 */
    PARSE_WAIT_HEADER1,         /* 等待第二个帧头字节 0x5A */
    PARSE_WAIT_CMD,             /* 等待命令字 */
    PARSE_WAIT_LEN,             /* 等待有效载荷长度 */
    PARSE_WAIT_PAYLOAD,         /* 正在接收有效载荷 */
    PARSE_WAIT_CHECKSUM         /* 等待校验字节 */
} parse_state_t;

/*===========================================================================
 * 内部状态
 *===========================================================================*/

static vision_map_data_t  s_data;                        /* 解析结果（对外只读）*/
static parse_state_t      s_state = PARSE_WAIT_HEADER0;  /* 状态机当前状态 */
static uint8_t            s_cmd   = 0;                   /* 当前帧命令字 */
static uint8_t            s_len   = 0;                   /* 当前帧载荷长度 */
static uint8_t            s_idx   = 0;                   /* 载荷接收计数 */
static uint8_t            s_xor   = 0;                   /* 运行中的异或校验值 */
static uint8_t            s_buf[VISION_PAYLOAD_MAX];     /* 载荷接收缓冲区 */
static volatile uint32_t  s_timeout_cnt = 0;             /* 超时计数器（5ms 递增）*/

/*===========================================================================
 * ISR 回调：将 UART4 接收字节写入软件环形缓冲区
 * 由 LPUART4_IRQHandler 调用
 *===========================================================================*/

void vision_map_uart_isr(void)
{
    uint8_t byte;

    /* 逐字节读取硬件 FIFO，直到读空 */
    while ((LPUART4->WATER & LPUART_WATER_RXCOUNT_MASK) >> LPUART_WATER_RXCOUNT_SHIFT)
    {
        byte = LPUART_ReadByte(LPUART4);

        uint16_t next_wr = (s_rx_wr + 1U) & VISION_RX_BUF_MASK;
        if (next_wr != s_rx_rd)   /* 缓冲区未满 */
        {
            s_rx_buf[s_rx_wr] = byte;
            s_rx_wr = next_wr;
        }
        /* 满了就丢弃，避免阻塞 ISR */
    }
}

/*===========================================================================
 * 内部函数：处理已通过校验的完整帧
 *===========================================================================*/

static void on_frame_received(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    switch (cmd)
    {
    case VISION_CMD_MAP:
    {
        /* 地图帧：payload 至少需要 195 字节 */
        if (len < VISION_MAP_PAYLOAD_LEN)
        {
            s_data.rx_bad++;
            return;
        }

        /* 解析车辆信息 */
        uint8_t car_x   = payload[0];
        uint8_t car_y   = payload[1];
        uint8_t car_dir = payload[2];

        /* 范围校验 */
        if (car_x >= GRID_MAP_W || car_y >= GRID_MAP_H || car_dir > 3U)
        {
            s_data.rx_bad++;
            return;
        }

        s_data.car_x   = car_x;
        s_data.car_y   = car_y;
        s_data.car_dir  = (car_direction_t)car_dir;

        /* 解析 192 字节地图数据（行优先） */
        const uint8_t *map_raw = &payload[3];
        uint8_t y;
        uint8_t x;

        for (y = 0U; y < GRID_MAP_H; y++)
        {
            for (x = 0U; x < GRID_MAP_W; x++)
            {
                uint8_t cell_val = map_raw[y * GRID_MAP_W + x];

                /* 限制到有效枚举范围 */
                if (cell_val > (uint8_t)GRID_CELL_CAR)
                {
                    cell_val = (uint8_t)GRID_CELL_UNKNOWN;
                }

                s_data.map.cells[y][x] = (grid_cell_t)cell_val;
            }
        }

        s_data.valid       = true;
        s_data.connected   = true;
        s_data.update_tick = 0;  /* 时间戳由超时计数器 s_timeout_cnt 管理 */
        s_data.rx_good++;
        s_timeout_cnt = 0;  /* 收到有效帧，清除超时计数 */
        break;
    }

    case VISION_CMD_HEARTBEAT:
    {
        /* 心跳帧：更新连接状态 */
        s_data.connected  = true;
        s_timeout_cnt = 0;
        s_data.rx_good++;
        break;
    }

    default:
        /* 未知命令字，忽略 */
        break;
    }
}

/*===========================================================================
 * 状态机：逐字节解析
 *===========================================================================*/

static void parse_byte(uint8_t byte)
{
    switch (s_state)
    {
    case PARSE_WAIT_HEADER0:
        if (byte == VISION_HEADER_0)
        {
            s_state = PARSE_WAIT_HEADER1;
        }
        break;

    case PARSE_WAIT_HEADER1:
        if (byte == VISION_HEADER_1)
        {
            s_state = PARSE_WAIT_CMD;
        }
        else if (byte == VISION_HEADER_0)
        {
            /* 连续出现 0xA5，继续等待 0x5A */
            s_state = PARSE_WAIT_HEADER1;
        }
        else
        {
            s_state = PARSE_WAIT_HEADER0;
        }
        break;

    case PARSE_WAIT_CMD:
        s_cmd = byte;
        s_xor = byte;       /* 校验从 CMD 开始 */
        s_state = PARSE_WAIT_LEN;
        break;

    case PARSE_WAIT_LEN:
        s_len = byte;
        s_xor ^= byte;
        s_idx = 0;
        if (s_len == 0U)
        {
            /* 无载荷，直接等校验 */
            s_state = PARSE_WAIT_CHECKSUM;
        }
        else if (s_len > VISION_PAYLOAD_MAX)
        {
            /* 长度超限，丢弃该帧 */
            s_data.rx_bad++;
            s_state = PARSE_WAIT_HEADER0;
        }
        else
        {
            s_state = PARSE_WAIT_PAYLOAD;
        }
        break;

    case PARSE_WAIT_PAYLOAD:
        s_buf[s_idx] = byte;
        s_xor ^= byte;
        s_idx++;
        if (s_idx >= s_len)
        {
            s_state = PARSE_WAIT_CHECKSUM;
        }
        break;

    case PARSE_WAIT_CHECKSUM:
        if (byte == s_xor)
        {
            /* 校验通过，处理帧 */
            on_frame_received(s_cmd, s_buf, s_len);
        }
        else
        {
            s_data.rx_bad++;
        }
        s_state = PARSE_WAIT_HEADER0;
        break;

    default:
        s_state = PARSE_WAIT_HEADER0;
        break;
    }
}

/*===========================================================================
 * 内部函数：发送一帧
 *===========================================================================*/

static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t header[4];
    uint8_t checksum;
    uint8_t i;

    header[0] = VISION_HEADER_0;
    header[1] = VISION_HEADER_1;
    header[2] = cmd;
    header[3] = len;

    /* 计算校验 */
    checksum = cmd ^ len;
    for (i = 0U; i < len; i++)
    {
        checksum ^= payload[i];
    }

    /* 发送帧头 */
    uart_write_buffer(BOARD_VISION_UART_INDEX, header, 4);

    /* 发送载荷 */
    if (len > 0U && payload != NULL)
    {
        uart_write_buffer(BOARD_VISION_UART_INDEX, payload, len);
    }

    /* 发送校验字节 */
    uart_write_byte(BOARD_VISION_UART_INDEX, checksum);
}

/*===========================================================================
 * 公共接口
 *===========================================================================*/

void vision_map_init(void)
{
    /* 清空内部状态 */
    memset((void *)&s_data, 0, sizeof(s_data));
    s_state       = PARSE_WAIT_HEADER0;
    s_timeout_cnt = 0;
    s_rx_wr       = 0;
    s_rx_rd       = 0;

    /* 初始化地图为全 UNKNOWN */
    grid_map_clear(&s_data.map);

    /* 初始化 UART4 */
    uart_init(BOARD_VISION_UART_INDEX,
              BOARD_VISION_UART_BAUD,
              BOARD_VISION_UART_TX_PIN,
              BOARD_VISION_UART_RX_PIN);

    /* 开启 UART4 接收中断（ISR 中调用 vision_map_uart_isr 存入环形缓冲区） */
    uart_rx_interrupt(BOARD_VISION_UART_INDEX, 1);
}

void vision_map_poll(void)
{
    uint16_t count = 0;

    /* 从环形缓冲区逐字节取出并解析，每次最多 300 字节避免阻塞 */
    while (count < 300U && s_rx_rd != s_rx_wr)
    {
        uint8_t byte = s_rx_buf[s_rx_rd];
        s_rx_rd = (s_rx_rd + 1U) & VISION_RX_BUF_MASK;
        parse_byte(byte);
        count++;
    }
}

void vision_map_tick(void)
{
    /* 每 5ms 调用一次，驱动超时检测 */
    s_timeout_cnt += BOARD_IMU_PERIOD_MS;

    if (s_timeout_cnt >= VISION_TIMEOUT_MS)
    {
        s_data.connected = false;
        s_timeout_cnt    = VISION_TIMEOUT_MS;  /* 防溢出 */
    }
}

void vision_map_request(void)
{
    uint8_t payload = 0x01U;  /* 请求全量地图 */
    send_frame(VISION_CMD_MAP_REQ, &payload, 1U);
}

void vision_map_send_heartbeat(uint8_t status)
{
    send_frame(VISION_CMD_HEARTBEAT, &status, 1U);
}

const vision_map_data_t *vision_map_get_data(void)
{
    return &s_data;
}

bool vision_map_is_online(void)
{
    return s_data.connected;
}
