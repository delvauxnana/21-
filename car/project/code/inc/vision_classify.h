#ifndef VISION_CLASSIFY_H
#define VISION_CLASSIFY_H

#include "zf_common_headfile.h"
#include "app_types.h"

/*
 * 文件作用：
 * 1. 负责分类 OpenART 的通信和结果解析。
 * 2. 向上层提供最近一次图片或数字识别结果。
 */

void vision_classify_init(void);
void vision_classify_poll(void);
classify_result_t vision_classify_get_latest(void);

#endif /* VISION_CLASSIFY_H */
