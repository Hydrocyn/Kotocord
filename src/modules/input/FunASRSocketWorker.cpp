#include "FunASRSocketWorker.h"
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QDebug>

FunASRSocketWorker::FunASRSocketWorker(QObject* parent)
	: QObject(parent) {
}

FunASRSocketWorker::~FunASRSocketWorker() {
	// 防止 socket 子对象析构时触发本类槽 (Q11 防护, 现在落在 worker 线程侧)
	if (m_socket) {
		disconnect(m_socket, nullptr, this, nullptr);
	}
}

void FunASRSocketWorker::initialize() {
	qDebug() << "[FunASR-Worker] initialize 线程:" << QThread::currentThread();
	if (m_socket) {
		return;// 只创建一次, 线程常驻复用
	}
	m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
	connect(m_socket, &QWebSocket::connected,
	        this, &FunASRSocketWorker::onSocketConnected);
	connect(m_socket, &QWebSocket::textMessageReceived,
	        this, &FunASRSocketWorker::onTextMessageReceived);
	connect(m_socket, &QWebSocket::errorOccurred,
	        this, &FunASRSocketWorker::onSocketError);
	connect(m_socket, &QWebSocket::disconnected,
	        this, &FunASRSocketWorker::onSocketDisconnected);
}

void FunASRSocketWorker::openConnection(const QUrl& url, const QString& wavName) {
	m_wavName = wavName;
	m_socket->open(url);// 连接结果经 connected / errorOccurred 异步通知
}

void FunASRSocketWorker::onSocketConnected() {
	sendConfigFrame();
	emit connected();
}

void FunASRSocketWorker::sendConfigFrame() {
	QJsonObject config{
		{"mode", QStringLiteral("2pass")},
		{"wav_name", m_wavName},
		{"is_speaking", true},
		{"wav_format", QStringLiteral("pcm")},
		{"chunk_size", QJsonArray{5, 10, 5}},
		{"audio_fs", 16000},
	};
	m_socket->sendTextMessage(
	    QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact)));
	qDebug() << "[FunASR-Worker] 配置帧已发送, 线程:" << QThread::currentThread();
}

void FunASRSocketWorker::sendAudioChunk(const QByteArray& chunk) {
	m_socket->sendBinaryMessage(chunk);
}

void FunASRSocketWorker::sendEndFlag() {
	m_socket->sendTextMessage(QStringLiteral(R"({"is_speaking": false})"));
}

void FunASRSocketWorker::abortConnection() {
	m_socket->abort();
}

void FunASRSocketWorker::onTextMessageReceived(const QString& message) {
	// JSON 解析在 worker 线程内完成 (L1 时此职责在主线程, 现已迁入)
	QJsonParseError parseError;
	const QJsonDocument doc =
	    QJsonDocument::fromJson(message.toUtf8(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		qWarning() << "[FunASR-Worker] 非法 JSON 帧, 忽略:" << parseError.errorString();
		return;
	}
	const QJsonObject obj = doc.object();
	const QString text = obj.value(QStringLiteral("text")).toString();
	if (text.isEmpty()) {
		return;// 空文本中间帧不产生结果
	}

	// 2pass 双最终帧映射 (code-review Q2/K1): offline=最终 / online=中间 / mode 缺失保留原值
	const QString mode = obj.value(QStringLiteral("mode")).toString();
	bool isFinal = obj.value(QStringLiteral("is_final")).toBool();
	if (mode.contains(QStringLiteral("offline"))) {
		isFinal = true;
	} else if (mode.contains(QStringLiteral("online"))) {
		isFinal = false;
	}
	qDebug() << "[FunASR-Worker] 结果帧 mode=" << mode
	         << "is_final=" << isFinal << "text=" << text;
	emit resultReady(text, isFinal);
}

void FunASRSocketWorker::onSocketError(QAbstractSocket::SocketError error) {
	qWarning() << "[FunASR-Worker] 连接错误:" << error << m_socket->errorString();
	emit socketError(
	    QStringLiteral("[FunASR] 连接错误: %1").arg(m_socket->errorString()));
}

void FunASRSocketWorker::onSocketDisconnected() {
	qDebug() << "[FunASR-Worker] 连接断开";
	emit disconnected();
}
