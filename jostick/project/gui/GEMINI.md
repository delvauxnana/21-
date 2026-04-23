# GEMINI.md - 摇杆端 GUI (LVGL) 上下文

## GUI 架构概览
摇杆端固件使用 **LVGL 8** 作为图形库。项目通过 NXP 的 **GUI Guider** 进行布局设计，但核心应用逻辑和事件处理手动实现在 `custom/` 子文件夹中。

### 核心目录说明
-   `generated/`：GUI Guider 自动生成的文件（图像、字体、屏幕设置）。**严禁手动修改。**
-   `custom/`：手动实现的代码，包括 `custom.c`（动态 UI 逻辑，如 PID 表格）和 `ui_user_events.c`（页面跳转、LVGL 事件钩子、硬件按键映射）。

## 按键与 LVGL 映射
硬件按键通过输入设备接口映射为 LVGL 标准导航键：
-   `LV_KEY_ENTER`：确认/进入编辑。
-   `LV_KEY_ESC`：返回/取消。
-   `LV_KEY_PREV / LV_KEY_UP`：上一个条目 / 增加数值。
-   `LV_KEY_NEXT / LV_KEY_DOWN`：下一个条目 / 减少数值。

## 开发指南

### 1. 职责分离
-   所有动态 UI 更新（如刷新电池电压、更新 PID 数值表）必须在 `custom.c` 中实现。
-   屏幕跳转逻辑和硬件事件钩子属于 `ui_user_events.c`。

### 2. 输入组与焦点管理
LVGL 导航需要将对象添加到输入组 (`lv_group_t`)：
-   通过 `board_input_get_group()` 获取当前活动组。
-   在页面加载时（如 `on_menu_loaded`），**必须**先调用 `ui_user_clear_group()` 清除旧焦点，再使用 `lv_group_add_obj()` 添加新页面的可交互对象。
-   对于自定义组件（如 PID 调参器）的内部编辑模式，需切换 `lv_group_set_editing()` 状态。

### 3. 页面加载规范
始终使用包装函数 `ui_load_scr_animation()` 加载新屏幕，以确保正确管理内存（卸载旧屏幕）并提供平滑的过渡动画。

### 4. 自定义组件 (例如 PID 调参器)
PID 调参界面是在 `custom.c` 中手动绘制的：
-   使用静态数组 `s_pid_items` 定义参数信息。
-   该页面的硬件按键事件由 `custom_pid_tune_handle_key()` 拦截处理。
-   修改完成后，确保调用 `protocol_send_param_set()` 将参数同步至车端。

## 最佳实践
-   不要手动编辑 `generated/` 文件夹；如需调整布局，请使用 GUI Guider 重新生成，然后更新 `custom/` 中的钩子。
-   访问 UI 元素前务必检查 `ui != NULL`，防止在页面切换期间发生崩溃。
-   利用 `lv_obj_add_event_cb` 监听 `LV_EVENT_SCREEN_LOADED` 事件来进行初始化配置。
