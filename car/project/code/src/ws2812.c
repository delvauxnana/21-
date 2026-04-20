/*===========================================================================
 * ws2812.c
 * WS2812B-V5/W LED 驱动（GPIO bit-bang + DWT 精确定时）
 *
 * 参考文档: WS2812B-V5-W.PDF
 *
 * 时序规格 (WS2812B-V5):
 *   T0H  220~380ns    逻辑0高电平
 *   T1H  580~1µs      逻辑1高电平
 *   T0L  580~1µs      逻辑0低电平
 *   T1L  580~1µs      逻辑1低电平
 *   RES  >280µs       复位信号（注意: V5 比旧版 50µs 大很多）
 *
 * 数据格式: GRB，高位在前
 * 数据速率: 800Kbps (1.25µs/bit)
 *
 * 硬件连接: D4 → DIN(LED5) → DOUT→DIN(LED6) → ... → LED8
 * 供电: VDD = 3.3V (注意: 原理图 LED8 的 VSS 标记有 ×)
 *
 * 原理图分析:
 *   - 4颗 WS2812B-V5/W 串联，MCU D4 引脚接第一颗 DIN
 *   - VDD = 3.3V (非 5V，信号幅度偏低但 V5 版本支持 3.3V)
 *   - LED8 的 DOUT 悬空（正常，链末尾不需要 DOUT）
 *   - LED8 的 VSS 原理图标记 × 需确认实际焊接是否连通
 *===========================================================================*/

#include "ws2812.h"
#include "board_config.h"
#include "scheduler.h"

/* ---- 全局颜色变量（兼容旧接口）---- */
uint8 ws2812_red   = 0;
uint8 ws2812_green = 0;
uint8 ws2812_blue  = 0;

/* ---- 内部状态 ---- */
static ws2812_state_t s_ws2812_state[BOARD_WS2812_COUNT][2];
static uint8 s_ws2812_grb[BOARD_WS2812_COUNT][3];

/*===========================================================================
 * DWT 精确延时（基于 ARM Cortex-M7 DWT 周期计数器）
 *
 * RT1064 @ 600MHz: 1 cycle = 1.667ns
 *   300ns = 180 cycles
 *   700ns = 420 cycles
 *   600ns = 360 cycles
 *   280µs = 168000 cycles
 *===========================================================================*/

/* DWT 寄存器地址 */
#define DWT_CTRL_REG    (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT_REG  (*(volatile uint32_t *)0xE0001004)
#define DEM_CR_REG      (*(volatile uint32_t *)0xE000EDFC)
#define DEM_CR_TRCENA   (1UL << 24)
#define DWT_CTRL_CYCCNTENA (1UL << 0)

/* 基于 600MHz 主频的周期数 */
#define CPU_FREQ_HZ     (600000000UL)
#define NS_TO_CYCLES(ns) ((uint32_t)(((uint64_t)(ns) * CPU_FREQ_HZ) / 1000000000ULL))

/* WS2812B-V5 时序目标值（取规格中间值，确保可靠） */
#define WS_T0H_NS      (300U)      /* 目标 300ns (规格 220~380ns) */
#define WS_T1H_NS      (790U)      /* 目标 790ns (规格 580~1000ns) */
#define WS_T0L_NS      (900U)      /* 目标 900ns (规格 580~1000ns) */
#define WS_T1L_NS      (410U)      /* 总位周期 ~1.2µs，此为补齐 */
#define WS_RES_US       (300U)      /* 复位 300µs (规格 >280µs，留裕量) */

/* 编译期计算各段的 CPU 周期数 */
static const uint32_t CYCLES_T0H = NS_TO_CYCLES(WS_T0H_NS);
static const uint32_t CYCLES_T1H = NS_TO_CYCLES(WS_T1H_NS);
static const uint32_t CYCLES_T0L = NS_TO_CYCLES(WS_T0L_NS);
static const uint32_t CYCLES_T1L = NS_TO_CYCLES(WS_T1L_NS);

/* GPIO 直接寄存器操作，跳过 gpio_set_level() 的函数调用开销 */
static volatile uint32_t *s_gpio_set_reg;     /* GPIO DR_SET 寄存器 */
static volatile uint32_t *s_gpio_clr_reg;     /* GPIO DR_CLEAR 寄存器 */
static uint32_t           s_gpio_pin_mask;    /* 引脚掩码 */

