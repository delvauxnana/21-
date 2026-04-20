#ifndef DISPLAY_H
#define DISPLAY_H

#include "zf_common_headfile.h"
#include <stdint.h>

/*
 * 文件作用:
 * 1. 封装 IPS200 Pro 智慧屏控件式显示。
 * 2. 按菜单页创建页面和 label 控件，更新中文标题、状态和参数。
 * 3. 利用 IPS200 Pro 内置中文字库，无需外部字模文件。
 */

void display_init(void);
void display_show_page(uint8_t page_id);
void display_update_100ms(void);

#endif /* DISPLAY_H */
