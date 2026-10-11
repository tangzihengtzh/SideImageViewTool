# ScreenDock Implementation Plan

**Goal:** 原生 Windows 截图便签：四边自动收起、区域截图、置顶缩放、PNG 历史。

**Architecture:** C++17 Win32 管理窗口与全局热键；Direct2D / DirectWrite 绘制；WIC 编解码 PNG。历史缩略图按需读取，原图只在置顶时载入；工作线程保存 PNG，避免阻塞交互。

**Tech Stack:** MSVC 2022, Windows SDK, CMake, Win32, Direct2D, DirectWrite, WIC，静态 C++ 运行库。

本任务在当前会话连续执行，所有文件位于独立 ScreenDock 文件夹。

## 文件结构与执行步骤

- [x] `CMakeLists.txt`、`src/app.manifest`：生成 x64 Windows EXE，开启 PerMonitorV2 DPI 和静态运行库。
- [x] `src/core.h`、`src/core.cpp`：实现矩形归一化、裁剪、缩放边界、WIC PNG 读写、缩略图、GDI 屏幕采集和异步保存队列。
- [x] `src/ui.h`、`src/ui.cpp`：封装 Direct2D 绘制、文字、按钮；处理设备资源失效后重建。
- [x] `src/app.h`、`src/app.cpp`：单实例、托盘、Ctrl+Alt+A 热键、四边/显示器菜单、配置、图片粘贴导入及历史。
- [x] `src/dock.cpp`：窄边条悬停展开，延迟收起，最近图片列表、分页、滚轮调整缩略图大小，点击置顶。
- [x] `src/pin.cpp`：置顶便签，拖动位置、滚轮缩放、双击 100%、复制、关闭、透明度、右键菜单。
- [x] `src/capture.cpp`：隐藏工具窗口后冻结虚拟桌面；拖拽框选，显示实际尺寸，Esc 取消，选区裁剪后异步归档。
- [x] `src/main.cpp`、`tests/self_test.cpp`：正常启动和 `--self-test <输出目录>`，验证反向框选、缩放、像素裁剪、PNG 往返、坏文件、多窗口创建和置顶、四边布局、窗口消息缩放、关闭保留文件。
- [x] `build.ps1`、`README.md`：一键构建和中文使用说明，生成 `dist/ScreenDock.exe` 与说明。

## 验证命令

从项目目录执行 `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`，随后 `cmake --build build --config Release`。两项均应返回 0。

执行 `dist/ScreenDock.exe --self-test tests/output`，由退出码 0 与 `tests/output/report.txt` 中逐项 PASS 作为自动化证据。以程序内部 UI 测试验证真实 HWND、Direct2D 渲染目标及消息处理，不把它等同于人工交互验收。

核对交付清单：四边停靠、区域截图、历史 PNG、前台便签、滚轮缩放、快捷键、托盘退出、中文说明和独立产物目录。记录实测 EXE 大小与启动后的进程内存。