/*
 * 初始化 DWT 周期计数器
 */
static void dwt_init(void)
{
    DEM_CR_REG   |= DEM_CR_TRCENA;         /* 开启 DWT 模块 */
    DWT_CYCCNT_REG = 0U;                   /* 清零计数器 */
    DWT_CTRL_REG |= DWT_CTRL_CYCCNTENA;    /* 使能周期计数 */
}

/*
 * 等待指定的 CPU 周期数（精确忙等）
 */
static inline void dwt_delay_cycles(uint32_t start, uint32_t cycles)
{
    while ((DWT_CYCCNT_REG - start) < cycles);
}

/*
 * 初始化 GPIO 直接寄存器指针
 * RT1064 GPIO 寄存器布局:
 *   GPIOx_DR       = base + 0x00
 *   GPIOx_GDIR     = base + 0x04
 *   ...
 *   GPIOx_DR_SET   = base + 0x84
 *   GPIOx_DR_CLEAR = base + 0x88
 *   GPIOx_DR_TOGGLE= base + 0x8C
 */
static void ws2812_init_gpio_direct(void)
{
    /*
     * 逐飞库的 pin 编号: 高 5 位 = port index，低 5 位 = pin number
     * PORTPTR[] 返回 GPIO_Type* 基地址
     */
    GPIO_Type *port = PORTPTR[BOARD_WS2812_PIN >> 5];
    uint32_t pin_num = BOARD_WS2812_PIN & 0x1FU;

    s_gpio_set_reg  = &port->DR_SET;
    s_gpio_clr_reg  = &port->DR_CLEAR;
    s_gpio_pin_mask = (1UL << pin_num);
}

/*
 * 发送单个像素（24 bit GRB，高位在前）
 * 使用 DWT 周期计数器实现精确时序，无需 volatile 循环
 */
static void ws2812_send_pixel(uint8 grb[3])
{
    uint32_t grb24 = ((uint32_t)grb[0] << 16) | ((uint32_t)grb[1] << 8) | grb[2];
    uint32_t mask = s_gpio_pin_mask;
    volatile uint32_t *set_reg = s_gpio_set_reg;
    volatile uint32_t *clr_reg = s_gpio_clr_reg;

    for (int32_t i = 23; i >= 0; i--)
    {
        uint32_t start;

        if (grb24 & (1UL << i))
        {
            /* 发送 '1': 高电平 T1H，低电平 T1L */
            start = DWT_CYCCNT_REG;
            *set_reg = mask;                        /* PIN = HIGH */
            dwt_delay_cycles(start, CYCLES_T1H);
            *clr_reg = mask;                        /* PIN = LOW */
            dwt_delay_cycles(start, CYCLES_T1H + CYCLES_T1L);
        }
        else
        {
            /* 发送 '0': 高电平 T0H，低电平 T0L */
            start = DWT_CYCCNT_REG;
            *set_reg = mask;                        /* PIN = HIGH */
            dwt_delay_cycles(start, CYCLES_T0H);
            *clr_reg = mask;                        /* PIN = LOW */
            dwt_delay_cycles(start, CYCLES_T0H + CYCLES_T0L);
        }
    }
}

/*===========================================================================
 * 公共接口
 *===========================================================================*/

void ws2812_init(void)
{
    uint8_t i;
    uint8_t p;

    /* 初始化 GPIO 为推挽输出 */
    gpio_init(BOARD_WS2812_PIN, GPO, 0, GPO_PUSH_PULL);

    /* 初始化 DWT 精确计时 */
    dwt_init();

    /* 初始化 GPIO 直接寄存器指针 */
    ws2812_init_gpio_direct();

    /* 清零所有 LED 状态 */
    for (i = 0U; i < BOARD_WS2812_COUNT; i++)
    {
        for (p = 0U; p < 2U; p++)
        {
            s_ws2812_state[i][p].active       = 0U;
            s_ws2812_state[i][p].mode         = WS2812_MODE_CONSTANT;
            s_ws2812_state[i][p].color_on.r   = 0U;
            s_ws2812_state[i][p].color_on.g   = 0U;
            s_ws2812_state[i][p].color_on.b   = 0U;
            s_ws2812_state[i][p].color_off.r  = 0U;
            s_ws2812_state[i][p].color_off.g  = 0U;
            s_ws2812_state[i][p].color_off.b  = 0U;
            s_ws2812_state[i][p].period_ms    = 0U;
            s_ws2812_state[i][p].duration_ms  = 0U;
            s_ws2812_state[i][p].start_ms     = 0U;
        }

        s_ws2812_grb[i][0] = 0U;
        s_ws2812_grb[i][1] = 0U;
        s_ws2812_grb[i][2] = 0U;
    }

    /* V5 版本复位需要 >280µs，发送初始全黑帧 */
    system_delay_us(WS_RES_US);
    ws2812_show_array(s_ws2812_grb, BOARD_WS2812_COUNT);
}

