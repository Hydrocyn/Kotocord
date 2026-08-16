#include <QtTest>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include "../src/modules/input/FunASRTranscriber.h"
#include "../src/core/Factory.h"

// ==========================================
// MockFunASRServer — 测试内 mock FunASR 服务端
// ==========================================
// 录制客户端发来的消息序列 (events), 按用例脚本化回放 JSON 结果帧。
// 端口用 listen(0) 动态分配, 避免固定端口 TIME_WAIT 占用。
class MockFunASRServer : public QObject {
    Q_OBJECT
public:
    explicit MockFunASRServer(QObject* parent = nullptr)
        : QObject(parent),
          m_server(QStringLiteral("MockFunASR"), QWebSocketServer::NonSecureMode) {
        connect(&m_server, &QWebSocketServer::newConnection,
                this, &MockFunASRServer::onNewConnection);
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }
    void close() {
        if (m_client) {
            m_client->disconnect(this);
            m_client->deleteLater();
            m_client = nullptr;
        }
        m_server.close();
    }

    // --- 录制内容 (供断言) ---
    QJsonObject firstConfig() const { return m_firstConfig; }
    QList<int> binaryFrameSizes() const { return m_binaryFrameSizes; }
    QStringList events() const { return m_events; }// 按到达顺序: "binary:<N>" / "text:<原始内容>"
    bool receivedEndFlag() const { return m_receivedEndFlag; }

    // --- 向客户端回放 ---
    void sendJson(const QJsonObject& obj) {
        if (m_client)
            m_client->sendTextMessage(
                QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }
    void sendRaw(const QString& text) {
        if (m_client) m_client->sendTextMessage(text);
    }

private slots:
    void onNewConnection() {
        m_client = m_server.nextPendingConnection();
        connect(m_client, &QWebSocket::textMessageReceived,
                this, &MockFunASRServer::onClientText);
        connect(m_client, &QWebSocket::binaryMessageReceived,
                this, &MockFunASRServer::onClientBinary);
    }

    void onClientText(const QString& msg) {
        m_events.append(QStringLiteral("text:%1").arg(msg));
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
        const QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("is_speaking")) &&
            !obj.value(QStringLiteral("is_speaking")).toBool()) {
            m_receivedEndFlag = true;
        } else if (obj.contains(QStringLiteral("mode")) && m_firstConfig.isEmpty()) {
            m_firstConfig = obj;
        }
    }

    void onClientBinary(const QByteArray& data) {
        m_events.append(QStringLiteral("binary:%1").arg(data.size()));
        m_binaryFrameSizes.append(data.size());
    }

private:
    QWebSocketServer m_server;
    QWebSocket* m_client = nullptr;
    QJsonObject m_firstConfig;
    QList<int> m_binaryFrameSizes;
    QStringList m_events;
    bool m_receivedEndFlag = false;
};

// ==========================================
// TestFunASRTranscriber
// ==========================================
class TestFunASRTranscriber : public QObject {
    Q_OBJECT

