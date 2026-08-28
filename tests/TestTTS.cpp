#include <QtTest>
#include "../src/modules/tts/MockTTS.h"
#include "../src/modules/tts/PythonEdgeTTS.h"
#include "../src/core/AppController.h"
#include "../src/core/Factory.h"

// ==========================================
// TestTTS — TTS 引擎与管线集成 (Phase 3 L1)
// ==========================================
// TTSPlayer 不在此单测 (依赖音频后端, 决策 D5), 经试听联调验证。
// 遵循已知测试陷阱: 自定义类型信号用 lambda 计数, 不用 QSignalSpy。
class TestTTS : public QObject {
    Q_OBJECT

private slots:
    // T1: MockTTS 合成 → audioReady 收到合法 WAV (RIFF 头 + 大小正确)
    void testMockTTSGeneratesWav() {
        MockTTS tts;
        QByteArray received;
        int count = 0;
        connect(&tts, &MockTTS::audioReady,
                [&](const QByteArray& data) { received = data; ++count; });

        tts.synthesize(QStringLiteral("你好"));
        QCOMPARE(count, 1);
        QCOMPARE(received.left(4), QByteArray("RIFF"));
        QCOMPARE(received.mid(8, 4), QByteArray("WAVE"));
        // 44 字节头 + 16000Hz × 0.3s × 2字节
        QCOMPARE(received.size(), 44 + 4800 * 2);
    }

    // T2: 打断语义 — 新 synthesize 覆盖旧合成 (决策 D2)
    void testInterruptSemantics() {
        MockTTS tts;
        int count = 0;
        connect(&tts, &MockTTS::audioReady,
                [&](const QByteArray&) { ++count; });

        tts.synthesize(QStringLiteral("第一句"));
        tts.synthesize(QStringLiteral("第二句"));
        QCOMPARE(tts.lastText(), QStringLiteral("第二句"));
        QCOMPARE(count, 2);// 同步假引擎每次调用各发一条 (真实引擎见品读 Q9)
    }

    // T3: 控制器集成 — ASR 最终帧 → LLM 处理 → TTS 收到 displayText (决策 D3)
    void testControllerTriggersTTS() {
        AppController controller;
        MockTTS tts;
        controller.setTTS(&tts);

        controller.onASRTextReady(QStringLiteral("你好"), true);
        QCOMPARE(tts.lastText(), QStringLiteral("你好"));
    }

    // T4: TTS 未注入时管线无崩溃 (nullable 路径)
    void testControllerWithoutTTS() {
        AppController controller;
        // 不调用 setTTS — 验证 m_tts 为空的路径安全
        controller.onASRTextReady(QStringLiteral("你好"), true);
        QVERIFY(true);
    }

    // T5: 工厂兼容性
    void testFactoryCompatibility() {
        GenericFactory<ITextToSpeech> factory;
        factory.regist<MockTTS>("mock");
        QVERIFY(factory.contains("mock"));
        auto engine = factory.create("mock");
        QVERIFY(engine != nullptr);
    }

    // T6 (D9): Python 路径无效 → error 信号 (FailedToStart 路径)
    void testPythonSidecarStartFailure() {
        PythonEdgeTTS tts;
        tts.setPythonPath(QStringLiteral("D:/nonexistent-python/python.exe"));
        QString errMsg;
        int errCount = 0;
        connect(&tts, &PythonEdgeTTS::error,
                [&](const QString& msg) { errMsg = msg; ++errCount; });

        tts.synthesize(QStringLiteral("测试"));
        QTRY_COMPARE(errCount, 1);
        QVERIFY(errMsg.contains(QStringLiteral("启动失败")));
    }
};

QTEST_MAIN(TestTTS)
#include "TestTTS.moc"
