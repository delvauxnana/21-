#ifndef GRID_MAP_MODULE_H
#define GRID_MAP_MODULE_H

#include "zf_common_headfile.h"
#include <stdint.h>

/*
 * 文件作用:
 * 1. 存放 16x12 栅格地图和元素枚举。
 * 2. 作为路径规划和任务状态机的基础数据结构。
 */

#define GRID_MAP_W                  (16U)   /* 地图 X 方向格子数 */
#define GRID_MAP_H                  (12U)   /* 地图 Y 方向格子数 */

typedef enum
{
    GRID_CELL_UNKNOWN = 0,  /* 未知格，尚未识别完成 */
    GRID_CELL_EMPTY,        /* 空地，可通行 */
    GRID_CELL_WALL,         /* 墙体，不可通行 */
    GRID_CELL_BOX,          /* 普通箱子 */
    GRID_CELL_GOAL,         /* 目标点 */
    GRID_CELL_BOMB,         /* 炸弹箱 */
    GRID_CELL_CAR           /* 虚拟车模所在位置 */
} grid_cell_t;

typedef enum
{
    CAR_DIR_UP    = 0,      /* 朝上 (+Y) */
    CAR_DIR_RIGHT = 1,      /* 朝右 (+X) */
    CAR_DIR_DOWN  = 2,      /* 朝下 (-Y) */
    CAR_DIR_LEFT  = 3       /* 朝左 (-X) */
} car_direction_t;

typedef struct
{
    grid_cell_t cells[GRID_MAP_H][GRID_MAP_W]; /* 栅格地图单元数组，按 [y][x] 索引 */
} grid_map_t;

void grid_map_clear(grid_map_t *map);
grid_cell_t grid_map_get_cell(const grid_map_t *map, uint8_t x, uint8_t y);
void grid_map_set_cell(grid_map_t *map, uint8_t x, uint8_t y, grid_cell_t cell);

#endif /* GRID_MAP_MODULE_H */
