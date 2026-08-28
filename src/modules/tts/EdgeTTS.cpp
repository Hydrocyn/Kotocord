#include "EdgeTTS.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QRandomGenerator>
#include <QDebug>
#include <ctime>

EdgeTTS::EdgeTTS(QObject* parent)
	: ITextToSpeech(parent) {
	connect(&m_socket, &QWebSocket::connected,
	        this, &EdgeTTS::onConnected);
	connect(&m_socket, &QWebSocket::binaryMessageReceived,
	        this, &EdgeTTS::onBinaryMessageReceived);
	connect(&m_socket, &QWebSocket::errorOccurred,
	        this, &EdgeTTS::onSocketError);
	connect(&m_socket, &QWebSocket::disconnected,
	        this, &EdgeTTS::onDisconnected);
}

EdgeTTS::~EdgeTTS() {
	// Q11 模式: 防止 socket 成员析构时触发本类槽
	disconnect(&m_socket, nullptr, this, nullptr);
}

void EdgeTTS::synthesize(const QString& text) {
	// 打断语义 (决策 D2): 新调用中断当前合成, 状态复位后重新发起
	m_currentText = text;
	m_synthesisInProgress = true;
	m_pending.clear();
	m_audioData.clear();
	m_socket.abort();

	// 握手头伪造 (对照本机 venv 中的 edge-tts 参考实现逐项核对, 2026-08-16):
	// 参考头集 = UA/Origin/Pragma/Cache-Control/Accept-Encoding/Accept-Language
	//          + Cookie: muid=<32位随机hex> ← 最新反滥用要求, 缺失即 401
	// 路线: QWebSocket::open(QNetworkRequest) + setRawHeader (KB-007)
	// 注意花括号初始化 — 圆括号会触发 Most Vexing Parse (被解析为函数声明)
	QNetworkRequest request{QUrl(buildWssUrl())};
	request.setRawHeader(QByteArrayLiteral("User-Agent"),
	                     QByteArrayLiteral(
	                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
	                         "AppleWebKit/537.36 (KHTML, like Gecko) "
	                         "Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0"));
	request.setRawHeader(QByteArrayLiteral("Origin"),
	                     QByteArrayLiteral(
	                         "chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold"));
	request.setRawHeader(QByteArrayLiteral("Pragma"), QByteArrayLiteral("no-cache"));
	request.setRawHeader(QByteArrayLiteral("Cache-Control"),
	                     QByteArrayLiteral("no-cache"));
	request.setRawHeader(QByteArrayLiteral("Accept-Encoding"),
	                     QByteArrayLiteral("gzip, deflate, br, zstd"));
	request.setRawHeader(QByteArrayLiteral("Accept-Language"),
	                     QByteArrayLiteral("en-US,en;q=0.9"));
	request.setRawHeader(QByteArrayLiteral("Cookie"),
	                     "muid=" + generateMuid().toLatin1() + ";");

	// TLS 指纹缓解 (实验 F 结论): Qt 默认密码套件清单窄, ClientHello 与
	// OpenSSL 默认差异大; 换全量支持套件对齐 OpenSSL 默认行为
	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
	ssl.setCiphers(QSslConfiguration::supportedCiphers());
	m_socket.setSslConfiguration(ssl);

	m_socket.open(request);
	qDebug() << "[EdgeTTS] 合成请求:" << text;
}

void EdgeTTS::stop() {
	m_synthesisInProgress = false;
	m_socket.abort();
}

void EdgeTTS::setEndpointUrl(const QString& url) {
	m_endpointUrl = url;
}

// ==========================================
// WebSocket 事件
// ==========================================

void EdgeTTS::onConnected() {
	qDebug() << "[EdgeTTS] 已连接, 发送配置与 SSML";
	sendConfig();
	sendSsml(m_currentText);
}

void EdgeTTS::sendConfig() {
	const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	QJsonObject config{
		{"context", QJsonObject{
			{"synthesis", QJsonObject{
				{"audio", QJsonObject{
					{"metadataoptions", QJsonObject{
						{"sentenceBoundaryEnabled", QStringLiteral("false")},
						{"wordBoundaryEnabled", QStringLiteral("false")},
					}},
					{"outputFormat", QString::fromLatin1(OUTPUT_FORMAT)},
				}},
			}},
		}},
	};
	QByteArray message;
	message.append("X-Timestamp:" + timestamp.toUtf8() + "\r\n");
	message.append("Content-Type:application/json; charset=utf-8\r\n");
	message.append("Path:speech.config\r\n\r\n");
	message.append(QJsonDocument(config).toJson(QJsonDocument::Compact));
	m_socket.sendTextMessage(QString::fromUtf8(message));
}

void EdgeTTS::sendSsml(const QString& text) {
	const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
	const QString ssml = QStringLiteral(
		"<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='zh-CN'>"
		"<voice name='%1'><prosody pitch='+0Hz' rate='+0%' volume='+0%'>%2</prosody></voice>"
		"</speak>")
	                         .arg(QString::fromLatin1(VOICE_NAME), text.toHtmlEscaped());
	QByteArray message;
	message.append("X-RequestId:" + requestId.toUtf8() + "\r\n");
	message.append("Content-Type:application/ssml+xml\r\n");
	message.append("X-Timestamp:" + timestamp.toUtf8() + "\r\n");
	message.append("Path:ssml\r\n\r\n");
	message.append(ssml.toUtf8());
	m_socket.sendTextMessage(QString::fromUtf8(message));
}

