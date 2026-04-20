# Repository Guidelines

## Project Structure & Module Organization
`car/` is the main vehicle workspace: firmware lives in `car/project/user/` for startup and ISR entry points, `car/project/code/{src,inc}/` for application modules such as `motion_manager`, `protocol`, and `display`, and `car/libraries/` for shared SDK, drivers, and device support. `car/dashboard/` is a separate Vite + React UI with source in `src/` and static assets in `public/`. `jostick/` is a sibling controller firmware project; keep handwritten code in `jostick/project/app/` and `jostick/project/gui/custom/`, not `jostick/project/gui/generated/` or `jostick/project/code/lvgl/`. `Example/` contains vendor demos and reference implementations, while the top-level PDFs and hardware folders are reference material rather than active source.

## Build, Test, and Development Commands
From `car/dashboard/`:
- `npm run dev` starts the local dashboard.
- `npm run build` creates a production bundle in `dist/`.
- `npm run lint` runs ESLint on `*.js` and `*.jsx`.

Firmware is built in Keil MDK or IAR. Main entry points are `car/project/mdk/rt1064.uvprojx`, `car/project/iar/rt1064.eww`, and the matching files under `jostick/project/`. The Keil target name is `nor_sdram_zf_dtcm`; CLI example: `UV4.exe -b car\project\mdk\rt1064.uvprojx -t nor_sdram_zf_dtcm`. Use the cleanup batch files in each `mdk/` or `iar/` folder before committing artifacts.

## Coding Style & Naming Conventions
C code follows the existing 4-space indentation, Allman braces, `snake_case` function names, `UPPER_SNAKE_CASE` macros, and paired `module.c` / `module.h` files. Keep reusable interfaces in `inc/` and implementation in `src/`. React code uses ES modules, PascalCase component files such as `App.jsx`, and camelCase variables. No formatter is checked in; preserve current include/import ordering and use `car/dashboard/eslint.config.js` as the JavaScript standard.

## Testing Guidelines
No automated firmware test framework or coverage gate is configured. Minimum validation is a successful MDK/IAR build, no new warnings, and a brief board-level smoke test for protocol, motor, sensor, or display changes. For dashboard changes, run `npm run lint` and `npm run build` before review.

## Commit & Pull Request Guidelines
This snapshot does not include a `.git/` directory, so local commit history cannot be inspected. Use short imperative commit subjects with a clear scope, for example `car: tune ws2812 status logic` or `dashboard: simplify telemetry card`. Pull requests should list touched areas, board/toolchain used, validation performed, linked issues, and screenshots for dashboard or LVGL UI changes. Do not commit generated folders such as `Objects/`, `Listings/`, `dist/`, or `node_modules/`.

## Agent-Specific Instructions
Prefer configured MCP servers before ad hoc web search when they match the task: use `pdf` for local hardware manuals and schematics, `context7` for current library and framework docs, `playwright` for `car/dashboard/` UI checks, and `github` for remote repos, issues, and PRs when `GITHUB_TOKEN` is available.
