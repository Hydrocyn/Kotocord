# KotoCord

面向 VRChat 无言势及 VTuber 的综合性直播辅助工具。集成语音识别（STT）、大语言模型（LLM）情绪分析，将语音/文本输入转化为带有情绪色彩的艺术字字幕，实时驱动虚拟形象的表情，增强直播互动性和表现力。

## 功能

- **双引擎语音识别** — Vosk（轻量离线流式）+ Whisper.cpp（高精度离线），可运行时切换
- **LLM 情绪分析** — 接入 DeepSeek API（或兼容 OpenAI 接口的本地模型），自动识别情绪并附加颜文字
- **可视化字幕** — 带情绪标签的艺术字渲染，支持队列调度与视觉锁机制
- **系统资源监控** — CPU / 内存实时面板，辅助调试和性能调优
- **双模式构建** — 同一份代码，vcpkg 全自动 或 手动 Qt 均可构建。详见 [BUILD.md](BUILD.md)

## 架构

```
麦克风 → AudioCapture → [VoskTranscriber / WhisperTranscriber]
                                    ↓
                            AppController (队列 + 视觉锁)
                                    ↓
                 [MockLLMWorker / DeepSeekAPIWorker]
                                    ↓
                      SubtitleFrame (情绪 + 文本 + 颜文字)
                                    ↓
                              MainWindow → 屏幕渲染
```

### 模块

| 目录 | 职责 |
|---|---|
| `src/core/` | `AppController` 核心调度、`DataTypes.h` 公用类型 |
| `src/ui/` | Qt 主窗口 (`.ui` 设计师文件) |
| `src/modules/input/` | ASR 抽象层 (`IAudioTranscriber`) + Vosk/Whisper 实现 |
| `src/modules/capture/` | 音频采集 (`AudioCapture` 麦克风 / `AudioFileSimulator` 调试) |
| `src/modules/llm/` | LLM 抽象层 + DeepSeek API / Mock + `KaomojiManager` 颜文字 |
| `src/modules/render/` | 字幕渲染 (`SubtitleRenderer`) |
| `src/modules/system/` | 系统资源监控 (`SystemResourceMonitor`) |
| `src/utils/` | `AppPaths` 统一资源路径 |

### 依赖

每个库 **独立可选** vcpkg 或手动管理:

| 依赖 | vcpkg 选项 | 手动选项 |
|---|---|---|
| Qt 6 (Widgets, Multimedia) | `USE_VCPKG_QT=ON` | `USE_VCPKG_QT=OFF` + 官方安装器 |
| whisper.cpp + ggml | `USE_VCPKG=ON` | `USE_VCPKG=OFF` + `third_party/whisper/` |
| Vosk API | — | `third_party/vosk/` (始终手动) |
| DeepSeek API | — | 纯网络, 无本地依赖 |

## 快速开始

```powershell
# 设环境变量 (每台机器一次)
setx VCPKG_ROOT "C:\vcpkg"     # 如果用 vcpkg
setx Qt6_DIR    "C:\Qt\..."    # 如果用官方 Qt (路径改为你自己的)

# 构建 — 选一个 Preset
cmake --preset msvc-debug-all-vcpkg                  # 全部 vcpkg
cmake --preset msvc-debug-all-manual                 # 全部手动
cmake --preset msvc-debug-qt-manual-whisper-vcpkg    # 官方 Qt + vcpkg whisper
cmake --build build/msvc-debug-all-vcpkg --config Debug
```

> 完整指南见 **[BUILD.md](BUILD.md)**，IDE 配置实录见 **[DEV_GUIDE.md](DEV_GUIDE.md)**。

## 项目结构

```
Kotocord/
├── CMakeLists.txt                     # 双模式构建脚本
├── CMakePresets.json.example          # Preset 模板
├── check-deps.ps1                     # 依赖扫描 (check only)
├── CLAUDE.md                          # AI 协作约定
├── BUILD.md                           # 构建与部署完整指南
├── DEV_GUIDE.md                       # IDE 配置实践记录
├── README.md                          # 本文件
│
├── src/                               # 源码
├── tests/                             # 单元测试 (Qt Test)
├── third_party/vosk/                  # Vosk SDK (手动下载)
├── resources/                         # 颜文字 + 模型文件 (模型不提交)
├── build/                             # CMake 中间产物 (不提交)
└── bin/                               # 可执行文件输出 (不提交)
```

## 非 Git 追踪的资源

以下文件因体积过大或版权原因不纳入版本控制，需自行下载：

| 资源 | 下载地址 | 放置位置 |
|---|---|---|
| Vosk 运行时 DLL | [vosk-win64-0.3.45.zip](https://github.com/alphacep/vosk-api/releases) | `third_party/vosk/lib/` |
| Vosk 中文模型 | [vosk-model-small-cn-0.22.zip](https://alphacephei.com/vosk/models) | `resources/model/vosk-model-small-cn-0.22/` |
| Whisper 模型 | [ggml-small.bin](https://huggingface.co/ggerganov/whisper.cpp) | `resources/model/` |
| API Key | 本地创建 | `apikey.txt`（项目根目录） |

> 运行 `.\check-deps.ps1` 可扫描以上资源的就绪状态。完整下载和配置指南见 [BUILD.md](BUILD.md)。

## 致谢

| 项目 | 用途 | 协议 |
|---|---|---|
| [Qt 6](https://www.qt.io/) | UI 框架与多媒体 | LGPL v3 |
| [Vosk API](https://alphacephei.com/vosk/) | 离线流式语音识别 | Apache 2.0 |
| [Whisper.cpp](https://github.com/ggml-org/whisper.cpp) | 高精度语音识别 | MIT |
| [vcpkg](https://vcpkg.io/) | C/C++ 包管理器 | MIT |