void EdgeTTS::onBinaryMessageReceived(const QByteArray& message) {
	m_pending.append(message);
	processPendingFrames();
}

void EdgeTTS::processPendingFrames() {
	// 帧格式: 2字节大端头长 | header 文本 (换行分隔 Key:Value) | 载荷
	// 一个 WS 消息可能含多帧 (粘包), 一帧也可能跨消息 (分包) — 循环解析
	while (m_pending.size() >= 2) {
		const int headerLength =
		    (quint8(m_pending.at(0)) << 8) | quint8(m_pending.at(1));
		const int frameTotal = 2 + headerLength;
		if (m_pending.size() < frameTotal) {
			return;// 帧不完整, 等下一块数据
		}
		const QByteArray header = m_pending.mid(2, headerLength);
		const QByteArray payload = m_pending.mid(frameTotal);
		m_pending.remove(0, frameTotal);

		const QString headerText = QString::fromUtf8(header);
		if (headerText.contains(QStringLiteral("Path:audio\r\n"))) {
			m_audioData.append(payload);
			qDebug() << "[EdgeTTS] 音频帧:" << payload.size()
			         << "字节 (累计" << m_audioData.size() << ")";
		} else if (headerText.contains(QStringLiteral("Path:turn.end"))) {
			qDebug() << "[EdgeTTS] 合成完成, 总音频:" << m_audioData.size() << "字节";
			m_synthesisInProgress = false;
			emit audioReady(m_audioData);
			m_audioData.clear();
		}
		// Path:turn.start / Path:audio.metadata 等帧忽略
	}
}

void EdgeTTS::onSocketError(QAbstractSocket::SocketError socketError) {
	// 参数名用 socketError 而非 error — 避免遮蔽 ITextToSpeech::error 信号 (KB-006)
	if (!m_synthesisInProgress) {
		return;// 完成/停止后的残余信号, 静默 (abort 产物等)
	}
	m_synthesisInProgress = false;
	const QString detail = m_socket.errorString();
	QString hint;
	if (detail.contains(QStringLiteral("403")) ||
	    detail.contains(QStringLiteral("handshake"))) {
		hint = QStringLiteral(" (可能是 Sec-MS-GEC-Version 过期, 品读 Q4)");
	}
	qWarning() << "[EdgeTTS] 连接错误:" << socketError << detail;
	emit error(QStringLiteral("[EdgeTTS] 连接错误: %1%2").arg(detail, hint));
}

void EdgeTTS::onDisconnected() {
	// 对端关闭前 errorOccurred 已先行报告; 此处仅复位状态
	qDebug() << "[EdgeTTS] 连接断开";
	m_synthesisInProgress = false;
}

// ==========================================
// 协议辅助
// ==========================================

QString EdgeTTS::buildWssUrl() const {
	QUrl url(m_endpointUrl);
	QUrlQuery query;
	// 顺序对齐 edge-tts 参考实现 (实验 F 字节对比): Token, ConnectionId, Sec-MS-GEC, Version
	query.addQueryItem(QStringLiteral("TrustedClientToken"),
	                   QString::fromLatin1(TRUSTED_CLIENT_TOKEN));
	query.addQueryItem(QStringLiteral("ConnectionId"),
	                   QUuid::createUuid().toString(QUuid::WithoutBraces));
	query.addQueryItem(QStringLiteral("Sec-MS-GEC"), secMsGec());
	query.addQueryItem(QStringLiteral("Sec-MS-GEC-Version"),
	                   QString::fromLatin1(SEC_MS_GEC_VERSION));
	url.setQuery(query);
	return url.toString();
}

QString EdgeTTS::secMsGec() {
	// Windows 100ns ticks 自 1601-01-01; UNIX→Windows 偏移 11644473600 秒 (品读 Q1)
	// 与参考实现 (edge-tts drm.py) 数学等价: 秒级 5 分钟取整 ×1e7 = ticks 级取整
	const qint64 unixSeconds = qint64(::time(nullptr));
	qint64 ticks = (unixSeconds + 11644473600LL) * 10000000LL;
	ticks -= ticks % 3000000000LL;// 5 分钟取整 (300s × 1e7 ticks)
	const QByteArray input =
	    QByteArray::number(ticks) + QByteArray(TRUSTED_CLIENT_TOKEN);
	return QString::fromLatin1(
	    QCryptographicHash::hash(input, QCryptographicHash::Sha256)
	        .toHex()
	        .toUpper());
}

QString EdgeTTS::generateMuid() {
	// 16 随机字节 → 32 位十六进制大写 (对齐 edge-tts drm.py generate_muid: token_hex(16).upper())
	QByteArray bytes;
	bytes.reserve(16);
	auto* rng = QRandomGenerator::global();
	for (int i = 0; i < 16; ++i) {
		bytes.append(char(rng->generate() & 0xFF));
	}
	return QString::fromLatin1(bytes.toHex().toUpper());
}
