# Car 目录协作指南

## 工作目标
这个目录是主车软件工作区。处理这里的任务时，优先判断用户是在改整车固件、共享底层，还是 `dashboard` 前端；如果上下文已经足够明确，就直接进入对应子目录工作，不要来回确认显而易见的事情。

## 目录分工
`project/user/` 放启动、入口、ISR 相关手写代码，例如 `main.c` 和 `isr.c`。`project/code/inc/` 与 `project/code/src/` 是整车业务逻辑主战场，包含运动管理、控制、协议、里程计、导航、显示、视觉、WS2812 等模块。协议和视觉相关说明文档主要在 `project/protocol_spec.md` 与 `project/code/doc/`。

`dashboard/` 是独立的 Vite + React 前端。源代码在 `dashboard/src/`，静态资源在 `dashboard/public/`。`dashboard/dist/` 与 `dashboard/node_modules/` 不做人工编辑。

`libraries/` 是 SDK、驱动和公共器件支持。默认先在 `project/` 查找更合适的修改点，只有确实属于共享底层问题时才改 `libraries/`，并且改动要尽量小、尽量局部。

## 默认修改策略
涉及整车行为、状态机、控制流程、通信、菜单、显示、传感器调度时，优先修改 `project/code/` 或 `project/user/`。涉及网页界面、可视化、卡片布局、浏览器交互时，优先修改 `dashboard/`。

如果需要新增固件文件，除了创建 `.c` / `.h` 外，还要同步检查 MDK 与 IAR 工程是否已包含该文件。不要假设 IDE 会自动收录新文件。

如果用户没有明确要求，不要顺手改大段 `libraries/` 厂商代码，也不要混合做“前端改一点、固件改一点”的跨域大杂烩提交；优先把一次任务收敛到一个明确范围。

## 构建与验证
固件工程入口：
- `project/mdk/rt1064.uvprojx`
- `project/iar/rt1064.eww`

Keil 目标名：
- `nor_sdram_zf_dtcm`

命令行示例：
`UV4.exe -b car\project\mdk\rt1064.uvprojx -t nor_sdram_zf_dtcm`

`dashboard/` 常用命令：
- `npm run dev`
- `npm run lint`
- `npm run build`

固件改动的最小验证标准是：至少一次成功构建，并完成与修改点直接相关的板级检查。尤其是改到 `main.c`、调度、协议、运动控制、显示或 WS2812 时，要额外关注启动流程、心跳/遥测发送、外设初始化和周期任务是否仍然正常。

前端改动的最小验证标准是：在 `car/dashboard/` 下运行 `npm run lint` 和 `npm run build`。如果改动涉及页面布局或交互，优先补一次浏览器实际检查。

## 编码习惯
C 代码保持现有 4 空格缩进、Allman 风格大括号、`snake_case` 命名和 `.c` / `.h` 配对。公共接口放 `project/code/inc/`，实现放 `project/code/src/`。尽量保持原有 include 顺序和文件风格，不引入与周围代码明显不一致的写法。

React 代码遵循 `dashboard/eslint.config.js`。除非任务本身要求，不要编辑构建产物或依赖目录。

## 输出要求
完成任务后，优先告诉用户：
- 改了哪些模块或页面
- 是否完成了构建、lint 或板级验证
- 是否还有未验证硬件路径、未接线外设或需要用户上板确认的部分
