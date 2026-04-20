#include "vision_classify.h"

static classify_result_t g_classify_result;

/*
 * 文件作用：
 * 1. 接收和缓存分类 OpenART 发来的识别结果。
 * 2. 后续在这里补充串口解析和超时判断。
 */

void vision_classify_init(void)
{
    g_classify_result = (classify_result_t){0};
}

void vision_classify_poll(void)
{
    /* TODO: 轮询分类串口缓冲区，解析识别结果。 */
}

classify_result_t vision_classify_get_latest(void)
{
    return g_classify_result;
}
