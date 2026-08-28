#ifndef EDGETTS_H
#define EDGETTS_H

#include "ITextToSpeech.h"
#include <QWebSocket>
#include <QByteArray>
#include <QString>
#include <QAbstractSocket>

// Edge-TTS 真实引擎 (L2) — 微软 Edge 免费 TTS, WebSocket 协议 (社区逆向)
// 协议调研见 tts-engine-plan.md §二; 打断语义与接口一致 (决策 D2)
// 初版主线程实现 (决策 D8), 需要时再 worker 化 (复用 FunASR 经验)
class EdgeTTS : public ITextToSpeech {
	Q_OBJECT
public:
	explicit EdgeTTS(QObject* parent = nullptr);
	~EdgeTTS() override;

	void synthesize(const QString& text) override;
	void stop() override;

	void setEndpointUrl(const QString& url);// 调试/测试: 覆盖默认 bing 端点 (如指向本地 dev-server)

private slots:
	void onConnected();
	void onBinaryMessageReceived(const QByteArray& message);
	void onSocketError(QAbstractSocket::SocketError socketError);
	void onDisconnected();

private:
	void processPendingFrames();        // 解析累积缓冲中的帧 (2字节大端头长)
	void sendConfig();                  // speech.config 消息
	void sendSsml(const QString& text); // ssml 消息
	QString buildWssUrl() const;        // Sec-MS-GEC 等查询参数拼接
	static QString secMsGec();          // SHA-256(ticks+token) 大写十六进制
	static QString generateMuid();      // 16 随机字节 → 32 位十六进制大写 (Cookie 用)

	QWebSocket m_socket;
	QString m_currentText;
	QString m_endpointUrl = QString::fromLatin1(WSS_ENDPOINT);// 默认 bing, 可覆盖
	QByteArray m_pending;   // 未解析完的二进制数据 (分包累积)
	QByteArray m_audioData; // 累积的 mp3 数据
	bool m_synthesisInProgress = false;

	// 社区逆向常量 (403 时需随 Edge 版本轮换 — 品读 Q4)
	static constexpr const char* TRUSTED_CLIENT_TOKEN = "6A5AA1D4EAFF4E9FB37E23D68491C6F4";
	static constexpr const char* SEC_MS_GEC_VERSION = "1-143.0.3650.75";
	static constexpr const char* WSS_ENDPOINT =
	    "wss://speech.platform.bing.com/consumer/speech/synthesize/readaloud/edge/v1";
	static constexpr const char* VOICE_NAME = "zh-CN-XiaoxiaoNeural";
	static constexpr const char* OUTPUT_FORMAT = "audio-24khz-48kbitrate-mono-mp3";
};

#endif // EDGETTS_H