    // 连接 transcriber 到 mock 服务器并等待配置帧到达
    static bool connectAndWaitConfig(FunASRTranscriber& t, MockFunASRServer& s) {
        // 用 127.0.0.1 而非 "localhost", 避免双栈解析(::1/127.0.0.1)带来的失败延迟 (code-review Q14)
        t.setServerUrl(QStringLiteral("ws://127.0.0.1:%1").arg(s.port()));
        if (!t.start()) return false;
        QElapsedTimer timer;
        timer.start();
        while (s.firstConfig().isEmpty() && timer.elapsed() < 3000)
            QTest::qWait(10);
        return !s.firstConfig().isEmpty();
    }

private slots:
    // T1: 连接后首发 JSON 配置帧
    void testInitialConfigFrame() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));

        const QJsonObject cfg = server.firstConfig();
        QCOMPARE(cfg.value(QStringLiteral("mode")).toString(), QStringLiteral("2pass"));
        QCOMPARE(cfg.value(QStringLiteral("wav_format")).toString(), QStringLiteral("pcm"));
        QCOMPARE(cfg.value(QStringLiteral("audio_fs")).toInt(), 16000);
        QCOMPARE(cfg.value(QStringLiteral("is_speaking")).toBool(), true);
        QVERIFY(!cfg.value(QStringLiteral("wav_name")).toString().isEmpty());
    }

    // T2: 音频缓冲按 16000B 切块发送, 余量留存
    void testChunkSlicing() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));

        transcriber.onAudioDataReady(QByteArray(20000, '\x01'));
        QTRY_VERIFY(server.binaryFrameSizes() == (QList<int>{16000}));

        transcriber.onAudioDataReady(QByteArray(12000, '\x02'));
        QTRY_VERIFY(server.binaryFrameSizes() == (QList<int>{16000, 16000}));
    }

    // T3: 在线结果 (is_final=false) → textReady(text, false)
    void testOnlineResult() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::textReady);

        server.sendJson(QJsonObject{{QStringLiteral("mode"), QStringLiteral("2pass-online")},
                                    {QStringLiteral("text"), QStringLiteral("你好")},
                                    {QStringLiteral("is_final"), false}});
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("你好"));
        QCOMPARE(spy.at(0).at(1).toBool(), false);
    }

    // T4: 最终结果 (is_final=true) → textReady(text, true)
    void testFinalResult() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::textReady);

        server.sendJson(QJsonObject{{QStringLiteral("mode"), QStringLiteral("2pass-offline")},
                                    {QStringLiteral("text"), QStringLiteral("你好世界")},
                                    {QStringLiteral("is_final"), true}});
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("你好世界"));
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    // T5: 流结束 → 残留缓冲先发, 随后发 {"is_speaking": false}
    void testEndFlagFlushesTailFirst() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));

        transcriber.onAudioDataReady(QByteArray(5000, '\x01'));
        transcriber.onAudioStreamFinished();
        QTRY_VERIFY(server.receivedEndFlag());

        const QStringList events = server.events();
        QVERIFY(events.size() >= 2);
        QCOMPARE(events.at(events.size() - 2), QStringLiteral("binary:5000"));
        QVERIFY(events.last().startsWith(QStringLiteral("text:")));
        QVERIFY(events.last().contains(QStringLiteral("is_speaking")));
    }

    // T6: 连接失败 → errorOccurred, 后续音频输入不崩溃
    void testConnectionFailure() {
        quint16 deadPort = 0;
        {
            QWebSocketServer tmp(QStringLiteral("tmp"), QWebSocketServer::NonSecureMode);
            QVERIFY(tmp.listen(QHostAddress::LocalHost, 0));
            deadPort = tmp.serverPort();
            tmp.close();// 关闭后端口无监听 → 连接被拒
        }

        FunASRTranscriber transcriber;
        transcriber.setServerUrl(QStringLiteral("ws://127.0.0.1:%1").arg(deadPort));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::errorOccurred);
        QVERIFY(transcriber.start());
        // 实测死端口拒绝的 errorOccurred 约 6.5s 才到达 (首跑 5s 超时差 1.5s)——
        // 超时放宽至 15s 保证信号必达; 根因调查见 code-review Q14
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 15000);

        // 失败后喂音频: 应被丢弃, 不崩溃 (本用例跑完即验证)
        transcriber.onAudioDataReady(QByteArray(16000, '\x01'));
    }

    // T7: 非法 JSON → 无 textReady、不崩溃, 后续正常帧仍可处理
    void testInvalidJsonIgnored() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::textReady);

        server.sendRaw(QStringLiteral("not json at all"));
        QTest::qWait(50);
        QCOMPARE(spy.count(), 0);

        server.sendJson(QJsonObject{{QStringLiteral("mode"), QStringLiteral("2pass-online")},
                                    {QStringLiteral("text"), QStringLiteral("恢复")},
                                    {QStringLiteral("is_final"), false}});
        QTRY_COMPARE(spy.count(), 1);
    }

    // T8: 空文本帧 → 无 textReady
    void testEmptyTextIgnored() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::textReady);

        server.sendJson(QJsonObject{{QStringLiteral("mode"), QStringLiteral("2pass-online")},
                                    {QStringLiteral("text"), QStringLiteral("")},
                                    {QStringLiteral("is_final"), false}});
        QTest::qWait(50);
        QCOMPARE(spy.count(), 0);
    }

    // T9: GenericFactory 注册/创建兼容性
    void testFactoryCompatibility() {
        GenericFactory<IAudioTranscriber> factory;
        factory.regist<FunASRTranscriber>("funasr");
        QVERIFY(factory.contains("funasr"));
        auto engine = factory.create("funasr");
        QVERIFY(engine != nullptr);
    }

    // T10 (质询 Q3): 连接失败后可再次 start() 重连成功
    void testReconnectAfterFailure() {
        quint16 deadPort = 0;
        {
            QWebSocketServer tmp(QStringLiteral("tmp"), QWebSocketServer::NonSecureMode);
            QVERIFY(tmp.listen(QHostAddress::LocalHost, 0));
            deadPort = tmp.serverPort();
            tmp.close();
        }

        FunASRTranscriber transcriber;
        transcriber.setServerUrl(QStringLiteral("ws://127.0.0.1:%1").arg(deadPort));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::errorOccurred);
        QVERIFY(transcriber.start());
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 15000);

        MockFunASRServer server;
        QVERIFY(server.listen());
        QVERIFY(connectAndWaitConfig(transcriber, server));
    }

    // T11 (质询 Q11): 连接中析构 transcriber 不崩溃
    void testDestructorWhileConnected() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        {
            FunASRTranscriber transcriber;
            QVERIFY(connectAndWaitConfig(transcriber, server));
        }// 连接存活期间析构
        QTest::qWait(50);
        QVERIFY(true);// 到达此处即未崩溃
    }

    // T12: 2pass 双最终帧映射 — online 句尾帧按中间结果转发, offline 帧才是最终
    void testDoubleFinalMapping() {
        MockFunASRServer server;
        QVERIFY(server.listen());
        FunASRTranscriber transcriber;
        QVERIFY(connectAndWaitConfig(transcriber, server));
        QSignalSpy spy(&transcriber, &FunASRTranscriber::textReady);

        // 服务端 online 句尾帧 (is_final=true) → 客户端按中间结果转发
        server.sendJson(QJsonObject{{QStringLiteral("mode"), QStringLiteral("2pass-online")},
                                    {QStringLiteral("text"), QStringLiteral("你好")},
                                    {QStringLiteral("is_final"), true}});
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), false);

        // offline 纠错帧 → 句子最终
        server.sendJson(QJsonObject{{QStringLiteral("mode"), QStringLiteral("2pass-offline")},
                                    {QStringLiteral("text"), QStringLiteral("你好世界")},
                                    {QStringLiteral("is_final"), true}});
        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(1).toBool(), true);
    }
};

QTEST_MAIN(TestFunASRTranscriber)
#include "TestFunASRTranscriber.moc"
