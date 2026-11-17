# ScreenDock Autostart Implementation Plan

> **For agentic workers:** Execute this plan inline. The user authorizes implementation and explicitly prohibits foreground testing; do not run the existing visual/clipboard self-test or launch the normal application.

**Goal:** 增加默认关闭、当前用户可选的开机自启动设置，并在完全后台验证后交付版本 1.2.0。

**Architecture:** 用 HKCU 的 Run 注册表值作为自启动状态的唯一来源，不在 settings.ini 保存副本。勾选时注册当前 EXE、实际数据目录和 --startup 参数，取消时仅删除 ScreenDock 自己的值；读取与取消均不创建注册表键。菜单每次打开读取状态。--startup 启动保持边栏收起，已有实例不展开。测试使用独立控制台测试程序和隔离注册表子键，不创建 HWND、不接触剪贴板、不操作用户真实 Run 项。

**Tech Stack:** C++17、Win32 Registry API、MSVC、CMake / CTest、PowerShell。

---

### Task 1: 自启动注册模块与后台测试

**Files:** 新建 `src/startup.h`、`src/startup.cpp`、`tests/startup_test.cpp`；修改 `CMakeLists.txt`。

- [x] 实现以下接口：生产默认 HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run 的 ScreenDock 值，测试显式传入隔离根句柄与 Run 子键。

文件：E:/CPPprj/old_prj_support/ScreenDock/src/startup.h；位置：sd::StartupRegistration 接口

```cpp
std::wstring startupCommand(const std::filesystem::path& executable,
                           const std::filesystem::path& dataDirectory);
class StartupRegistration {
public:
    explicit StartupRegistration(HKEY root=HKEY_CURRENT_USER,
        std::wstring key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    LSTATUS query(bool& enabled) const;
    LSTATUS setEnabled(bool enabled,const std::wstring& command) const;
};
```

- [x] startupCommand 对路径作 Windows 参数引号转义，包括尾部反斜杠；使用 `"EXE" --startup --data-dir "目录"`，避免登录时当前工作目录改变数据位置。
- [x] query 对键或值不存在返回成功且关闭；格式错误或无法访问返回错误。setEnabled(true) 限制命令为有效 REG_SZ，超过 Run 的 260 字符限制时拒绝；setEnabled(false) 缺失时幂等成功，只删除自己的值，保留其他项。
- [x] 后台测试通过 CommandLineToArgvW 解析实际命令，验证中文 / 空格 / 尾部反斜杠；验证默认关闭、开启 / 重读 / 更新 / 关闭 / 重复关闭、其他值保持完整、格式错误与无效句柄错误。测试临时注册表树使用 PID 和 tick 组成唯一名称，退出仅清理这棵树。
- [x] 新建 ScreenDockStartupTests 控制台目标，链接 advapi32 和 shell32。CTest 标记 background；原有 ScreenDockSelfTest 标记 foreground。

### Task 2: 菜单与登录启动行为

**Files:** 修改 `src/app.h`、`src/app.cpp`、`src/dock.cpp`、`src/main.cpp`。

- [x] App 增加 StartupRegistration、queryStartup 和 toggleStartup。只在用户点击时写注册表，成功后反馈启用 / 关闭；失败提示并保持实际状态。EXE 路径通过 GetModuleFileNameW 获取，目录为 App::data 的绝对路径。
- [x] Dock::showMenu 新增命令 34：“开机自启动（当前用户）”，依据 queryStartup 显示勾选；读取失败时禁用。托盘复用同一菜单。
- [x] wWinMain 接受 --startup；已有实例时仅手动启动才展开边栏。首次登录启动沿用默认收起状态，不添加自动恢复便签。

### Task 3: 构建、打包与交付

**Files:** 修改 `build.ps1`、`CMakeLists.txt`、`src/app.manifest`、README、CHANGELOG、`docs/validation.md`，更新 dist 和便携 ZIP。

- [x] build.ps1 添加 BackgroundTest 开关，与 Test 互斥，仅运行 `ctest --test-dir build -C Release -L background --no-tests=error --output-on-failure`；Test 保持完整验证用途。
- [x] 版本升至 1.2.0，文档说明默认关闭、菜单位置、只针对当前用户、注销 / 登录生效、移动便携目录后取消再勾选刷新路径，以及 Windows 任务管理器禁用与系统策略仍可能阻止启动。
- [x] 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -BackgroundTest`，Release 构建成功且仅后台测试通过（25 项检查，0 个失败）。未运行无过滤的 CTest、--self-test、--clipboard-test、正常程序或前台检查。
- [x] 检查 ZIP 只含 EXE、README、CHANGELOG，核对版本资源、git diff --check 和修改范围。测试结果如实说明未做登录重启和前台验收。
- [ ] 沿用项目版本控制方式提交，保留 Codex co-author，添加 v1.2.0 标签并普通推送；核对远端提交及标签。不要启动新版或改动实际自启动状态。
