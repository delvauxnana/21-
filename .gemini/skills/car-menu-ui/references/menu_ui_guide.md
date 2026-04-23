# 车端 UI 与 菜单上下文

## UI 架构概览
车端固件使用基于状态机的自定义文本菜单，显示在 128x64 屏幕上。核心逻辑位于 `car/project/code/src/menu.c` 和 `car/project/code/inc/menu.h`。

### 核心组件
- **状态管理**：`menu.h` 中的 `menu_snapshot_t` 结构体存储当前页面、光标位置、列表偏移量和编辑模式状态。
- **更新循环**：`menu_update_20ms()` 被周期性调用，轮询硬件按键（上、下、确认、返回）并触发状态转换。
- **渲染驱动**：`car/project/code/src/display.c` 根据 `menu_get_snapshot()` 提供的快照信息进行 UI 绘制。

## 菜单结构
- **层级**：锁屏页 (LOCKSCREEN) -> 主页 (HOME) -> [状态页, 运动测试, 位姿页, 参数设置]。
- **子页面**：运动测试包含 轴向移动测试 (AXIS_TEST) 和 单轮测试 (WHEEL_TEST)。
- **编辑模式**：PID 参数、测试距离等项可在“编辑模式”下调整（由 `g_menu_snapshot.edit_mode` 标识）。

## 开发指南

### 1. 添加新菜单页面
1.  **定义页面**：在 `menu.h` 的 `menu_page_t` 枚举中添加新页面。
2.  **条目计数**：定义条目数量宏，并更新 `menu.c` 中的 `menu_get_item_count()`。
3.  **实现处理器**：创建 `menu_handle_xxx_page()` 函数处理该页面的按键逻辑。
4.  **更新跳转**：在 `menu_update_20ms()` 的 `switch` 语句中集成新处理器。

### 2. 参数修改规范
-   可调参数在 `menu_adjust_xxx_item()` 函数中处理。
-   **必须使用安全限幅**：使用 `menu_limit_i32()` 或 `menu_limit_f()` 强制执行硬件安全限制（如电机转速、PID 增益）。
-   进入编辑模式时应调用 `system_state_set_mode(APP_MODE_PARAM_EDIT)` 以便在必要时暂停后台任务。

### 3. 按键映射
-   **上/下**：移动光标或增减数值。
-   **确认 (OK)**：进入子菜单、切换编辑模式或确认执行动作（如启动电机测试）。
-   **返回 (BACK)**：返回上级页面或退出编辑模式。

## 最佳实践
-   保持处理器逻辑非阻塞，避免在 `menu_update_20ms()` 中使用长延时。
-   光标移动后务必调用 `menu_sync_list_offset()`，确保当前选中的条目在小屏幕上可见。
-   UI 状态需与 `motion_manager` 协同，防止在菜单导航时产生冲突的电机控制指令。