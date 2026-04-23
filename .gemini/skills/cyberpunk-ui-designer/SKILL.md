---
name: cyberpunk-ui-designer
description: 用于在车端和摇杆端固件中应用和维护“赛博朋克”（Cyberpunk）视觉主题的技能。当需要修改 UI 颜色、优化屏幕布局、或在 LVGL 中应用统一视觉风格时使用。
---

# Cyberpunk UI Designer (赛博朋克 UI 设计师)

## 概述
本技能封装了针对 NXP i.MX RT1064 智能车项目的视觉设计标准。它指导如何应用高对比度的霓虹配色方案（赛博朋克风），并确保在 240x320 IPS 屏幕和 LVGL GUI 上保持一致的用户体验。

## 核心规范
请参考 [visual-standards.md](references/visual-standards.md) 获取完整的调色板 (RGB565) 和布局参数。

## 快速应用指南

### 1. 修改 C 语言 UI 代码 (IPS200 Pro)
在处理 `display.c` 或类似文件时，确保定义了标准的配色宏，并使用 `rs()` 函数模式来处理行样式：

```c
static void rs(uint8_t i, bool sel, bool ed) {
    if (ed) {
        // 编辑态: 霓虹粉背景 + 黑色文字
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_WHITE);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_ACCENT_BG);
    } else if (sel) {
        // 选中态: 深青色背景 + 霓虹青文字
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_ACCENT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_ACCENT_LT);
    } else {
        // 普通态: 深紫色背景 + 纯白文字
        ips200pro_set_color(wNm[i], COLOR_FOREGROUND, W_TEXT);
        ips200pro_set_color(wNm[i], COLOR_BACKGROUND, W_CARD);
    }
}
```

### 2. 应用于 LVGL (摇杆端)
在修改 `custom.c` 中的 LVGL 对象样式时，手动映射 RGB565 颜色：
- `LV_COLOR_MAKE(0x00, 0xFF, 0xFF)` -> 对应 `W_ACCENT`
- `LV_COLOR_MAKE(0xFF, 0x00, 0xFF)` -> 对应 `W_ACCENT_BG`
- 背景始终使用黑色或极深紫色。

## 最佳实践
- **对比度优先**：在鲜艳的背景上（如编辑模式的粉色）务必使用黑色文字。
- **一致性**：所有页面的标题栏高度和颜色必须统一。
- **反馈**：确保按键操作能立即触发颜色的状态转换（如选中项的高亮切换）。

### 3. 动态层级菜单与自适应布局架构 (LVGL)
当遇到较长的参数列表（如 PID 调节）时，为保持赛博朋克大字体和高对比度的美观性，应采用以下 UI 架构规范：
- **层级分类**：将长列表拆分为“主菜单 -> 子菜单”结构。
- **动态容器与字体自适应**：
  - 当页面项数量 **≤ 3** 时：使用大卡片（例如高度 `58`）和大号字体（例如 `lv_font_ALMM_23`）。
  - 当页面项数量 **> 3** 时：使用紧凑卡片（例如高度 `38`）和常规字体（例如 `lv_font_ALMM_16`）。
- **容器等距平铺**：不写死 Y 轴坐标，而是根据当前激活项数量动态计算间距：`gap = (area_h - (active_count * row_h)) / (active_count - 1)`，使元素在可用区域内均匀垂直分布。
- **卡片焦点毛玻璃特效**：
  - **普通态**：深色半透明背景（`bg_opa: 120`, `bg_color: 0x000000`），带有微透明细边框（`border_width: 1`, `border_color: 0xFFFFFF`, `border_opa: 50`）。
  - **焦点态**：背景加深（`bg_opa: 160`），边框加粗高亮（`border_width: 2`, `border_color: CLR_PRIMARY/CLR_EDIT`, `border_opa: 255`）。
- **动态左侧光标**：废弃右侧静态箭头，在行容器左侧使用独立的光标 Label（如 `>`）。利用透明度 (`text_opa`) 实现选中态显示 (`255`)，未选中态隐藏 (`0`)，并跟随高亮边框改变颜色。
