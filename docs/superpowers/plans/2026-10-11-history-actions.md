# ScreenDock v1.1.0 Implementation Plan

**Goal:** 侧边历史支持只移除软件索引、复制 PNG 文件，并发布带 Git 标签的版本。

**Architecture:** 每张历史卡片增加两个独立按钮；移除记录写入数据目录的 UTF-8 排除索引，扫描历史时过滤，截图 PNG 始终保留。文件复制发布 Unicode CF_HDROP 与 Preferred DropEffect=COPY，供资源管理器粘贴。

**Tech Stack:** C++17、Win32、Direct2D、MSVC、CMake，沿用原生实现。

## 文件与执行清单

- [x] 新增 `src/history_index.h/.cpp`：`HistoryIndex::load/hide/restoreAll/contains` 持久化相对文件名；提交新索引失败时保持内存列表不变。
- [x] 修改 `src/core.h/.cpp`：`fileDropData` 生成绝对 UTF-16 文件路径列表；`copyFile` 同时写入 CF_HDROP 与复制操作标记，拒绝不存在的文件，不修改源文件。
- [x] 修改 `src/app.h/.cpp`：启动读取排除索引，`hideRecord` 只移除记录，已有便签继续显示；`restoreHistory` 恢复索引；`copyRecordFile` 对保存中和缺失的原图明确报错。
- [x] 修改 `src/dock.cpp`：每张卡片展示“复制文件”和“删除记录”，命中按钮优先于置顶；增加“恢复已移出的记录”菜单；适配上下边栏，复制成功短暂显示“已复制”。
- [x] 修改 `src/pin.cpp`：右键菜单也提供“复制文件”。
- [x] 扩展 `tests/self_test.cpp`：索引跨重启持久化、原图像素不变、恢复、保存过程中删除、持久化失败、真实按钮点击、Unicode 文件复制、复制标志及原文件保留。文件剪贴板验证使用独立子进程，先备份现有剪贴板格式和内容，测试后恢复并核对；备份失败时中止测试。
- [x] 修改 `CMakeLists.txt`、manifest，新增版本资源：升级为 1.1.0 并在 EXE 属性和边栏展示版本。
- [x] 更新 README、CHANGELOG、build.ps1 和验证记录；构建脚本生成最新版便携包。
- [x] 正常关闭运行中的旧程序，完成 Release 构建、CTest 和新增按钮视觉检查，再启动新版。
- [ ] 新提交保留 `Co-authored-by: Codex <codex@openai.com>`；给原提交添加 v1.0.0、新提交添加 v1.1.0，普通推送 main 和两个标签；核对远端提交及标签。

## 验证入口

`cmake -S . -B build -G "Visual Studio 17 2022" -A x64`、`cmake --build build --config Release` 和 `ctest --test-dir build -C Release --output-on-failure` 均应返回 0。逐项记录写入 `tests/output/report.txt`；实际窗口预览由测试生成，使用 view_image 检查横向和纵向卡片操作按钮是否完整。

对“删除”只允许改动软件索引，测试验证原始 PNG 的像素内容仍完整。发布时用 `git diff --check` 和远端 refs 对照验证提交与版本标签。

本版本构建脚本验证通过；77 项主检查及 9 项剪贴板检查通过，便携包仅包含 EXE、README 和 CHANGELOG。最终发布项以远端 main 与版本标签核对结果为准。
