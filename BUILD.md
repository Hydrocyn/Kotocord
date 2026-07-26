# 构建与部署指南

## 目录

- [设备间迁移](#设备间迁移)
- [环境变量](#环境变量)
- [两种构建模式](#两种构建模式)
- [前置条件 (两种模式共用)](#前置条件)
- [模式 A：vcpkg 全自动管理](#模式-avcpkg-全自动管理)
- [模式 B：手动 Qt + third_party](#模式-b手动-qt--third_party)
- [CMake 预设参考](#cmake-预设参考)
- [构建产物](#构建产物)
- [分发给他人 (打包)](#分发给他人)
- [vcpkg 编译提速](#vcpkg-编译提速)
- [跨平台](#跨平台)
- [常见问题](#常见问题)

---

## 设备间迁移

git 追踪所有源码和配置文件。在不同设备上使用本项目：

```powershell
# 方式一：直接拷贝整个仓库文件夹（含 .git 目录）
# → 所有源码即刻就位，git 历史完整保留

# 方式二：从远程克隆
git clone <repo-url>
cd Kotocord
```

克隆/拷贝后，需手动下载**不被 git 跟踪的大文件**（DLL、模型文件）。详情见各构建场景的步骤，或运行 `.\check-deps.ps1` 扫描缺失项。

> 为什么不用打包脚本？git 仓库本身就是一个完整的"源码包"。直接拷贝或 clone 即可。体积大的第三方二进制文件和模型文件因版权和空间原因不纳入 git——按需下载即可。

---

## 环境变量

本项目 **不在配置文件中存储任何机器相关的绝对路径**。所有外部工具路径通过环境变量注入：

| 变量 | 用途 | 示例 |
|---|---|---|
| `VCPKG_ROOT` | vcpkg 安装目录 | `C:\vcpkg` |
| `Qt6_DIR` | Qt6 MSVC 安装目录（仅手动模式） | `C:\Qt\6.5.0\msvc2019_64` |

### 设置方法 (Windows)

**永久 (推荐)：**  以管理员身份打开终端，每台机器执行一次：

```powershell
# vcpkg 模式用户 (新电脑)
setx VCPKG_ROOT "C:\vcpkg"

# 手动模式用户 (旧电脑)
setx Qt6_DIR "H:\Software\Qt\Qt6.9.3\6.9.3\msvc2022_64"
```

> 路径改为你自己的安装位置。`setx` 设置后需**重新打开终端**才能在新窗口中生效。

**临时 (仅当前终端)：**

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
$env:Qt6_DIR    = "C:\Qt\6.5.0\msvc2019_64"
```

> 如果你在 VS Code 或 Qt Creator 的集成终端中设置，该设置仅在该 IDE 会话中有效。

### 验证

```powershell
# 全部 vcpkg
echo $env:VCPKG_ROOT
cmake --preset msvc-debug-all-vcpkg

# 全部手动
echo $env:Qt6_DIR
cmake --preset msvc-debug-all-manual

# 混合: 官方 Qt + vcpkg 的 whisper
echo $env:VCPKG_ROOT; echo $env:Qt6_DIR
cmake --preset msvc-debug-qt-manual-whisper-vcpkg
```

---

## 四种组合

项目的 CMakeLists.txt 提供 **两个独立选项**, 不捆绑任何一个库:

| `USE_VCPKG_QT` | `USE_VCPKG` | 场景 |
|---|---|---|
| ON | ON | 全新 PC, 全部 vcpkg |
| OFF | OFF | 旧 PC, 官方 Qt + third_party 手动库 |
| ON | OFF | vcpkg 的 Qt, 但 whisper 用本地自编译版本 |
| OFF | ON | 官方 Qt, 只把 whisper 交给 vcpkg |

> 需要 vcpkg toolchain 的预设必须设 `toolchainFile`。纯手动预设不需要 vcpkg 存在。
> 同时使用 vcpkg 和官方 Qt 时, `CMAKE_PREFIX_PATH` 中官方 Qt 路径会优先于 vcpkg 的 Qt。

---

## 前置条件

- **Git** + **CMake 3.16+**
- **编译器**：Visual Studio 2022 (含 "使用 C++ 的桌面开发") 或 MinGW-w64
- **Vosk 预编译库** — `third_party/vosk/lib/` 下需有运行时 DLL 和导入库
- **语音模型文件**：见 [模型文件准备](#a5-下载模型文件)

---

## 场景 A：全部 vcpkg (Qt=vcpkg, Whisper=vcpkg)

### A1. 安装 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

### A2. 设置环境变量

```powershell
setx VCPKG_ROOT "C:\vcpkg"
# 重新打开终端使其生效
```

### A3. 安装项目依赖 (仅首次，约 1–3 小时)

```powershell
cd $env:VCPKG_ROOT
.\vcpkg install qtbase[windeployqt] qtmultimedia[widgets] whisper-cpp
```

### A4. 准备 Vosk

Vosk 不在 vcpkg 中，需手动下载。

1. 从 [Vosk Releases](https://github.com/alphacep/vosk-api/releases) 下载 `vosk-win64-0.3.45.zip`
2. 解压后，将以下文件放入 `third_party/vosk/lib/`：

| 文件 | 说明 |
|---|---|
| `libvosk.dll` | Vosk 运行时 |
| `libgcc_s_seh-1.dll` | MinGW GCC 运行时 |
| `libstdc++-6.dll` | MinGW C++ 标准库 |
| `libwinpthread-1.dll` | MinGW POSIX 线程库 |

3. **生成 MSVC 导入库**（Vosk 是 MinGW 编译的，自带 `.lib` 不能用于 MSVC 链接器）：

```powershell
# 创建 DEF 文件
@"
EXPORTS
vosk_model_new
vosk_model_free
vosk_model_find_word
vosk_recognizer_new
vosk_recognizer_new_grm
vosk_recognizer_new_spk
vosk_recognizer_free
vosk_recognizer_set_max_alternatives
vosk_recognizer_set_words
vosk_recognizer_set_partial_words
vosk_recognizer_set_grm
vosk_recognizer_set_spk_model
vosk_recognizer_accept_waveform
vosk_recognizer_result
vosk_recognizer_partial_result
vosk_recognizer_final_result
vosk_recognizer_reset
vosk_set_log_level
vosk_gpu_init
vosk_gpu_thread_init
vosk_free
"@ | Out-File -FilePath "third_party\vosk\lib\libvosk.def" -Encoding ascii

# 用 MSVC lib.exe 生成
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe" `
    /def:third_party\vosk\lib\libvosk.def /machine:x64 /out:third_party\vosk\lib\vosk.lib
```

> **MinGW 用户**：使用 MinGW 编译器时可直接用 zip 中的 `libvosk.lib`，不需要上述步骤。

### A5. 构建

```powershell
cmake --preset msvc-debug-all-vcpkg
cmake --build build/msvc-debug-all-vcpkg --config Debug
```

### A6. 下载模型文件 (运行时需要)

- **Vosk 模型**：[vosk-model-small-cn-0.22.zip](https://alphacephei.com/vosk/models) → 解压到 `resources/model/vosk-model-small-cn-0.22/`
- **Whisper 模型**：[ggml-small.bin](https://huggingface.co/ggerganov/whisper.cpp) → 放到 `resources/model/`

```

---

## 场景 B：全部手动 (Qt=手动, Whisper=手动)

不依赖 vcpkg。适合已有 Qt 安装的旧电脑。

### B1. 目录结构要求

在 `third_party/` 下手动准备：

```
third_party/
├── vosk/
│   ├── include/vosk_api.h              ← 提交
│   └── lib/                            ← 不提交
│       ├── libvosk.dll
│       ├── libgcc_s_seh-1.dll          (MinGW 运行时)
│       ├── libstdc++-6.dll
│       ├── libwinpthread-1.dll
│       └── vosk.lib                    (MSVC) 或 libvosk.lib (MinGW)
│
└── whisper/                            ← 不提交
    ├── include/
    │   └── whisper.h
    ├── lib/
    │   ├── whisper.dll
    │   └── whisper.lib
    └── ggml/
        ├── include/
        │   └── ggml.h ...              (全部 ggml 头文件)
        └── lib/
            ├── ggml.dll                (运行时 DLL, 必须)
            ├── ggml-base.dll
            ├── ggml-cpu.dll
            └── ggml.lib                (可选 — 有些包把 ggml 编入了 whisper.lib)
```

> **关于 ggml.lib**：部分 whisper.cpp 预编译包不提供单独的 ggml 导入库——ggml 符号已编入 `whisper.lib`。CMakeLists.txt 会自动检测：找到独立 ggml 就链接，找不到就假定它在 whisper.lib 里。

### B2. 安装 Qt6

通过 [Qt 官方安装器](https://www.qt.io/download) 安装。勾选 **MSVC 或 MinGW** 版本 + **Multimedia** 模块。记下安装路径。

### B3. 设置环境变量

```powershell
# Qt MSVC 版:
setx Qt6_DIR "H:\Software\Qt\Qt6.9.3\6.9.3\msvc2022_64"

# Qt MinGW 版:
setx Qt6_DIR "C:\Qt\6.5.0\mingw_64"
```

### B4. 构建

```powershell
cmake --preset msvc-debug-all-manual
cmake --build build/msvc-debug-all-manual --config Debug
```

---

## CMake 预设参考

| 预设名 | Qt | 其他库 | 编译器 |
|---|---|---|---|
| `msvc-debug-all-vcpkg` | vcpkg | vcpkg | MSVC |
| `msvc-release-all-vcpkg` | vcpkg | vcpkg | MSVC |
| `msvc-debug-all-manual` | 手动 | 手动 | MSVC |
| `msvc-release-all-manual` | 手动 | 手动 | MSVC |
| `msvc-debug-qt-manual-whisper-vcpkg` | 手动 | vcpkg | MSVC |

> 用户可根据需要自定义组合——只需在本地 `CMakePresets.json` 中复制一个预设，修改 `USE_VCPKG_QT` 和 `USE_VCPKG` 的值即可。

### 日常命令

```powershell
# 首次 configure
cmake --preset <预设名>

# 增量构建
cmake --build build/<目录> --config Debug

# 清理重编译
cmake --build build/<目录> --config Debug --clean-first

# 完全重置
Remove-Item -Recurse -Force build\<目录>
cmake --preset <预设名>
cmake --build build/<目录> --config Debug
```

---

## 构建产物

```
bin/
├── Debug/
│   ├── Kotocord.exe
│   ├── *.dll                       ← Qt/whisper/ggml/Vosk 自动部署
│   ├── platforms/qwindowsd.dll      ← Qt 平台插件 (必须)
│   ├── styles/                     ← Windows 原生风格
│   ├── imageformats/               ← JPEG/GIF/SVG 支持
│   ├── tls/                        ← HTTPS (DeepSeek API 需要)
│   └── multimedia/                 ← 音频采集插件
└── Release/
    └── ...
```

---

## 分发给他人

构建 Release 后，`bin/Release/` 目录即为完整运行包。需同时打包：

```
Kotocord-v0.1.0/
├── Kotocord.exe
├── *.dll
├── platforms/qwindows.dll
├── styles/
├── imageformats/
├── tls/
├── multimedia/
└── resources/
    ├── kaomoji.json
    └── model/
        ├── vosk-model-small-cn-0.22/
        └── ggml-small.bin
```

**发布前检查：**
- [ ] exe 同级有 `platforms/qwindows.dll`
- [ ] `resources/model/` 下有模型文件
- [ ] 在未安装 Qt 的裸机上测试过双击运行

---

## vcpkg 编译提速

| 策略 | 效果 |
|---|---|
| **二进制缓存** | 备份 `C:\vcpkg\packages\` 和 `C:\vcpkg\archives\`，重装后放回 |
| **只装需要的模块** | 本项目已采用 (`qtbase[widgets]` 而非全量 `qt6`) |
| **远程缓存** | NuGet/S3 团队共享，见 [Binary Caching](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching) |

---

## 跨平台

CMakeLists.txt 已预留 macOS/Linux 分支。vcpkg 在 macOS/Linux 上使用方式相同：

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase qtmultimedia[widgets] whisper-cpp
```

---

## 常见问题

### Q: "no Qt platform plugin" 错误

`platforms/qwindows.dll` 缺失。vcpkg 模式下构建自动部署；手动模式需运行一次 `windeployqt`。

### Q: "Could not find GGML_LIB" (旧电脑)

ggml 的 `.lib` 缺失——某些预编译包把 ggml 编入了 whisper.lib。CMakeLists.txt 会自动降级：找不到独立 ggml 就假定它在 whisper.lib 里。如果仍有问题，运行 `dir /s third_party\whisper\*.lib` 检查。

### Q: 如何实现 "vcpkg 的 Qt + 本地自编译 whisper"？

复制 `msvc-debug-all-vcpkg` 预设，将 `USE_VCPKG` 改为 `OFF`。whisper 会自动回退到 `third_party/whisper/` 加载。CMakeLists.txt 中 whisper 的发现逻辑独立于 Qt。

### Q: "Vosk library not found"

`third_party/vosk/lib/` 下缺少 `.lib`。MSVC 需用 `lib.exe` 生成（见模式 A 步骤 3）。

### Q: 运行时 "无法加载 Vosk 模型" / "Whisper 模型未正确初始化"

`resources/model/` 下缺少模型文件。检查 `src/utils/AppPaths.h` 中的路径。

### Q: vcpkg 的 Qt 和官方 Qt 可以共存吗？

可以。它们安装在不同目录，互不干扰。Kit/预设决定了用哪个。

### Q: `CMakePresets.json` 修改后 git 不追踪了？

这是设计如此。`.gitignore` 已忽略 `CMakePresets.json`。每人从 `CMakePresets.json.example` 复制后自行修改。
