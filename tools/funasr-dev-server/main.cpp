#include <QCoreApplication>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QFile>
#include <QTextStream>
#include <QDebug>

// ==========================================
// FunASR 协议级练习服务器 (L2-2f)
// ==========================================
// 学习目的: 完整体验 FunASR WebSocket 服务端的状态机设计。
// 刻意不做真 ASR — 按官方协议回放脚本化识别结果:
//   配置帧 → 记录会话; 二进制 PCM → 每满 16000B 回一条 2pass-online;
//   {"is_speaking": false} → 回 2pass-offline 最终帧 + is_end 结束帧。
// 全协议流量打印到控制台 (学习观察窗口)。
// 用法: funasr_dev_server [port=10095] [script-file]

// 每客户端一个会话: 帧缓冲计数 + 结果段计数 + 脚本游标
class Session : public QObject {
	Q_OBJECT
public:
	Session(QWebSocket* socket, const QStringList& script, QObject* parent = nullptr)
		: QObject(parent), m_socket(socket), m_script(script) {
		connect(socket, &QWebSocket::disconnected, this, &QObject::deleteLater);
	}

public slots:
	void onTextReceived(const QString& msg) {
		qInfo() << "[C→S] text:" << msg;
		QJsonParseError err;
		const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
		if (err.error != QJsonParseError::NoError || !doc.isObject()) {
			return;
		}
		const QJsonObject obj = doc.object();

		if (obj.contains(QStringLiteral("is_speaking")) &&
		    !obj.value(QStringLiteral("is_speaking")).toBool()) {
			// 结束标志: 回离线最终帧 + 结束帧, 会话计数复位 (可复用同一连接)
			sendResult(QStringLiteral("2pass-offline"), true);
			m_socket->sendTextMessage(
			    QStringLiteral(R"({"is_end": true, "is_final": true})"));
			qInfo() << "[S→C] is_end 结束帧";
			m_receivedBytes = 0;
			m_segmentCount = 0;
		} else if (obj.contains(QStringLiteral("mode"))) {
			// 配置帧: 记录会话参数
			m_wavName = obj.value(QStringLiteral("wav_name")).toString();
			qInfo() << "[S→C] 会话建立: wav_name=" << m_wavName
			        << "mode=" << obj.value(QStringLiteral("mode")).toString();
		}
	}

	void onBinaryReceived(const QByteArray& data) {
		m_receivedBytes += data.size();
		qInfo() << "[C→S] binary:" << data.size() << "字节 (累计" << m_receivedBytes << ")";
		// 每满 16000B (0.5s @16kHz/16bit) 回一条在线结果 — 与客户端切块节奏一致
		while (m_receivedBytes >= CHUNK_INTERVAL) {
			m_receivedBytes -= CHUNK_INTERVAL;
			sendResult(QStringLiteral("2pass-online"), false);
		}
	}

private:
	void sendResult(const QString& mode, bool isFinal) {
		const QString text = nextText();
		QJsonObject result{
			{QStringLiteral("mode"), mode},
			{QStringLiteral("text"), text},
			{QStringLiteral("is_final"), isFinal},
			{QStringLiteral("wav_name"), m_wavName},
		};
		m_socket->sendTextMessage(
		    QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
		qInfo() << "[S→C]" << mode << "is_final=" << isFinal << "text=" << text;
	}

	QString nextText() {
		++m_segmentCount;
		if (!m_script.isEmpty()) {
			return m_script.at((m_segmentCount - 1) % m_script.size());
		}
		return QStringLiteral("第%1段模拟识别文本").arg(m_segmentCount);
	}

	static constexpr int CHUNK_INTERVAL = 16000;// 0.5s × 16kHz × 2字节

	QWebSocket* m_socket;
	QStringList m_script;
	QString m_wavName;
	qint64 m_receivedBytes = 0;
	int m_segmentCount = 0;
};

class DevServer : public QObject {
	Q_OBJECT
public:
	explicit DevServer(const QStringList& script, QObject* parent = nullptr)
		: QObject(parent), m_script(script),
		  m_server(QStringLiteral("funasr-dev-server"), QWebSocketServer::NonSecureMode) {
		connect(&m_server, &QWebSocketServer::newConnection,
		        this, &DevServer::onNewConnection);
	}

	bool listen(quint16 port) { return m_server.listen(QHostAddress::Any, port); }

private slots:
	void onNewConnection() {
		QWebSocket* socket = m_server.nextPendingConnection();
		Session* session = new Session(socket, m_script);
		connect(socket, &QWebSocket::textMessageReceived,
		        session, &Session::onTextReceived);
		connect(socket, &QWebSocket::binaryMessageReceived,
		        session, &Session::onBinaryReceived);
		qInfo() << "[DevServer] 新客户端接入:" << socket->peerAddress().toString();
		// 握手头捕获 — 观察任意 ws 客户端实际发送的头 (L2 诊断 + 协议学习窗口)
		const QNetworkRequest req = socket->request();
		qInfo() << "[DevServer] 握手头:";
		const QList<QByteArray> headerNames = req.rawHeaderList();
		for (const QByteArray& name : headerNames) {
			qInfo() << "   " << name << ":" << req.rawHeader(name);
		}
	}

private:
	QStringList m_script;
	QWebSocketServer m_server;
};

int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);

	quint16 port = 10095;
	QStringList script;
	if (argc > 1) {
		port = QString::fromLocal8Bit(argv[1]).toUShort();
	}
	if (argc > 2) {
		QFile file(QString::fromLocal8Bit(argv[2]));
		if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QTextStream stream(&file);
			script = stream.readAll().split('\n', Qt::SkipEmptyParts);
			qInfo() << "[DevServer] 已加载脚本文本" << script.size() << "行";
		} else {
			qWarning() << "[DevServer] 脚本文件打开失败, 使用内置生成器";
		}
	}

	DevServer server(script);
	if (!server.listen(port)) {
		qCritical() << "[DevServer] 端口监听失败:" << port;
		return 1;
	}
	qInfo() << "[DevServer] 练习服务器已就绪 — 端口:" << port
	        << " 文本来源:" << (script.isEmpty() ? QStringLiteral("内置生成器")
	                                            : QStringLiteral("脚本文件"));
	return app.exec();
}

#include "main.moc"
