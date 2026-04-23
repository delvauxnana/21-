# 仓库协作指南

## 协作方式
默认使用中文交流，表达尽量直接、简洁、可执行。开始处理任务时，先用 1 到 2 句话说明理解和下一步；如果已经足够明确，就直接动手，不要长时间停留在“给方案但不落地”的状态。

做较大修改、跨目录修改、构建验证、删除产物、改动生成代码或厂商代码前，先明确告诉用户准备做什么。遇到信息不完整但风险较低的情况，优先做合理假设并继续推进，同时在结果中把假设说清楚；只有在假设会明显影响结果时再提问。

结束时优先汇报三件事：改了什么、怎么验证的、还剩什么风险或未完成项。进入 `car/` 或 `jostick/` 后，优先遵循各自目录下的 `AGENTS.md`。

## 项目结构
`car/` 是主车工程，包含整车固件、共享底层库和一个独立的 `dashboard` 前端。固件入口主要在 `car/project/user/`，应用模块主要在 `car/project/code/{src,inc}/`，共享 SDK、驱动和器件支持在 `car/libraries/`。

`jostick/` 是手柄控制器工程，手写应用代码主要在 `jostick/project/app/` 和 `jostick/project/gui/custom/`。`jostick/project/gui/generated/` 与 `jostick/project/code/lvgl/` 默认视为生成物或第三方代码，不作为常规业务改动入口。

`Example/` 是厂商示例与参考实现。根目录下的 PDF、原理图、封装、尺寸图等主要作为资料使用，不是常规开发目录。

## 构建与验证
`car/dashboard/` 使用 Vite + React，常用命令如下：
- `npm run dev`
- `npm run lint`
- `npm run build`

固件工程使用 Keil MDK 或 IAR，主工程入口分别在：
- `car/project/mdk/rt1064.uvprojx`
- `car/project/iar/rt1064.eww`
- `jostick/project/mdk/rt1064.uvprojx`
- `jostick/project/iar/rt1064.eww`

Keil 目标名为 `nor_sdram_zf_dtcm`，命令行示例：
`UV4.exe -b car\project\mdk\rt1064.uvprojx -t nor_sdram_zf_dtcm`

本仓库没有自动化固件测试框架。默认最小验证标准是：能成功构建、没有新增关键告警、并完成一轮与改动相关的板级冒烟测试。涉及前端时，至少运行 `npm run lint` 和 `npm run build`。

## 代码与目录约束
C 代码保持现有 4 空格缩进、Allman 大括号、`snake_case` 函数名、`UPPER_SNAKE_CASE` 宏名，以及 `.c` / `.h` 成对组织方式。通用接口放在 `inc/`，实现放在 `src/`。没有统一格式化工具时，尽量保持原文件风格和 include 顺序。

React 代码使用 ES Modules，组件文件保持现有命名习惯，例如 `App.jsx`。前端风格以 `car/dashboard/eslint.config.js` 为准。

新增固件源文件时，不要只把文件放进目录里，还要确认 MDK 与 IAR 工程都已纳入该文件；这些工程默认不会自动发现新文件。

## 提交与产物
提交信息建议使用简短祈使句，并带清晰范围，例如 `car: adjust ws2812 status update` 或 `dashboard: simplify telemetry layout`。

不要提交构建产物、依赖目录或临时文件，尤其是：
- `Objects/`
- `Listings/`
- `dist/`
- `node_modules/`

如果构建过程生成了临时产物，优先使用各工程 `mdk/` 或 `iar/` 目录下已有的清理脚本处理。

## 工具优先级
优先使用已配置的 MCP 工具而不是临时外部搜索：
- 本地手册、原理图、数据手册优先用 `pdf`
- 库或框架文档优先用 `context7`
- `car/dashboard/` 的界面检查优先用 `playwright`
- 远程仓库、Issue、PR 相关操作优先用 `github`
