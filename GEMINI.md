# GEMINI.md - 项目上下文

## 项目概览
本项目是专为 NXP I.MX RT1064 微控制器开发的开源平台，特别针对**智能车**竞赛领域。项目基于 **逐飞科技 (SeekFree) RT1064 开源库**，通过将代码加载到 SDRAM 并利用 TCM 实现高速执行来优化性能，主要目的实现对小车固件和摇杆固件中的UI进行编写。

项目分为三个主要部分：
1.  **小车固件 (`car/`)**：主车辆固件，包括电机控制、底盘运动学、IMU 集成和里程计。
2.  **摇杆固件 (`jostick/`)**：基于 LVGL GUI 的远程控制器，用于控制小车。
3.  **仪表盘 (`car/dashboard/`)**：基于 React 的 Web 界面，用于远程监控和可视化。

## 架构与核心技术
-   **微控制器**: NXP i.MX RT1064 (Cortex-M7 @ 600MHz)。
-   **固件框架**: 逐飞科技 RT1064 开源库。
-   **GUI 框架**: LVGL (用于摇杆端)。
-   **通信协议**: 自定义串口协议 (帧头: `0x55 0xAA`)，通过无线串口 (LPUART8) 传输。
-   **Web 仪表盘**: React 19, Vite, Tailwind CSS。

## 目录结构
-   `/car/project/user/`: 小车固件的主要入口。
-   `/car/project/code/{src,inc}/`: 小车应用模块。
-   `/jostick/project/app/`: 摇杆应用逻辑。
-   `/jostick/project/gui/custom/`: 摇杆自定义 UI 代码。
-   `/libraries`: 逐飞科技提供的共享 SDK、驱动和设备支持。
-   `/Example`: RT1064 外设的参考例程。

## 构建与运行

### 固件 (小车 & 摇杆)
-   **IAR**: 工作区位于 `project/iar/rt1064.eww`。
-   **MDK (Keil)**: 项目位于 `project/mdk/rt1064.uvprojx`。
    -   **目标名称**: `nor_sdram_zf_dtcm`。
    -   **CLI 构建**: `UV4.exe -b <项目路径> -t nor_sdram_zf_dtcm`。
-   **注意**: 新的源文件必须手动添加到 MDK 和 IAR 项目文件中，因为它们不支持自动发现。

### 仪表盘
-   **路径**: `car/dashboard/`。
-   **命令**: `npm install`, `npm run dev`, `npm run lint`, `npm run build`。

## 开发规范
-   **C 语言风格**: 4 空格缩进，Allman 括号风格，`snake_case` 函数名，`UPPER_SNAKE_CASE` 宏定义。
-   **头文件/源文件**: `.c` 和 `.h` 配对；`inc/` 存放通用接口，`src/` 存放实现。
-   **React 风格**: ES 模块，命名规范如 `App.jsx`，ESLint 规则位于 `car/dashboard/eslint.config.js`。
-   **协议**: 严格遵守 `car/project/protocol_spec.md`。
-   **通信**: **必须始终使用中文与用户对话**。偏好直接且简练的交流方式。
