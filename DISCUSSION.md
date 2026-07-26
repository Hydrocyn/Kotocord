# Kotocord 项目设计与架构分析报告

> **作者**: Hydrocyn  
> **日期**: 2026-07-12  
> **版本**: v0.1.0  
> **许可**: MIT  
> **本文目的**: 向他人清晰阐述 Kotocord 的架构设计思想、编程范式和工程实践，同时也作为自我复盘的技术笔记。文末预留了讨论区，欢迎针对具体设计决策提出疑问和探讨。

---

## 目录

- [1. 项目概述](#1-项目概述)
- [2. 高层架构总览](#2-高层架构总览)
- [3. 设计范式与模式分析](#3-设计范式与模式分析)
- [4. 模块深度剖析](#4-模块深度剖析)
  - [4.1 核心调度层 (`src/core/`)](#41-核心调度层-srccore)
  - [4.2 语音采集层 (`src/modules/capture/`)](#42-语音采集层-srcmodulescapture)
  - [4.3 语音识别层 (`src/modules/input/`)](#43-语音识别层-srcmodulesinput)
  - [4.4 大模型层 (`src/modules/llm/`)](#44-大模型层-srcmodulesllm)
  - [4.5 渲染层 (`src/modules/render/`)](#45-渲染层-srcmodulesrender)
  - [4.6 系统监控层 (`src/modules/system/`)](#46-系统监控层-srcmodulessystem)
  - [4.7 UI 层 (`src/ui/`)](#47-ui-层-srcui)
  - [4.8 工具层 (`src/utils/`)](#48-工具层-srcutils)
- [5. 构建系统设计](#5-构建系统设计)
- [6. 关键设计决策与权衡](#6-关键设计决策与权衡)
- [7. 代码质量观察](#7-代码质量观察)
- [8. 演进方向与待讨论议题](#8-演进方向与待讨论议题)

---

## 1. 项目概述

### 1.1 Kotocord 是什么

Kotocord 是一个**面向 VRChat 无言势用户及 VTuber 的综合性直播辅助工具**。它的核心能力是将语音/文本输入转化为带有**情绪色彩的艺术字字幕**，并计划在未来通过 OSC 协议实时驱动虚拟形象的表情 Blendshapes。

用一句话概括：**"你说的话，经过 AI 理解情绪后，变成漂亮且有感情的字幕，浮在屏幕上供观众观看。"**

### 1.2 核心功能矩阵

| 功能 | 实现方式 | 技术选型理由 |
|---|---|---|
| 语音采集 | Qt Multimedia `QAudioSource` | 跨平台音频 API，免去 Windows WASAPI 手动适配 |
| 语音识别（轻量） | Vosk API（离线流式） | 低延迟、低资源占用，适合游戏双开场景 |
| 语音识别（高精度） | Whisper.cpp（离线） | 识别精度远超 Vosk，GGML 推理引擎无需 GPU |
| 情绪分析 | DeepSeek API（兼容 OpenAI 格式） | 结构化 JSON 输出，`response_format: json_object` |
| 颜文字映射 | JSON 词库 + 随机抽取 | 可扩展、可热更，Neutral 情绪返回空串 |
| 字幕渲染 | Qt `QPainterPath` + 无边框置顶透明窗口 | OBS 窗口捕获友好，无边框可拖拽 |
| 系统监控 | Win32 `GetProcessTimes` / `GetProcessMemoryInfo` | 实时 CPU/内存仪表盘 |
| 双模式构建 | CMake + `USE_VCPKG` 开关 | 一份代码同时适配"新 PC 全自动"和"旧 PC 手动" |

### 1.3 技术栈

```
语言:     C++17
UI框架:   Qt 6 (Widgets + Multimedia)
构建:     CMake 3.16+
包管理:   vcpkg (可选)
ASR:      Vosk API + Whisper.cpp (GGML)
LLM:      DeepSeek API (HTTP/JSON, 兼容 OpenAI 接口格式)
平台:     Windows (已预留 macOS/Linux 分支)
```

---

## 2. 高层架构总览

### 2.1 数据流管道

整个应用可以视为一条 **单向数据流管道**，每一层只关心自己的输入和输出：

```
┌──────────┐    ┌─────────────────┐    ┌──────────────────┐
│ 麦克风    │───▶│  AudioCapture   │───▶│ VoskTranscriber  │
│ (硬件)    │    │  (PCM 采集)     │    │ 或               │
└──────────┘    └─────────────────┘    │ WhisperTranscriber│
                                       └────────┬─────────┘
                                                │ textReady(text, isFinal)
                                                ▼
┌──────────┐    ┌─────────────────────────────────────────┐
│ 手动输入  │───▶│         AppController (核心调度)         │
│ (UI)     │    │  · 视觉锁 (Screen Lock)                  │
└──────────┘    │  · 句子队列 (Pending Queue)              │
                │  · 帧 ID 时序保护                        │
                └────────┬───────────────┬────────────────┘
                         │               │
                         ▼               ▼
              ┌──────────────────┐  ┌──────────────────┐
              │ MockLLMWorker /  │  │ KaomojiManager   │
              │ DeepSeekAPIWorker│  │ (颜文字词典)      │
              └────────┬─────────┘  └────────┬─────────┘
                       │                    │
                       ▼                    ▼
              ┌─────────────────────────────────────────┐
              │   SubtitleFrame (完整数据包)              │
              │   { rawText, displayText, emotion, ... } │
              └────────────────────┬────────────────────┘
                                   │ subtitleReadyForRender
                                   ▼
              ┌─────────────────────────────────────────┐
              │         MainWindow                      │
              │  ┌───────────────────────────────────┐  │
              │  │  SubtitleRenderer (透明浮层)       │  │
              │  │  · QPainterPath 智能排版          │  │
              │  │  · 三层绘制 (阴影/描边/渐变)       │  │
              │  │  · 无边框拖拽                      │  │
              │  └───────────────────────────────────┘  │
              └─────────────────────────────────────────┘
```

### 2.2 模块依赖图

```
                        ┌─────────────┐
                        │  main.cpp   │  ← 组装所有组件的"工厂"
                        └──────┬──────┘
                               │
          ┌────────────────────┼────────────────────┐
          ▼                    ▼                    ▼
   ┌──────────────┐   ┌──────────────┐    ┌────────────────┐
   │   src/core/  │   │   src/ui/    │    │  src/utils/    │
   │ AppController│◄──│ MainWindow   │    │  AppPaths      │
   │ DataTypes.h  │   │ SubtitleRndr │    └────────────────┘
   └──────┬───────┘   └──────┬───────┘
          │                  │
    ┌─────┴────────────┬─────┴────────────┬──────────────┐
    ▼                  ▼                  ▼              ▼
┌──────────┐   ┌──────────────┐   ┌──────────┐   ┌──────────┐
│ capture/ │   │   input/     │   │   llm/   │   │ system/  │
│AudioCap  │   │IAudioTranscr │   │ILanguage │   │SysResMon │
│AudioSim  │   │VoskTranscr   │   │Model     │   └──────────┘
└──────────┘   │WhisperTranscr│   │DeepSeek  │
               └──────────────┘   │MockLLM   │
                                  │KaomojiMgr│
                                  └──────────┘
```

关键依赖原则：
- **`src/core/`** 是唯一知道所有模块如何协作的地方（通过 `main.cpp` 的信号槽连线）
- **接口层**（`IAudioTranscriber`, `ILanguageModel`）隔离了具体实现，使模块间松耦合
- **`DataTypes.h`** 是全局共享的"数据类型"包，各模块不直接互相引用

---

## 3. 设计范式与模式分析

### 3.1 策略模式 (Strategy Pattern) — 双引擎切换

这是项目中最显著的设计模式应用，在两个维度上同时出现：

```
IAudioTranscriber (抽象策略)          ILanguageModel (抽象策略)
    ├── VoskTranscriber                  ├── MockLLMWorker
    └── WhisperTranscriber               └── DeepSeekAPIWorker
```

**运行时切换实现**（[main.cpp:64](src/main.cpp#L64)）：

```cpp
// 用一个基类指针追踪当前选中的引擎
IAudioTranscriber* currentASR = &voskEngine;

// 用户点击 UI 切换时，停掉旧引擎，换上新引擎
QObject::connect(&window, &MainWindow::asrEngineSwitched, [&](bool isWhisper) {
    currentASR->stop();
    currentASR = isWhisper
        ? static_cast<IAudioTranscriber*>(&whisperEngine)
        : static_cast<IAudioTranscriber*>(&voskEngine);
    if (isAsrEnabled) currentASR->start();
});
```

**设计评价**：干净利落。`static_cast` 在这里是安全的，因为每个具体类的实例都在栈上分配，切换时只改指针指向，没有堆分配开销。唯一可商榷的是两个引擎在 `main.cpp` 中同时构造——这意味着 Whisper 的几百 MB 模型在启动时就会被加载到内存，即使用户只使用 Vosk。这是一个"预加载 vs 懒加载"的经典权衡。

### 3.2 依赖注入 (Dependency Injection) — 控制反转

全项目没有任何 `new` 操作发生在业务逻辑类中（除了 UI 内部创建 `SubtitleRenderer`）。所有依赖在 `main()` 中构造，通过 setter 注入：

```cpp
// main.cpp — "组装车间"
AppController controller;
controller.setLanguageModel(&mockLLM);       // 注入 LLM
controller.setKaomojiManager(&kaomojiManager); // 注入颜文字管理器

MainWindow window(&controller);               // 通过构造函数注入控制器
```

[AppController::setLanguageModel](src/core/AppController.h#L19) 通过 setter 接收 `ILanguageModel*`，内部进行信号连接：

```cpp
void AppController::setLanguageModel(ILanguageModel* llm) {
    m_llm = llm;
    if (m_llm) {
        connect(m_llm, &ILanguageModel::textProcessed,
                this, &AppController::onLLMTextProcessed);
    }
}
```

**设计评价**：典型的纯手工 DI，没有使用任何框架。优点是极其透明——所有依赖关系在 `main.cpp` 中一目了然；缺点是随着模块增多，`main.cpp` 会越来越臃肿。对于当前 6 个核心模块的规模，手工 DI 完全够用。

### 3.3 接口隔离原则 (ISP) — 小而专注的抽象

项目中的接口都是 **极小** 的：

- [IAudioTranscriber](src/modules/input/IAudioTranscriber.h) — 仅 4 个纯虚方法 + 2 个信号
- [ILanguageModel](src/modules/llm/ILanguageModel.h) — 仅 1 个纯虚方法 + 3 个信号

不强迫实现类依赖它们不需要的方法。比如 `MockLLMWorker` 不需要 `performanceMetricsReported` 信号的具体实现——它只需继承基类信号即可，在合适时机 emit。

### 3.4 信号槽事件总线 — Qt 的元对象系统

这是项目架构的"神经系统"。各模块间完全通过 Qt 信号槽通信，没有任何模块直接调用另一个模块的方法（除了 setter 注入时）。

一个典型的跨模块通信链（[main.cpp:87-96](src/main.cpp#L87-L96)）：

```cpp
// 麦克风 → Vosk (音频数据)
connect(&micCapture, &AudioCapture::audioDataReady,
        &voskEngine,  &VoskTranscriber::onAudioDataReady);

// 麦克风 → Whisper (同一路音频，双投)
connect(&micCapture, &AudioCapture::audioDataReady,
        &whisperEngine, &WhisperTranscriber::onAudioDataReady);

// Vosk → 控制器 (识别文本)
connect(&voskEngine,    &IAudioTranscriber::textReady,
        &controller,    &AppController::onASRTextReady);
```

这种设计的优势：
1. **松耦合**：`AudioCapture` 不知道谁在消费音频数据，`IAudioTranscriber` 不知道谁在使用识别结果
2. **一对多广播**：同一路 PCM 数据同时喂给 Vosk 和 Whisper，实现"双引擎热备"
3. **线程安全**：Qt 的信号槽在跨线程时会自动将调用排队到接收者所在线程

### 3.5 基于值的不可变数据包 (Value Object)

[SubtitleFrame](src/core/DataTypes.h#L26-L43) 是整个系统的"数据货币"：

```cpp
struct SubtitleFrame {
    qint64 frameId;         // 时序保护：拒绝旧帧
    QString rawText;        // 原始文本（不变）
    QString displayText;    // 渲染文本（可叠加颜文字）
    EmotionType emotion;    // LLM 判定结果
    bool isFinal;           // 是否为最终结果（vs. ASR 中间结果）
    bool isLlmProcessed;    // 是否已过 LLM
    int tokenCost;          // Token 消耗
};
```

作为值类型通过信号槽传递（注册了 `Q_DECLARE_METATYPE`），每个接收者拿到的是独立的副本，不会出现数据竞争。`frameId` 单调递增，用于 [SubtitleRenderer 拒绝旧帧](src/modules/render/SubtitleRenderer.cpp#L27-L29)：

```cpp
void SubtitleRenderer::updateFrame(const SubtitleFrame& frame) {
    if (m_currentFrame.frameId > frame.frameId) return; // 丢弃旧结果
    // ...
}
```

---

## 4. 模块深度剖析

### 4.1 核心调度层 (`src/core/`)

#### 4.1.1 AppController — 视觉锁 + 句子队列

这是整个项目中**设计最精妙**的模块。它解决了一个核心矛盾：

> LLM API 调用有 1-3 秒延迟，但 ASR 在此期间还在持续产生新文本。如何防止前后文互相覆盖？

**解决方案：视觉锁 (Screen Lock) + 句子队列 (Pending Queue)**

状态机如下：

```
                        ┌──────────────────────────┐
                        │      NORMAL (未锁)        │
                        │  接收 ASR 文本 → 立刻上屏  │
                        └───────────┬──────────────┘
                                    │ 收到 isFinal=true
                                    ▼
                        ┌──────────────────────────┐
                        │      LOCKED (上锁)        │
                        │  · 丢弃 ASR partial 文本  │
                        │  · 完整句进 pendingQueue  │
                        │  · 等待 LLM 返回          │
                        └───────────┬──────────────┘
                                    │ LLM 返回 + 1.5s 定时器
                                    ▼
                        ┌──────────────────────────┐
                        │    TIMER FIRED (解锁)     │
                        │  · 检查队列是否为空        │
                        │  · 有 → 取队首，再次上锁   │
                        │  · 无 → 回到 NORMAL       │
                        └──────────────────────────┘
```

关键代码（[AppController.cpp:46-62](src/core/AppController.cpp#L46-L62)）：

```cpp
void AppController::onASRTextReady(const QString& text, bool isFinal) {
    if (m_isScreenLocked) {
        if (!isFinal) return;  // 锁屏时丢弃 ASR 的中间结果
        // 完整句子进队列（队列上限 2）
        if (m_pendingQueue.size() >= 2) {
            m_pendingQueue.dequeue(); // 队列拥堵，丢弃最旧的
        }
        m_pendingQueue.enqueue(frame);
        return;
    }
    // ... 正常流程
}
```

**设计评价**：
- 视觉锁的概念借鉴了图形学中的"垂直同步"思想——让显示跟上处理节奏
- 队列上限设为 2 是一种实用主义：字幕观众不需要看到积压的 10 句话，保留最近 2 句即可
- 解锁定时器设 1.5 秒既保证了阅读时间，又不会让对话节奏太拖沓
- **潜在隐患**：当前锁是 bool 而非引用计数——如果某种异常导致 LLM 不返回，锁永远不会释放。建议增加一个超时保底机制（如 10 秒强解）

#### 4.1.2 DataTypes.h — 统一的类型语言

把所有共享枚举和结构体集中在一个文件，避免了头文件循环依赖。`EmotionType` 枚举和 `stringToEmotion()` 工具函数支持中英文双语映射，这是直接对接 LLM 输出的务实选择——DeepSeek 有时返回 `"Joy"`，有时返回 `"高兴"`。

### 4.2 语音采集层 (`src/modules/capture/`)

#### 4.2.1 AudioCapture — 薄封装层

[AudioCapture](src/modules/capture/AudioCapture.cpp) 封装了 Qt Multimedia 的 `QAudioSource`，约 100 行代码，职责单一：
- 固定采样率 16kHz / 单声道 / 16-bit（Vosk 和 Whisper 的共同要求）
- 200ms 内部缓冲区（`(sampleRate * channels * bytesPerSample) / 5`）
- 错误时用 `deleteLater()` 而非裸 `delete`，防止信号回调期间的内存访问崩溃

#### 4.2.2 AudioFileSimulator — 调试利器

[AudioFileSimulator](src/modules/capture/AudioFileSimulator.cpp) 模拟麦克风输入，每 100ms 读一块 WAV 文件并发射与 `AudioCapture` 相同的 `audioDataReady` 信号。它和 `AudioCapture` 没有共享基类——因为它们通过**信号签名**而非继承来达成可替换性。

**设计评价**：这是"鸭子类型"在 C++ 信号槽系统中的自然应用。只要发射的信号签名一致，下游消费者就不关心数据来源。不过，如果未来需要统一的错误处理或多路音频源混合，可以考虑提取一个 `IAudioSource` 接口。

### 4.3 语音识别层 (`src/modules/input/`)

#### 4.3.1 VoskTranscriber — 流式识别的标杆实现

核心循环（[VoskTranscriber.cpp:55-76](src/modules/input/VoskTranscriber.cpp#L55-L76)）：

```cpp
void VoskTranscriber::onAudioDataReady(const QByteArray& data) {
    int isFinal = vosk_recognizer_accept_waveform(m_recognizer, data.constData(), data.size());
    if (isFinal) {
        parseAndEmitResult(vosk_recognizer_result(m_recognizer), true);  // 最终结果
    } else {
        parseAndEmitResult(vosk_recognizer_partial_result(m_recognizer), false); // 中间结果
    }
}
```

Vosk 的 C API 设计得很巧：`AcceptWaveform()` 返回 1 表示检测到句尾，此时 `Result()` 返回完整的句子；返回 0 时 `PartialResult()` 返回实时部分文字。Kotocord 充分利用了这个特性——中间结果立刻上屏给观众（`isFinal=false`），给 LLM 做情绪分析的是最终结果（`isFinal=true`）。

**设计评价**：Vosk 的流式设计天然适合直播场景。0.1-0.5 秒的实时中间结果让观众感觉"响应很快"，而最终结果的准确性又保证了字幕质量。

#### 4.3.2 WhisperTranscriber — VAD + 后台线程推理

这是项目中**复杂度最高**的模块。Whisper.cpp 无法像 Vosk 那样流式处理——它需要一段"完整的话"才能准确推理。解决方案：

1. **VAD (Voice Activity Detection)** 断句算法（[WhisperTranscriber.cpp:70-107](src/modules/input/WhisperTranscriber.cpp#L70-L107)）：
   - 16-bit PCM 振幅阈值 500（约为满量程的 1.5%，极其灵敏）
   - 连续静音超过 0.8 秒 → 触发推理
   - 状态机在 `m_isSpeaking` 和静音计时器之间流转

2. **后台推理线程**（[WhisperTranscriber.cpp:117-186](src/modules/input/WhisperTranscriber.cpp#L117-L186)）：
   - `std::atomic<bool> m_isInferencing` 防止重复触发
   - `QThread::create()` 将耗时的 CPU 推理移到后台，主线程不卡顿
   - `QMutexLocker` 保护音频缓冲区的读写

```cpp
m_inferenceThread = QThread::create([this, bufferCopy]() {
    // 在后台线程中执行 whisper_full()
    // 完成后 m_isInferencing.store(false) 解锁
});
connect(m_inferenceThread, &QThread::finished,
        m_inferenceThread, &QObject::deleteLater); // 自动清理
```

**设计评价**：
- 多线程模型清晰：主线程只做 VAD 检测和结果分发，Worker 线程只做推理
- `QThread::create` + `deleteLater` 模式是老练的 Qt 实践，避免了手动 `delete` 的悬挂指针风险
- 潜在改进点：当前如果在推理期间又进来新音频，会被静默丢弃（`m_isInferencing` 保护）。可以改为双缓冲——推理时新音频写入另一个 buffer，推理完成后无缝衔接

### 4.4 大模型层 (`src/modules/llm/`)

#### 4.4.1 ILanguageModel — 极简接口

```cpp
class ILanguageModel : public QObject {
    Q_OBJECT
public:
    virtual void processText(const SubtitleFrame& frame) = 0;
signals:
    void textProcessed(const SubtitleFrame& processedFrame);
    void performanceMetricsReported(int promptTokens, int completionTokens, qint64 latencyMs);
    void errorOccurred(const QString& errorMsg);
};
```

只有一个纯虚方法 `processText()`。信号的参数已经精心设计好——任何实现类只需要在合适时机 emit，上层监控 UI 就能工作。

#### 4.4.2 DeepSeekAPIWorker — 健壮的网络调用

设计亮点：

1. **5 秒超时自毁**（[DeepSeekAPIWorker.cpp:86-94](src/modules/llm/DeepSeekAPIWorker.cpp#L86-L94)）：
   ```cpp
   QTimer* timeoutTimer = new QTimer(reply); // 挂载在 reply 上自动销毁
   timeoutTimer->setSingleShot(true);
   timeoutTimer->start(5000);
   connect(timeoutTimer, &QTimer::timeout, reply, [=]() {
       if (reply->isRunning()) reply->abort();
   });
   ```
   定时器作为 `reply` 的子对象——当 `reply` 销毁时，定时器自动销毁，不存在内存泄漏。

2. **fallbackFrame 保底机制**：无论 API 失败、超时、JSON 解析失败，都保证有一个 `EmotionType::Neutral` 的帧通过 `textProcessed` 信号交还给控制器。这个设计保证了**即使 LLM 完全不可用，字幕流也不会中断**。

3. **系统 Prompt 工程**（[DeepSeekAPIWorker.cpp:28-35](src/modules/llm/DeepSeekAPIWorker.cpp#L28-L35)）：
   ```
   "绝对不要输出任何分析过程或多余文字。"
   "强制返回合法的 JSON 对象，格式必须为：{\"emotion\": \"Joy\"}"
   ```
   加上 `response_format: json_object` 参数，双重保险确保输出可解析。

4. **兼容性设计**（[DeepSeekAPIWorker.h:14-15](src/modules/llm/DeepSeekAPIWorker.h#L14-L15)）：
   ```cpp
   // 如果你想用本地的 Ollama，只需改成
   // "http://localhost:11434/v1/chat/completions" 和对应的本地模型名即可！
   ```
   代码注释直接告诉使用者如何切换到本地模型。API URL、模型名都是可配置的，遵循 OpenAI 兼容格式。

**设计评价**：这个模块展示了"防御性编程"的典范。每一层都有fail-safe：API Key 缺失 → 返回 Neutral；网络超时 → 返回 Neutral；JSON 解析失败 → 返回 Neutral。最坏情况下，字幕仅丢失情绪信息，文字内容不变。

#### 4.4.3 MockLLMWorker — 经济实惠的测试替身

```cpp
void MockLLMWorker::processText(const SubtitleFrame& frame) {
    SubtitleFrame processedFrame = frame;
    processedFrame.emotion = EmotionType::Joy; // 永远开心
    QTimer::singleShot(1000, this, [this, processedFrame]() {
        emit textProcessed(processedFrame); // 1秒后异步返回
    });
}
```

与 DeepSeek 相同的接口、相同的异步行为（模拟了 1 秒网络延迟）。对于 LLM 不可用或未配置 Key 的场景，它保证系统至少能运转——让用户看到字幕，虽然情绪分析是假的。

#### 4.4.4 KaomojiManager — 数据驱动的扩展点

[KaomojiManager](src/modules/llm/KaomojiManager.cpp) 从 JSON 文件加载颜文字词典，EmotionType → QVector\<QString\> 映射，`getKaomoji()` 随机抽取。特性：
- 文件不存在时自动生成默认词库（降级策略）
- Neutral 情绪返回空字符串（不附加颜文字）
- `saveToFile()` 预留了编辑器的持久化接口

### 4.5 渲染层 (`src/modules/render/`)

#### 4.5.1 SubtitleRenderer — 三层绘制管线

绘制分为三层（[SubtitleRenderer.cpp:109-148](src/modules/render/SubtitleRenderer.cpp#L109-L148)）：

| 层 | 绘制内容 | 参数 | 作用 |
|---|---|---|---|
| 底层 | 黑色半透明阴影 | `(0,0,0,150)`, penWidth=6, offset(4,4) | 增强对比度，适应浅色背景 |
| 中层 | 白色粗描边 | `(255,255,255)`, penWidth=4 | OBS 捕获后在任意背景下都清晰 |
| 顶层 | 粉蓝渐变填充 | `#FFAFCC → #A2D2FF` | 视觉美感，情绪温度感 |

LLM 处理中 vs 处理完毕的视觉区分：
- **处理中**（`!isLlmProcessed && isFinal`）：纯白文字
- **处理完毕**（`isLlmProcessed`）：粉蓝渐变色

这个细节让观众能直观感知"AI 正在思考中"。

#### 4.5.2 buildTextPath() — 自适应排版引擎

[buildTextPath()](src/modules/render/SubtitleRenderer.cpp#L42-L107) 实现了完整的文本排版循环：

```
for (fontSize = 60; fontSize > 12; fontSize -= 2) {
    分词 → 贪心换行 → 检查是否超出窗口高度
    若能容纳 → break
}
```

关键细节：
- **CJK 感知**：中文字符（Unicode 范围 `0x4E00-0x9FA5`）每个字作为独立 Token，字母/数字/符号"粘"在一起
- **渐进缩小**：字体从 60pt 起每次减 2pt，直到文本能放进窗口为止
- **QPainterPath 缓存**：重排只在 `updateFrame()` 和 `resizeEvent()` 时触发，`paintEvent()` 仅回放缓存的 Path

**设计评价**：将排版计算（CPU 密集）从绘制事件（需 60fps）中剥离，是典型的"脏标记 + 缓存"模式。`m_textPath` 作为排版结果的不可变缓存，避免了每帧重复计算。

### 4.6 系统监控层 (`src/modules/system/`)

[SystemResourceMonitor](src/modules/system/SystemResourceMonitor.cpp) 使用 Windows API：
- CPU：`GetProcessTimes()` 两次采样差分 / `GetSystemTimeAsFileTime()` 经过时间
- 内存：`GetProcessMemoryInfo()` 的 WorkingSetSize

实现约 76 行，预留了非 Windows 平台的 `#else` 分支。定时器驱动，每秒刷新一次。设计上保持了极简——它只是一个数据源，不关心谁在消费。

### 4.7 UI 层 (`src/ui/`)

#### 4.7.1 MainWindow — 薄控制器模式

MainWindow 遵循"薄 UI"原则：它只是数据的终点和用户操作的起点，**不包含任何业务逻辑**。所有逻辑委托给 `AppController`。

#### 4.7.2 API Key 管理 — 安全分层的工程设计

API Key 的存储采用了三层优先级（[MainWindow.cpp:94-119](src/ui/MainWindow.cpp#L94-L119)）：

```
apikey.txt (exe 同级，分发版用户可编辑)
    ↓ 不存在或为空
QSettings (Windows 注册表，开发机缓存)
    ↓ 使用 XOR + Base64 混淆存储
```

关于 XOR 混淆：代码注释自称为"简单的异或混淆"，这说明开发者清楚它不是真正的加密——它只防止明文 Key 躺在注册表里被随意浏览。真正的安全应由操作系统用户权限来保障。

#### 4.7.3 SubtitleRenderer 作为独立窗口

```cpp
setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
setAttribute(Qt::WA_TranslucentBackground);
```

渲染窗口**不是 MainWindow 的子控件**，而是一个独立的无边框置顶透明窗口。这个设计选择是因为：
1. OBS 窗口捕获可以选择"捕获特定窗口"
2. 独立窗口可以自由拖拽定位到任何屏幕位置
3. 透明背景使字幕叠加在任何内容之上

鼠标拖拽兼容了两条路径：Wayland 原生的 `startSystemMove()` 和手动坐标追踪的降级方案。

### 4.8 工具层 (`src/utils/`)

#### 4.8.1 AppPaths — 自适应路径解析

[AppPaths](src/utils/AppPaths.h) 自动适配两种运行模式：

```cpp
static QString getProjectRootDir() {
    QString exeDir = QCoreApplication::applicationDirPath();
    // 分发模式: resources/ 在 exe 同级
    if (QDir(exeDir + "/resources").exists()) return exeDir;
    // 开发模式: exe 在 bin/Debug, 项目根在 ../..
    return QDir::cleanPath(exeDir + "/../..");
}
```

这解决了 C++ 桌面应用最常见的痛点：**"开发时和打包后的路径不一致"**。

---

## 5. 构建系统设计

### 5.1 双模式 CMake 架构

整个构建系统的核心思想：**一份 CMakeLists.txt，一个 `USE_VCPKG` 开关，两种依赖解析路径**。

```
option(USE_VCPKG "Use vcpkg" ON)
       │
       ├── ON (默认):  vcpkg toolchain 自动设 CMAKE_PREFIX_PATH
       │               find_package(whisper CONFIG)
       │               部署: 自动拷贝 Qt 插件目录 + whisper/ggml DLL
       │
       └── OFF:        手动设 CMAKE_PREFIX_PATH → Qt 安装目录
                       find_library(WHISPER_LIB) → third_party/
                       部署: 手动拷贝 whisper/ggml DLL, windeployqt 处理 Qt
```

**设计精妙之处**：
- 源码完全不知道构建模式的存在——它只链接 `${WHISPER_LIBS}`，这个变量在两种模式下都被正确设置
- ggml 的三种存在形式（单体 `ggml.lib` / 分体 `ggml-base.lib` + `ggml-cpu.lib` / 编入 `whisper.lib`）都被兼容
- 运行时 DLL 自动部署极大降低了"在我机器上能跑"的问题

### 5.2 预编译头 (PCH)

[CMakeLists.txt:325-336](CMakeLists.txt#L325-L336) 使用了 Qt 常用头的预编译：

```cmake
target_precompile_headers(${PROJECT_NAME} PRIVATE
    <QString> <QObject> <QWidget> <QDebug>
    <QMutex> <QThread> <QSettings> <QTimer>
    <QJsonDocument> <QNetworkAccessManager>
)
```

这 9 个头文件覆盖了项目 90% 的 Qt API 使用场景，能显著减少增量编译时间。

### 5.3 Preset 体系

```
CMakePresets.json.example  ← 模板，提交到 git
CMakePresets.json          ← 个人配置，.gitignore 忽略
CMakeUserPresets.json      ← 个人配置（另一种形式），.gitignore 忽略
```

这种分层让不同开发者可以在同一仓库上使用不同的 Qt 安装路径、不同的 vcpkg 位置，而互不干扰。

---

## 6. 关键设计决策与权衡

### 6.1 决策：栈上分配所有核心组件

在 [main.cpp](src/main.cpp) 中，所有核心组件（Controller、Engine、Worker）都在栈上构造：

```cpp
AppController controller;
VoskTranscriber voskEngine;
WhisperTranscriber whisperEngine;
// ...
```

**优点**：零堆分配开销，析构顺序确定（栈反向），无内存泄漏风险  
**代价**：所有对象生命周期与 `main()` 绑定，无法动态加载/卸载模块  
**评价**：对于桌面直播工具这种"启动后一直运行"的模式，栈分配是正确的选择

### 6.2 决策：Whisper 模型在构造时加载

```cpp
WhisperTranscriber::WhisperTranscriber(QObject* parent) {
    m_ctx = whisper_init_from_file_with_params(modelPath.toUtf8().constData(), cparams);
    // 这会加载几百 MB 的模型文件到内存
}
```

**问题**：即使用户只用 Vosk，Whisper 的几百 MB 模型也会被加载  
**可能的改进**：延迟加载——在 `start()` 时才初始化模型，`stop()` 时释放  
**当前取舍**：用户需要在 UI 上切换 ASR 引擎时获得即时响应。Whisper 加载需要 2-5 秒，如果在切换时才加载，用户体验会明显下降

### 6.3 决策：视觉锁每句 1.5 秒

```cpp
m_unlockTimer.start(1500); // [AppController.cpp:99]
```

**为什么不根据文本长度动态调整？**  
简单回答：1.5 秒是经验值，对大多数句子（5-20 字）阅读时间合适。动态调整的收益有限但复杂度增加不少。

**讨论点**：要不要让用户可配置这个值？要不要做一个"句末延长"——根据字数线性调整？

### 6.4 决策：不使用异常处理

整个项目没有任何 `try-catch` 块（除了依赖库可能抛出）。错误处理全部通过信号 `errorOccurred()` 和保底值（fallbackFrame）实现。

**风格一致**：Qt 社区传统上不鼓励异常，因为信号槽机制本身就提供了错误传播通道。这种风格结合了 Qt 惯用法和 fail-safe 设计。

### 6.5 决策：双 ASR 引擎同时接收音频

[main.cpp:87-90](src/main.cpp#L87-L90) 中，同一路 PCM 数据同时喂给两个引擎：

```cpp
connect(&micCapture, &AudioCapture::audioDataReady,
        &voskEngine,  &VoskTranscriber::onAudioDataReady);
connect(&micCapture, &AudioCapture::audioDataReady,
        &whisperEngine, &WhisperTranscriber::onAudioDataReady);
```

**代价**：两个引擎都在运转，CPU 和内存消耗更大  
**收益**：切换引擎时零延迟——数据处理从未中断  
**评价**：如果目标机器性能很差（如 4 核低压 CPU），这可能是问题。可以考虑按需启停。但对于"游戏 PC + 直播"的典型配置（6 核+），双引擎热备不是瓶颈。

---

## 7. 代码质量观察

### 7.1 值得肯定的实践

| 实践 | 体现位置 | 为什么好 |
|---|---|---|
| 接口隔离 | `IAudioTranscriber`, `ILanguageModel` | 接口小而专，不强迫实现不需要的方法 |
| 防御性编程 | `DeepSeekAPIWorker` 的 fallbackFrame 链 | 任何失败路径都有兜底，不会崩溃或卡死 |
| 资源管理 | `deleteLater()` + 子对象挂载 | 利用 Qt 对象树自动清理，避免悬挂指针 |
| 时序保护 | `SubtitleFrame::frameId` | 拒绝过时的异步结果，避免 UI 闪烁 |
| 排版缓存 | `QPainterPath m_textPath` | 重排（CPU 密集）与重绘（60fps）分离 |
| 自适应路径 | `AppPaths::getProjectRootDir()` | 同一份代码适配开发和分发两种布局 |
| 双语兼容 | `stringToEmotion()` 支持中英文 | 直接适配 LLM 输出的多样性 |
| 注释即文档 | BUILD.md + DEV_GUIDE.md | 详细记录了"为什么这样做"而非"做了什么" |

### 7.2 可改进的方向

| 类别 | 观察 | 建议 |
|---|---|---|
| **可测试性** | 没有单元测试。`MockLLMWorker` 和 `AudioFileSimulator` 是好的测试替身雏形 | 可以基于 Qt Test 框架添加模块级测试 |
| **配置外部化** | 视觉锁时长 (1.5s)、VAD 静音阈值 (0.8s)、队列上限 (2) 都是硬编码 | 抽成 `QSettings` 可配置项，方便不同场景调优 |
| **日志系统** | 全项目使用 `qDebug()`，没有分级（Info/Warning/Error） | 引入 `qCDebug/category` 或 spdlog，方便发布版本关闭调试日志 |
| **Whisper 内存** | 构造时即加载模型，不使用时占用内存 | 延迟初始化或"按需加载/释放" |
| **错误恢复** | `AppController` 的视觉锁缺乏超时保底 | 10 秒超时强解 + emit errorOccurred |
| **国际化** | UI 字符串硬编码为中文 | `QObject::tr()` + Qt Linguist 可支持多语言 |
| **CI/CD** | 无 CI 配置 | GitHub Actions + vcpkg 二进制缓存可实现自动构建 |

---

## 8. 演进方向与待讨论议题

### 8.1 路线图级话题

1. **OSC 协议集成**  
   README 提到未来将驱动虚拟形象的 Blendshapes。`EmotionType` 枚举已经为此预留——5 种情绪可直接映射到 VRChat 的 5 个表情通道。需要引入 OSC 库（如 `ossia` 或手写 UDP 包）。

2. **多模态输入**  
   当前只有语音和文本。是否考虑：聊天弹幕 → LLM → 虚拟形象反应？游戏事件 → 系统通知字幕？

3. **插件化**  
   如果 ASR/LLM 引擎种类继续增长（SenseVoice、FunASR、Ollama、本地 Qwen），当前的手工 DI 模式会变得笨重。是否需要引入插件加载机制（`QPluginLoader` + JSON 描述文件）？

### 8.2 设计级讨论题

以下问题留给读者（也包括未来的自己）思考：

- **Q1**: 双 ASR 热备 vs 按需切换——对于"游戏直播"场景，内存多占用 500MB 是否可接受？
- **Q2**: 视觉锁 + 队列 vs 全显示 + 覆盖——哪种用户体验更好？当前方案丢弃了锁期间的 partial 文本，观众会感到"跳跃"吗？
- **Q3**: `SubtitleFrame` 作为值类型被大量拷贝（每个信号槽连接一次），当文本很长时是否有性能隐患？是否该考虑 `QSharedPointer<SubtitleFrame>` 或者 move 语义？
- **Q4**: `main.cpp` 随着模块增多会持续膨胀。是否应该在某个规模阈值引入一个 `Application` 类来封装组装逻辑？
- **Q5**: 颜文字的随机抽取（`QRandomGenerator::global()->bounded()`）在连续相同情绪时可能重复抽取同一个颜文字。是否需要"不放回"抽取或冷却机制？

---

> **本文持续更新。下次复盘时，带着上述讨论题的答案回来，把新的设计决策和反思补充在下方。**

---

## 附：讨论记录

<!-- 
讨论格式：
### [日期] 讨论主题
**问题**：...
**分析**：...
**结论**：...
**代码变更**：见 commit xxxxxx
-->