void ws2812_show_array(uint8 grb_array[][3], uint16 count)
{
    uint32 primask;
    uint16 i;

    primask = interrupt_global_disable();

    for (i = 0U; i < count; i++)
    {
        ws2812_send_pixel(grb_array[i]);
    }

    /* V5 版本复位信号: 低电平 >280µs（旧版仅 50µs） */
    gpio_set_level(BOARD_WS2812_PIN, 0);
    system_delay_us(WS_RES_US);

    interrupt_global_enable(primask);
}

void ws2812_set_state(uint8 led_index,
                      ws2812_priority_e priority,
                      ws2812_mode_e mode,
                      uint8 r, uint8 g, uint8 b,
                      uint32 period_ms,
                      uint32 duration_ms)
{
    uint8 p;
    ws2812_state_t *st;

    if (led_index >= BOARD_WS2812_COUNT)
    {
        return;
    }

    p = (priority == WS2812_PRIORITY_HIGH) ? WS2812_PRIORITY_HIGH : WS2812_PRIORITY_LOW;
    st = &s_ws2812_state[led_index][p];

    st->active       = 1U;
    st->mode         = mode;
    st->color_on.r   = (uint8)(r / BOARD_WS2812_BRIGHTNESS_DIV);
    st->color_on.g   = (uint8)(g / BOARD_WS2812_BRIGHTNESS_DIV);
    st->color_on.b   = (uint8)(b / BOARD_WS2812_BRIGHTNESS_DIV);
    st->period_ms    = period_ms;
    st->duration_ms  = duration_ms;
    st->start_ms     = scheduler_get_tick_ms();
}

void ws2812_clear_state(uint8 led_index, ws2812_priority_e priority)
{
    uint8 p;
    ws2812_state_t *st;

    if (led_index >= BOARD_WS2812_COUNT)
    {
        return;
    }

    p = (priority == WS2812_PRIORITY_HIGH) ? WS2812_PRIORITY_HIGH : WS2812_PRIORITY_LOW;
    st = &s_ws2812_state[led_index][p];
    st->active      = 0U;
    st->duration_ms = 0U;
}

void ws2812_task(void)
{
    uint32 now_ms;
    uint8 i;

    if (!BOARD_WS2812_ENABLE)
    {
        return;
    }

    now_ms = scheduler_get_tick_ms();

    for (i = 0U; i < BOARD_WS2812_COUNT; i++)
    {
        ws2812_color_t result = {0U, 0U, 0U};
        int32_t p;

        for (p = WS2812_PRIORITY_HIGH; p >= WS2812_PRIORITY_LOW; p--)
        {
            ws2812_state_t *st = &s_ws2812_state[i][p];

            if (!st->active)
            {
                continue;
            }

            if (st->duration_ms > 0U)
            {
                uint32 elapsed = now_ms - st->start_ms;
                if (elapsed >= st->duration_ms)
                {
                    st->active = 0U;
                    continue;
                }
            }

            if (st->mode == WS2812_MODE_CONSTANT)
            {
                result = st->color_on;
            }
            else if (st->period_ms == 0U)
            {
                result = st->color_on;
            }
            else
            {
                uint32 t = (now_ms - st->start_ms) % st->period_ms;
                if (t < (st->period_ms / 2U))
                {
                    result = st->color_on;
                }
                else
                {
                    result = st->color_off;
                }
            }
            break;
        }

        s_ws2812_grb[i][0] = result.g;
        s_ws2812_grb[i][1] = result.r;
        s_ws2812_grb[i][2] = result.b;
    }

    ws2812_show_array(s_ws2812_grb, BOARD_WS2812_COUNT);
}
