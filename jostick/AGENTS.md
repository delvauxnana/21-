# Jostick 目录协作指南

## 工作目标
这个目录是手柄控制器工程，包含运行时逻辑、输入采集、无线协议和 LVGL 界面。处理这里的任务时，先分清用户要改的是应用逻辑、输入链路、界面行为，还是生成界面本身，然后把改动尽量限制在最靠近业务的一层。

## 目录分工
`project/app/` 是手写应用逻辑主入口，包含 `main.c`、中断、板级初始化、输入采集与协议处理等代码。与通信相关的说明文档在 `project/app/protocol_spec.md`。

`project/gui/custom/` 是手写 LVGL 自定义层，适合放自定义事件、业务联动、界面状态更新和用户逻辑。只要这里能解决，就优先不要碰生成代码。

`project/gui/generated/` 是 GUI Guider 生成目录，包含 `gui_guider.c`、`setup_scr_*.c`、字体和图片资源。默认把它当作生成物处理，除非用户明确要求直接修改，或者确实没有合适的自定义扩展点；如果必须修改，要在结果里明确说明这些改动可能被重新生成覆盖。

`project/code/lvgl/` 是 LVGL 库本体，`project/code/lv_conf.h` 与 `project/code/lv_conf_ext.h` 是配置层。普通业务功能不要直接改 LVGL 内核；优先改 `project/app/`、`project/gui/custom/` 或配置层。

`libraries/` 是底层 SDK、驱动和设备支持。和主车工程一样，只有在问题明确属于共享底层时才修改这里。

## 默认修改策略
涉及协议、按键、摇杆、板级初始化、状态同步时，优先改 `project/app/`。涉及界面交互、界面状态驱动、用户事件处理时，优先改 `project/gui/custom/`。只有在界面结构本身必须调整且无法通过自定义层接管时，才考虑 `project/gui/generated/`。

如果新增了 `project/app/` 或 `project/gui/custom/` 下的手写源文件，要同步确认 MDK 和 IAR 工程都已经纳入这些文件。

除非任务明确要求，不要把一次修改扩散到 `project/code/lvgl/` 大量底层文件；这类改动维护成本高，也更容易引入回归。

## 构建与验证
固件工程入口：
- `project/mdk/rt1064.uvprojx`
- `project/iar/rt1064.eww`

Keil 目标名：
- `nor_sdram_zf_dtcm`

命令行示例：
`UV4.exe -b jostick\project\mdk\rt1064.uvprojx -t nor_sdram_zf_dtcm`

最小验证标准是：至少完成一次成功构建，并针对改动路径做设备侧冒烟检查。界面类修改要关注页面创建、控件事件绑定、焦点与输入流是否正常；输入或协议类修改要关注摇杆采样、按键更新、无线串口接收与解析、界面状态同步是否正常。

如果改到 `main.c`、`board_input_*`、`protocol_*`、`custom_*` 或 `ui_user_events.*`，结果里要明确说清楚哪些路径已验证、哪些还需要上板确认。

## 编码习惯
C 代码保持现有 4 空格缩进、Allman 大括号和 `snake_case` 命名。应用逻辑放 `project/app/`，UI 自定义逻辑放 `project/gui/custom/`，生成界面与 LVGL 内核默认不作为常规手写区。

不要提交构建产物、临时目录或自动生成但本次无需变更的文件，尤其是：
- `project/mdk/Objects/`
- `project/mdk/Listings/`
- `project/mdk/pdf_images/`

## 输出要求
完成任务后，优先告诉用户：
- 改动落在 `app`、`gui/custom`、`gui/generated` 还是 `lvgl` 配置层
- 是否完成了构建与设备侧验证
- 是否存在“生成代码可能覆盖手工修改”或“仍需上板确认输入链路”的风险
