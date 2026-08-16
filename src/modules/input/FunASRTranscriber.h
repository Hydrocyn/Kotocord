#ifndef FUNASRTRANSCRIBER_H
#define FUNASRTRANSCRIBER_H

#include "IAudioTranscriber.h"
#include <QByteArray>
#include <QString>
#include <QThread>

class FunASRSocketWorker;

// FunASR 流式语音识别 — WebSocket 客户端 (网络线程化版, L1.5-P3)
// 结构: 本类为主线程接口对象, IAudioTranscriber 语义不变 (与 L1 版完全相同);
//       QWebSocket 与 JSON 解析在专用 worker 线程 (FunASRSocketWorker) 内运行,
//       两边经跨线程信号自动排队通信。
// 协议: 官方 funasr_wss_server 2pass 模式 (配置帧 + 0.5s 切块 PCM + 结束帧)
// 断句由服务端 VAD 完成, 客户端只做固定 0.5s 切块流式发送
class FunASRTranscriber : public IAudioTranscriber {
	Q_OBJECT
public:
	explicit FunASRTranscriber(QObject* parent = nullptr);
	~FunASRTranscriber() override;

	void setServerUrl(const QString& url);// 默认 ws://localhost:10095

	bool start() override;// 发起异步连接; true = 连接尝试已发起
	void stop() override; // 主动终止: 立即断开, 不发结束帧

public slots:
	void onAudioDataReady(const QByteArray& data) override;// 主线程: 缓冲 → 0.5s 切块 → 转交 worker
	void onAudioStreamFinished() override;                 // 主线程: 冲残留 → 转交 worker 发结束帧

signals:
	// 主线程 → worker (跨线程自动排队)
	void audioChunkReady(const QByteArray& chunk);
	void endStreamRequested();
	// 对外通知 (L2-2g CLI 联调用; 未来 UI 接入亦用): 连接成功后驱动方再开始喂音频
	void connectionEstablished();

private slots:
	// worker → 主线程 (跨线程自动排队)
	void onWorkerConnected();
	void onWorkerResult(const QString& text, bool isFinal);
	void onWorkerError(const QString& message);
	void onWorkerDisconnected();

private:
	void flushChunks();// 缓冲 ≥ CHUNK_BYTES 时切块转交 worker

	QThread m_workerThread;
	FunASRSocketWorker* m_worker = nullptr;// moveToThread 到 m_workerThread, 线程结束时 deleteLater
	QString m_serverUrl = QStringLiteral("ws://localhost:10095");
	QByteArray m_pendingAudio;
	bool m_connected = false;  // 由 worker 信号回写 (跨线程排队到达)
	bool m_connecting = false; // start() 已发起、尚未接通
	static constexpr int CHUNK_BYTES = 16000;// 16kHz × 2字节 × 0.5s
};

#endif // FUNASRTRANSCRIBER_H
