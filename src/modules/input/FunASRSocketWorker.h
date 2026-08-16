#ifndef FUNASRSOCKETWORKER_H
#define FUNASRSOCKETWORKER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QAbstractSocket>

class QWebSocket;

// FunASR 网络工作对象 — 运行于专用 QThread (moveToThread 常驻模式)
// 职责: QWebSocket 生命周期、连接、收发、JSON 解析全部在 worker 线程内完成;
//       主线程 (FunASRTranscriber) 只经 queued 信号/槽与它交互。
// 协议: 官方 funasr_wss_server 2pass 模式
class FunASRSocketWorker : public QObject {
	Q_OBJECT
public:
	explicit FunASRSocketWorker(QObject* parent = nullptr);
	~FunASRSocketWorker() override;

public slots:
	void initialize();                                      // 线程启动后在 worker 线程内创建 socket (socket 必须创建于其所属线程)
	void openConnection(const QUrl& url, const QString& wavName);// 发起连接; 成功后自动发配置帧
	void sendAudioChunk(const QByteArray& chunk);           // 发送已切块的 PCM (0.5s 块)
	void sendEndFlag();                                     // 发送 {"is_speaking": false}
	void abortConnection();                                 // 立即断开 (stop 语义)

signals:
	void connected();
	void resultReady(const QString& text, bool isFinal);    // 已含 is_final 映射 (双最终帧防护)
	void socketError(const QString& message);
	void disconnected();

private slots:
	void onSocketConnected();
	void onTextMessageReceived(const QString& message);
	void onSocketError(QAbstractSocket::SocketError error);
	void onSocketDisconnected();

private:
	void sendConfigFrame();

	QWebSocket* m_socket = nullptr;   // initialize() 中于 worker 线程创建, 常驻复用
	QString m_wavName;                // 会话标识, openConnection 时注入
};

#endif // FUNASRSOCKETWORKER_H
