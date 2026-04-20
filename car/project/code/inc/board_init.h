#ifndef BOARD_INIT_H
#define BOARD_INIT_H

#include "zf_common_headfile.h"

/*
 * 文件作用：
 * 1. 统一管理板级初始化流程。
 * 2. 控制各类外设的初始化顺序和上电顺序。
 */

void board_init_clock(void);
void board_init_peripherals(void);
void board_init_all(void);

#endif /* BOARD_INIT_H */
