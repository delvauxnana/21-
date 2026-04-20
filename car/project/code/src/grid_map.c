#include "grid_map.h"

/*
 * 文件作用：
 * 1. 提供地图的基础读写操作。
 * 2. 后续给 solver 和显示模块复用。
 */

void grid_map_clear(grid_map_t *map)
{
    uint8_t x;
    uint8_t y;

    if (map == 0)
    {
        return;
    }

    for (y = 0U; y < GRID_MAP_H; ++y)
    {
        for (x = 0U; x < GRID_MAP_W; ++x)
        {
            map->cells[y][x] = GRID_CELL_UNKNOWN;
        }
    }
}

grid_cell_t grid_map_get_cell(const grid_map_t *map, uint8_t x, uint8_t y)
{
    if ((map == 0) || (x >= GRID_MAP_W) || (y >= GRID_MAP_H))
    {
        return GRID_CELL_UNKNOWN;
    }

    return map->cells[y][x];
}

void grid_map_set_cell(grid_map_t *map, uint8_t x, uint8_t y, grid_cell_t cell)
{
    if ((map == 0) || (x >= GRID_MAP_W) || (y >= GRID_MAP_H))
    {
        return;
    }

    map->cells[y][x] = cell;
}
