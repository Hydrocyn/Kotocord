#include "FunASRTranscriber.h"
#include "FunASRSocketWorker.h"
#include <QDateTime>
#include <QUrl>
#include <QMetaObject>
#include <QDebug>

FunASRTranscriber::FunASRTranscriber(QObject* parent)
	: IAudioTranscriber(parent) {
	// worker 对象创建于主线程, 随后 moveToThread — 其成员及后续创建的子对象
	// (QWebSocket) 的线程亲和性都归 worker 线程
	m_worker = new FunASRSocketWorker;
	m_worker->moveToThread(&m_workerThread);

	// 线程启动 → 在 worker 线程内初始化 socket (socket 必须创建于其所属线程)
	connect(&m_workerThread, &QThread::started,
	        m_worker, &FunASRSocketWorker::initialize);
	// 线程退出 → 回收 worker (及其子对象 socket), 防止泄漏
	connect(&m_workerThread, &QThread::finished,
	        m_worker, &QObject::deleteLater);

	// worker → 主线程: 跨线程信号自动排队
	connect(m_worker, &FunASRSocketWorker::connected,
	        this, &FunASRTranscriber::onWorkerConnected);
	connect(m_worker, &FunASRSocketWorker::resultReady,
	        this, &FunASRTranscriber::onWorkerResult);
	connect(m_worker, &FunASRSocketWorker::socketError,
	        this, &FunASRTranscriber::onWorkerError);
	connect(m_worker, &FunASRSocketWorker::disconnected,
	        this, &FunASRTranscriber::onWorkerDisconnected);

	// 主线程 → worker: 音频与结束请求经 queued 信号转发
	connect(this, &FunASRTranscriber::audioChunkReady,
	        m_worker, &FunASRSocketWorker::sendAudioChunk);
	connect(this, &FunASRTranscriber::endStreamRequested,
	        m_worker, &FunASRSocketWorker::sendEndFlag);

	m_workerThread.start();
}

FunASRTranscriber::~FunASRTranscriber() {
	// 停止线程并等待 worker 线程内事务收尾 (与 WhisperTranscriber 的 wait 模式一致)
	m_workerThread.quit();
	m_workerThread.wait();
}

void FunASRTranscriber::setServerUrl(const QString& url) {
	m_serverUrl = url;
}

bool FunASRTranscriber::start() {
	if (m_connecting || m_connected) {
		qWarning() << "[FunASR] start() 在连接进行中/已连接状态下被调用, 忽略";
		return false;
	}
	m_connecting = true;
	const QString wavName = QStringLiteral("kotocord-%1")
	                            .arg(QDateTime::currentMSecsSinceEpoch());
	m_pendingAudio.clear();// Q9: 防跨会话残留

	// 两条 queued 调用按序在 worker 线程执行: 先确保 socket 就绪, 再发起连接
	QMetaObject::invokeMethod(m_worker, "initialize", Qt::QueuedConnection);
	QMetaObject::invokeMethod(m_worker, "openConnection", Qt::QueuedConnection,
	                          Q_ARG(QUrl, QUrl(m_serverUrl)),
	                          Q_ARG(QString, wavName));
	return true;// 连接结果经 onWorkerConnected / onWorkerError 异步通知
}

void FunASRTranscriber::stop() {
	// 主动终止: 主线程侧状态立即复位, worker 侧 abort 排队执行
	m_connected = false;
	m_connecting = false;
	QMetaObject::invokeMethod(m_worker, "abortConnection", Qt::QueuedConnection);
}

// ==========================================
// 音频输入 (主线程槽, IAudioTranscriber 接口)
// ==========================================

void FunASRTranscriber::onAudioDataReady(const QByteArray& data) {
	qDebug() << "[FunASR] onAudioDataReady 线程:" << QThread::currentThread();
	if (!m_connected) {
		qWarning() << "[FunASR] 未连接, 丢弃音频" << data.size() << "字节";
		return;
	}
	m_pendingAudio.append(data);
	flushChunks();
}

void FunASRTranscriber::onAudioStreamFinished() {
	if (!m_connected) {
		return;
	}
	// 冲掉残留缓冲 (不足一块也发送), 尾部 <0.5s 音频不丢失;
	// 信号按发射顺序排队到 worker, 保证残留块先于结束帧到达
	if (!m_pendingAudio.isEmpty()) {
		emit audioChunkReady(m_pendingAudio);
		m_pendingAudio.clear();
	}
	emit endStreamRequested();
}

void FunASRTranscriber::flushChunks() {
	while (m_pendingAudio.size() >= CHUNK_BYTES) {
		QByteArray chunk = m_pendingAudio.left(CHUNK_BYTES);
		m_pendingAudio.remove(0, CHUNK_BYTES);
		emit audioChunkReady(chunk);
	}
}

// ==========================================
// worker 信号回传 (主线程槽, 跨线程自动排队)
// ==========================================

void FunASRTranscriber::onWorkerConnected() {
	m_connected = true;
	m_connecting = false;
	qDebug() << "[FunASR] 已连接:" << m_serverUrl;
	emit connectionEstablished();
}

void FunASRTranscriber::onWorkerResult(const QString& text, bool isFinal) {
	emit textReady(text, isFinal);
}

void FunASRTranscriber::onWorkerError(const QString& message) {
	m_connected = false;
	m_connecting = false;
	emit errorOccurred(message);
}

void FunASRTranscriber::onWorkerDisconnected() {
	m_connected = false;
	qDebug() << "[FunASR] 连接断开";
}
