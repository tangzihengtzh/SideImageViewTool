# Immediate Dock Collapse Implementation Plan

> **For agentic workers:** Execute inline and preserve the user's no-foreground-testing requirement. Do not display windows, move the pointer, capture the desktop, use the clipboard or restart the user's running copy.

**Goal:** 鼠标离开展开边栏时同步收起，消除 650 ms 等候及轮询延迟，发布 v1.2.1。

**Architecture:** WM_MOUSELEAVE 直接收起；菜单和截图期间暂缓，菜单操作结束后立即检查指针位置。首次 WM_MOUSEMOVE 在展开前就注册 TME_LEAVE，避免快速进出漏掉离开通知。复制成功反馈改为独立定时器，不再依赖鼠标位置轮询。

**Tech Stack:** C++17、Win32 窗口消息、TrackMouseEvent、CMake / CTest、PowerShell。

### Task 1: 消除收起延迟

**Files:** `src/app.h`、`src/dock.cpp`。

- [x] 删除 leaveAt 状态和 HoverTimer 的位置检测，仅保留 CopyFeedbackTimer 用于复制反馈。
- [x] 新增 Dock::trackMouseLeave 和 Dock::collapseIfOutside；后者供菜单结束后使用，检查 expanded、menuOpen、capturing、GetCursorPos、GetWindowRect 和 PtInRect，鼠标在外时立即 expand(false)，仍悬停在真实边栏 HWND 上时重新注册离开跟踪。
- [x] WM_MOUSEMOVE 在展开前请求离开跟踪；WM_MOUSELEAVE 在非菜单 / 截图期间立即 expand(false)。showMenu 执行菜单命令后调用 collapseIfOutside，确保选菜单期间不意外缩回，菜单关闭后无需等待。

文件：E:/CPPprj/old_prj_support/ScreenDock/src/dock.cpp；函数：Dock::collapseIfOutside

```cpp
if(!expanded || menuOpen || app_.capturing)return;
POINT point{};RECT rectangle{};
if(!GetCursorPos(&point) || !GetWindowRect(hwnd,&rectangle))return;
if(!PtInRect(&rectangle,point))expand(false);
else if(WindowFromPoint(point)==hwnd)trackMouseLeave();
```

### Task 2: 后台回归验证

**Files:** 新建 `tests/dock_background_test.cpp`；修改 `src/main.cpp`、`src/app.h`、`CMakeLists.txt`。

- [x] 新增 --dock-background-test 分支，在普通应用启动前直接调用独立测试函数；错误写报告并返回失败码，不弹窗。
- [x] 测试用 App::init(directory,true) 创建始终隐藏的窗口；不发送真实输入，不改变鼠标、不创建托盘、不注册热键。通过 SendMessage 对隐藏 HWND 验证首次进入展开、离开消息处理结束即收起、再次进入 / 离开、菜单及截图期间暂缓、菜单结束立即收起、菜单关闭时鼠标仍在区域内则保持展开。隐藏 HWND 不接受真实悬停，TME_LEAVE 的实际系统投递只作源码检查，不作前台测试。
- [x] 使用隐藏窗口位置匹配当前鼠标的内外状态，不移动真实鼠标；断言窗口始终不可见。新测试归入 background，仅运行 `build.ps1 -BackgroundTest`，保留既有 25 项自启动检查。

### Task 3: 版本和交付

**Files:** CMakeLists、manifest、README、CHANGELOG、validation、dist 和便携 ZIP。

- [x] 升级 1.2.1，README 去除延迟收起描述，说明立即收起及菜单操作保护。
- [x] Release 构建、后台 CTest、diff 检查及 ZIP 内容检查通过；不启动新版，不执行前台或剪贴板套件。
- [ ] 只提交本次相关文件，保留用户解压的未跟踪 ScreenDock-portable 文件夹；保留 Codex co-author 并推送 v1.2.1 标签。
