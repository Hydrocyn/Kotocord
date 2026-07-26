#include <QtTest>
#include <QSignalSpy>

#include "core/AppController.h"
#include "core/DataTypes.h"
#include "modules/llm/ILanguageModel.h"
#include "modules/llm/KaomojiManager.h"

// 轻量 Mock LLM — 不做实际网络请求, 直接返回 Joy 情绪
class MockLLMForTest : public ILanguageModel {
    Q_OBJECT
public:
    explicit MockLLMForTest(QObject* parent = nullptr) : ILanguageModel(parent) {}

    void processText(const SubtitleFrame& frame) override {
        SubtitleFrame result = frame;
        result.emotion = EmotionType::Joy;
        result.isLlmProcessed = true;
        emit textProcessed(result);
    }
};

class TestAppController : public QObject {
    Q_OBJECT

private:
    int m_signalCount = 0;
    SubtitleFrame m_lastFrame;

private slots:
    void initTestCase() {
        m_controller = new AppController(this);
        m_llm = new MockLLMForTest(this);
        m_kaomoji = new KaomojiManager(this);
        m_controller->setKaomojiManager(m_kaomoji);

        // 直接用 lambda 计数, 绕过 QSignalSpy 对自定义类型的序列化要求
        connect(m_controller, &AppController::subtitleReadyForRender,
                this, [this](const SubtitleFrame& frame) {
            m_signalCount++;
            m_lastFrame = frame;
        });
    }

    void init() {
        // 每个测试前重置
        m_controller->resetState();
        m_controller->setLanguageModel(nullptr);
        m_controller->setLLMEnabled(false);
        m_signalCount = 0;
        m_lastFrame = SubtitleFrame{};
    }

    // --- 1. 空文本不触发信号 ---
    void testEmptyTextIgnored() {
        m_controller->onASRTextReady("", true);
        QCOMPARE(m_signalCount, 0);

        m_controller->onManualTextEntered("");
        QCOMPARE(m_signalCount, 0);
    }

    // --- 2. 正常文本 → 发射信号 (第一帧: 原始文本) ---
    void testNormalTextEmitsImmediately() {
        m_controller->onASRTextReady("你好", true);

        // 没有 LLM 时, onASRTextReady 发射第一帧, 然后 onLLMTextProcessed 发射第二帧
        QVERIFY(m_signalCount >= 1);
        QCOMPARE(m_lastFrame.rawText, QString("你好"));
        QVERIFY(m_lastFrame.isFinal);
    }

    // --- 3. 中间结果发射但 isFinal=false ---
    void testPartialTextEmits() {
        m_controller->onASRTextReady("你好世", false);

        QVERIFY(m_signalCount >= 1);
        QVERIFY(!m_lastFrame.isFinal);
    }

    // --- 4. LLM 启用时 → 帧被 LLM 处理, 情绪被设置 ---
    void testLLMCalledWhenEnabled() {
        m_controller->setLanguageModel(m_llm);
        m_controller->setLLMEnabled(true);

        m_controller->onASRTextReady("测试", true);

        // 至少有第一帧
        QVERIFY(m_signalCount >= 1);

        // 等待 LLM 回调 (MockLLM 同步)
        QTest::qWait(100);

        // LLM 处理后的帧应该有 Joy 情绪
        QVERIFY(m_signalCount >= 2);
        QVERIFY(m_lastFrame.isLlmProcessed);
    }

    // --- 5. 屏幕锁定: 部分结果被丢弃 ---
    void testPartialDiscardedWhenLocked() {
        m_controller->setLanguageModel(m_llm);
        m_controller->setLLMEnabled(true);

        // 发送一句完整话 → 锁屏
        m_controller->onASRTextReady("第一句", true);
        int countAfterFirst = m_signalCount;
        QVERIFY(countAfterFirst >= 1);

        // 发中间结果 → 应被丢弃
        m_controller->onASRTextReady("半截话", false);
        QCOMPARE(m_signalCount, countAfterFirst);
    }

    // --- 6. 锁屏时完整句子进入队列 ---
    void testQueuedWhenLocked() {
        m_controller->setLanguageModel(m_llm);
        m_controller->setLLMEnabled(true);

        // 第一句 → 触发锁屏
        m_controller->onASRTextReady("第一句", true);
        int countAfterFirst = m_signalCount;

        // 第二句(完整) → 入队 (不立即上屏)
        m_controller->onASRTextReady("第二句", true);
        QCOMPARE(m_signalCount, countAfterFirst);
    }

    // --- 7. setLLMEnabled 切换 ---
    void testLLMEnabledToggle() {
        m_controller->setLanguageModel(m_llm);

        // LLM 关闭时
        m_controller->setLLMEnabled(false);
        m_controller->onASRTextReady("测试", true);
        int countOff = m_signalCount;
        QVERIFY(countOff >= 1);

        // LLM 开启时
        m_controller->setLLMEnabled(true);
        m_controller->onASRTextReady("测试2", true);
        QVERIFY(m_signalCount > countOff);
    }

    // --- 8. 手动文本输入接入 ---
    void testManualTextInput() {
        m_controller->onManualTextEntered("手动输入");
        QVERIFY(m_signalCount >= 1);
        QCOMPARE(m_lastFrame.rawText, QString("手动输入"));
        QVERIFY(m_lastFrame.isFinal);
    }

private:
    AppController* m_controller = nullptr;
    MockLLMForTest* m_llm = nullptr;
    KaomojiManager* m_kaomoji = nullptr;
};

QTEST_MAIN(TestAppController)
#include "TestAppController.moc"
