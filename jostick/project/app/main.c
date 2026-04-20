/*********************************************************************************************************************
* RT1064DVL6A Opensourec Library （RT1064DVL6A 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2022 SEEKFREE 逐飞科技
* 
* 本文件是 RT1064DVL6A 开源库的一部分
* 
* RT1064DVL6A 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License， GNU通用公共许可证）
* 的 GPL 的第3版（即 GPL3.0）或选择的任何后续版本，重新发布/修改
* 
* 开源库的发布是希望能够有用，但未提供任何的保证
* 甚至没有对适销或特定用途的保证
* 更详细请参见 GPL
* 
* 您应该已在收到开源库的同时收到一份 GPL 的副本
* 如果没有，请<https://www.gnu.org/licenses/>
* 
* 备注：
* 开源库使用 GPL3.0 开源许可证协议 以上为中文版本
* 英文版本在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证在 libraries 文件夹 下文件夹下的 LICENSE 文件
* 欢迎各位使用并传播 修改时请保留逐飞科技的版权声明
* 
* 文件名          main
* 公司名          成都逐飞科技有限公司
* 版本信息        查看 libraries/doc 文件夹下 version 文件 版本说明
* 开发环境        IAR 8.32.4 or MDK 5.33
* 适用平台        RT1064DVL6A
* 店铺链接        https://seekfree.taobao.com/
* 
* 修改记录
* 日期              作者                备注
* 2022-09-21        SeekFree            first version
********************************************************************************************************************/

#include "zf_common_headfile.h"
#include "lvgl.h"
#include "board_init.h"
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"
#include "board_input.h"
#include "protocol.h"

int main(void)
{
    clock_init(SYSTEM_CLOCK_600M);
    debug_init();

    // LVGL 初始化
    lv_init();
    board_init();

    // GUI Guider 界面初始化
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    custom_init(&guider_ui);

    // 输入设备初始化（按键 + 摇杆）
    board_input_init(&guider_ui);

    // 无线串口硬件初始化（必须在 protocol_init 之前）
    wireless_uart_init();

    // 无线串口协议解析器初始化
    protocol_init();

    // PIT 定时器
    pit_ms_init(PIT_CH0, 5);    // CH0: 5ms — LVGL 时钟节拍
    pit_ms_init(PIT_CH1, 10);   // CH1: 10ms — 按键扫描 + 摇杆 ADC + 协议看门狗
    interrupt_set_priority(PIT_IRQn, 0);

    while(1)
    {
        lv_timer_handler();
        board_input_joystick_update(&guider_ui);
        board_input_status_update(&guider_ui);
        protocol_poll();    // 非阻塞：从无线串口 FIFO 取字节 → 状态机解析
    }
}
