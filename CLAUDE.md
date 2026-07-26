# CLAUDE.md — Kotocord AI 协作约定

## Git 操作

- **所有 git 命令由用户本人执行。** AI 只提供命令和预期反馈，不执行 `git add`、`git commit`、`git push`、`git rebase` 等任何 git 操作。
- AI 可以提供建议性的 git 命令，描述命令的作用和预期结果，但绝不在终端中实际运行它们。
- **不允许将 Claude 或任何 AI agent 列为 git 仓库的协作者（contributor/co-author）。** 所有 commit 的作者和贡献者只能是真实人类。

## 项目信息

- **项目名称**：Kotocord — 面向 VRChat 无言势及 VTuber 的直播辅助字幕渲染工具
- **语言**：C++17
- **UI 框架**：Qt 6 (Widgets + Multimedia)
- **构建系统**：CMake 3.16+ + MSVC 2022
- **包管理**：vcpkg（可选）+ 手动 third_party（可选），双模式可自由组合
- **测试框架**：Qt Test

## 构建

```powershell
# 配置
cmake --preset msvc-debug-all-vcpkg

# 构建
cmake --build build/msvc-debug-all-vcpkg --config Debug
```

完整构建指南见 [BUILD.md](BUILD.md)，IDE 配置见 [DEV_GUIDE.md](DEV_GUIDE.md)。

AI 不自动执行构建命令。当用户请求构建时，提供命令供用户手动执行。

## 代码风格

- C++17 标准，使用现代 C++ 特性（智能指针、移动语义、模板）
- 遵循 C++ Core Guidelines（RAII、接口隔离、编译期类型检查）
- Qt 信号槽用于模块间通信
- 抽象接口（纯虚类）用于模块解耦
- 工厂模式用于运行时组件选择和依赖注入

## 模块架构

```
main.cpp → AppController（核心调度）
              ├── AudioCapture / AudioFileSimulator（音频采集）
              ├── VoskTranscriber / WhisperTranscriber（语音识别，IAudioTranscriber 接口）
              ├── MockLLMWorker / DeepSeekAPIWorker（LLM，ILanguageModel 接口）
              ├── KaomojiManager（颜文字管理）
              ├── SubtitleRenderer（字幕渲染）
              ├── SystemResourceMonitor（系统资源监控）
              └── MainWindow（Qt UI）
```

## 已知测试陷阱

- Qt Test 中 `QSignalSpy` 对自定义类型（如 `SubtitleFrame`）不稳定，改用 lambda 计数器
- 测试间状态泄漏：需在 `init()` 中调用 `resetState()`
- 需要 `QT_ASSUME_STDERR_HAS_CONSOLE=1` 以在 Windows 控制台输出测试日志
