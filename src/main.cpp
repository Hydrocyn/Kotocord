#include <QApplication>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include "ui/MainWindow.h"
#include "utils/AppPaths.h"
#include "core/AppController.h"
#include "core/Factory.h"            // GenericFactory — 泛型注册工厂
#include "modules/input/VoskTranscriber.h"
#include "modules/input/WhisperTranscriber.h"
#include "modules/capture/AudioCapture.h"
#include "modules/llm/MockLLMWorker.h"
#include "modules/llm/DeepSeekAPIWorker.h"
#include "modules/llm/KaomojiManager.h"
#include "modules/system/SystemResourceMonitor.h"
#include "modules/tts/PythonEdgeTTS.h" // Phase 3: TTS 引擎 (D9 Python 侧车, 产品引擎)
#include "modules/tts/TTSPlayer.h"     // Phase 3: 音频播放器

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // ==========================================
    // 第一步：使用工厂模式创建所有组件
    // ==========================================

    // --- ASR 引擎工厂 ---
    GenericFactory<IAudioTranscriber> asrFactory;
    asrFactory.regist<VoskTranscriber>("vosk");
    asrFactory.regist<WhisperTranscriber>("whisper");

    auto voskEngine   = asrFactory.create("vosk");
    auto whisperEngine = asrFactory.create("whisper");
    // static_cast 安全: create("vosk") 一定返回 VoskTranscriber
    auto* voskRaw    = static_cast<VoskTranscriber*>(voskEngine.get());
    auto* whisperRaw = static_cast<WhisperTranscriber*>(whisperEngine.get());

    // --- LLM 引擎工厂 ---
    GenericFactory<ILanguageModel> llmFactory;
    llmFactory.regist<MockLLMWorker>("mock");
    llmFactory.regist<DeepSeekAPIWorker>("deepseek");

    auto mockLLM     = llmFactory.create("mock");
    auto deepSeekLLM = llmFactory.create("deepseek");
    auto* deepSeekRaw = static_cast<DeepSeekAPIWorker*>(deepSeekLLM.get());

    // --- 基础设施组件 (不需要工厂 — 各只有一个实例) ---
    AppController controller;
    AudioCapture micCapture;
    KaomojiManager kaomojiManager;
    SystemResourceMonitor sysMonitor;
    PythonEdgeTTS pythonTts; // D9: TTS 产品引擎 (QProcess → venv edge-tts)
    TTSPlayer ttsPlayer;     // Phase 3: 播放器

    // 尝试从 apikey.txt 加载 API Key
    QString apiKeyPath = AppPaths::getApiKeyFilePath();
    if (QFile::exists(apiKeyPath)) {
        QFile keyFile(apiKeyPath);
        if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString fileKey = QString::fromUtf8(keyFile.readAll()).trimmed();
            if (!fileKey.isEmpty()) {
                deepSeekRaw->setApiConfig(fileKey);
                qDebug() << "[Main] API Key 已从文件加载:" << apiKeyPath;
            }
            keyFile.close();
        }
    }

    MainWindow window(&controller);

    // ==========================================
    // 第二步：依赖注入与初始配置
    // ==========================================

    kaomojiManager.loadFromFile(AppPaths::getKaomojiPath());
    controller.setKaomojiManager(&kaomojiManager);
    controller.setLanguageModel(mockLLM.get()); // 默认使用 Mock LLM
    controller.setTTS(&pythonTts); // Phase 3: 注入 TTS 引擎 (决策 D3/D4: 默认开启)

    // 类型安全的引擎选择: QString key 替代 bool flag
    //   "vosk"    → VoskTranscriber
    //   "whisper" → WhisperTranscriber
    QString currentAsrKey = "vosk";
    IAudioTranscriber* currentASR = voskEngine.get();
    bool isAsrEnabled = false;

    // ==========================================
    // 第三步：信号-槽连线
    // ==========================================

    // --- 1. 控制器 → UI ---
    QObject::connect(&controller, &AppController::subtitleReadyForRender,
                     &window, &MainWindow::onSubtitleReady);

    // --- 2. 系统/LLM → UI 监控看板 ---
    QObject::connect(&sysMonitor, &SystemResourceMonitor::resourceUpdated,
                     &window, &MainWindow::updateCpuMem);

    QObject::connect(mockLLM.get(), &ILanguageModel::textProcessed,
                     &window, &MainWindow::updateEmotionLabel);
    QObject::connect(deepSeekLLM.get(), &ILanguageModel::textProcessed,
                     &window, &MainWindow::updateEmotionLabel);
    QObject::connect(deepSeekLLM.get(), &ILanguageModel::performanceMetricsReported,
                     &window, &MainWindow::updateLatencyAndTokens);

    // --- 3. 麦克风 → ASR 引擎 ---
    QObject::connect(&micCapture, &AudioCapture::audioDataReady,
                     voskRaw, &VoskTranscriber::onAudioDataReady);
    QObject::connect(&micCapture, &AudioCapture::audioDataReady,
                     whisperRaw, &WhisperTranscriber::onAudioDataReady);

    // --- 4. ASR 引擎 → 控制器 ---
    QObject::connect(voskEngine.get(), &IAudioTranscriber::textReady,
                     &controller, &AppController::onASRTextReady);
    QObject::connect(whisperEngine.get(), &IAudioTranscriber::textReady,
                     &controller, &AppController::onASRTextReady);

    // --- 5. TTS 引擎 → 播放器 (Phase 3) ---
    QObject::connect(&pythonTts, &PythonEdgeTTS::audioReady,
                     &ttsPlayer, &TTSPlayer::play);
    QObject::connect(&pythonTts, &PythonEdgeTTS::error,
                     [](const QString& msg) { qWarning() << msg; });

    // --- 6. UI → 控制器/底层 ---
    // 切换 LLM
    QObject::connect(&window, &MainWindow::llmEngineSwitched, [&](bool isDeepSeek) {
        controller.setLanguageModel(
            isDeepSeek ? static_cast<ILanguageModel*>(deepSeekLLM.get())
                       : static_cast<ILanguageModel*>(mockLLM.get()));
    });

    // 录入 API Key
    QObject::connect(&window, &MainWindow::apiKeyChanged, [&](const QString& key) {
        deepSeekRaw->setApiConfig(key);
    });

    // 启停语音识别
    QObject::connect(&window, &MainWindow::asrToggleRequested, [&](bool enabled) {
        isAsrEnabled = enabled;
        if (enabled) {
            if (currentASR->start()) micCapture.start();
        } else {
            micCapture.stop();
            currentASR->stop();
        }
    });

    // 切换 ASR 引擎 — 用 currentAsrKey 替代 bool isWhisper
    QObject::connect(&window, &MainWindow::asrEngineSwitched, [&](bool isWhisper) {
        currentASR->stop();
        currentAsrKey = isWhisper ? "whisper" : "vosk";
        currentASR = isWhisper
            ? static_cast<IAudioTranscriber*>(whisperEngine.get())
            : static_cast<IAudioTranscriber*>(voskEngine.get());
        if (isAsrEnabled) {
            currentASR->start();
        }
    });

    // ==========================================
    // 第四步：启动
    // ==========================================
    sysMonitor.start(1000);
    window.show();

    return app.exec();
}
