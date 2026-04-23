# 赛博朋克视觉规范 (Cyberpunk Visual Standards)

## 核心调色板 (RGB565)
针对 IPS200 Pro (240x320) 和 LVGL 8 开发。

| 变量名 | 颜色值 | 十六进制 | 描述 |
| :--- | :--- | :--- | :--- |
| W_BG | 0x0000 | #000000 | 纯黑背景 |
| W_CARD | 0x1004 | #110022 | 深紫色卡片背景 |
| W_TEXT | 0xFFFF | #FFFFFF | 纯白主文字 |
| W_TEXT2 | 0x867F | #88CCFF | 浅蓝色副文字 |
| W_DIM | 0x3186 | #333333 | 暗灰色占位/失效态 |
| W_ACCENT | 0x07FF | #00FFFF | 霓虹青强调色 (选中) |
| W_ACCENT_LT | 0x0186 | #003333 | 深青色高亮背景 |
| W_ACCENT_BG | 0xF81F | #FF00FF | 霓虹粉强调色 (编辑) |
| W_WHITE | 0x0000 | #000000 | 黑色文字 (用于高亮背景) |
| W_GREEN | 0x3FE2 | #39FF14 | 霓虹绿 (在线/成功) |
| W_RED | 0xF807 | #FF003C | 鲜红色 (报警/停止) |
| W_AMBER | 0xFD00 | #FF9800 | 橙色 (警告) |

## IPS200 Pro 布局规范
- **行数 (ROWS)**: 5 (保证调参时的可读性)
- **行高 (ROW_H)**: 42px
- **间距 (ROW_GAP)**: 4px
- **标题高度 (HDR_H)**: 48px
- **页脚高度 (FT_H)**: 36px

## 状态设计模式 (State Styling)
始终遵循以下交互反馈逻辑：
1. **编辑态 (Editing Mode)**:
   - 背景: `W_ACCENT_BG` (霓虹粉)
   - 文字: `W_WHITE` (即黑色，提供最大反差)
2. **选中态 (Selected State)**:
   - 背景: `W_ACCENT_LT` (深青色)
   - 文字: `W_ACCENT` (霓虹青)
3. **普通态 (Normal State)**:
   - 背景: `W_CARD` (深紫色卡片)
   - 文字: `W_TEXT` (白色) 或 `W_TEXT2` (浅蓝)
