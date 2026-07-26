#include "DeepSeekAPIWorker.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QDebug>

DeepSeekAPIWorker::DeepSeekAPIWorker(QObject* parent)
    : ILanguageModel(parent),
    m_netManager(new QNetworkAccessManager(this)),
    m_apiUrl("https://api.deepseek.com/chat/completions"),
    m_modelName("deepseek-chat") {
}

DeepSeekAPIWorker::~DeepSeekAPIWorker() {
    // m_netManager 挂载在 this 上，由 Qt 对象树自动销毁
}

void DeepSeekAPIWorker::setApiConfig(const QString& apiKey, const QString& apiUrl, const QString& modelName) {
    m_apiKey = apiKey;
    if (!apiUrl.isEmpty()) m_apiUrl = apiUrl;
    if (!modelName.isEmpty()) m_modelName = modelName;

    qDebug() << "[DeepSeekWorker] 配置已更新 | 端点:" << m_apiUrl << "| 模型:" << m_modelName;
}

QString DeepSeekAPIWorker::buildSystemPrompt() const {
    // 结构化 Prompt：严厉要求返回 JSON
    return "你是一个为直播字幕服务的情绪分析器。请分析用户输入的文本，"
        "并在以下5个选项中选择最匹配的一个：[Joy, Sadness, Anger, Surprise, Neutral]。\n"
        "严格遵守以下规则：\n"
        "1. 绝对不要输出任何分析过程或多余文字。\n"
        "2. 强制返回合法的 JSON 对象，格式必须为：{\"emotion\": \"Joy\"}";
}

void DeepSeekAPIWorker::processText(const SubtitleFrame& frame) {

	//保底结果
	SubtitleFrame fallbackFrame = frame;
	fallbackFrame.emotion = EmotionType::Neutral;

    if (frame.rawText.isEmpty() || m_apiKey.isEmpty()) {
		qDebug() << "[DeepSeekWorker] 拦截: 文本为空或未配置 API Key。";
		if(m_apiKey.isEmpty()){
			qDebug() << "未配置 API Key";
		}
		emit errorOccurred(m_apiKey.isEmpty() ? "未配置 API Key" : "文本为空");
		emit textProcessed(fallbackFrame); //强行交还钥匙
        return;
    }

    QNetworkRequest request((QUrl(m_apiUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    // 构造兼容 OpenAI 格式的 JSON Payload
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = buildSystemPrompt();

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = frame.rawText;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject payload;
    payload["model"] = m_modelName;
    payload["messages"] = messages;
    // 强制 DeepSeek/OpenAI 使用 JSON 模式返回 (部分极旧本地模型可能不支持此参数，届时可做兼容开关)
    QJsonObject responseFormat;
    responseFormat["type"] = "json_object";
    payload["response_format"] = responseFormat;

    QJsonDocument doc(payload);
    QByteArray postData = doc.toJson();

	// 使用时间戳，避免 new 带来的泄漏风险
	qint64 startTimeMs = QDateTime::currentMSecsSinceEpoch();
	QNetworkReply* reply = m_netManager->post(request,postData);

	// 引入 5 秒 API 超时自毁机制
	QTimer* timeoutTimer = new QTimer(reply); // 挂载在 reply 上自动销毁
	timeoutTimer->setSingleShot(true);
	timeoutTimer->start(5000);
	connect(timeoutTimer,&QTimer::timeout,reply,[=]() {
		if(reply->isRunning()) {
			qDebug() << "[DeepSeekWorker] ⚠️ API 请求超时 5 秒，强制阻断！";
			reply->abort(); // 这会立刻触发下方的 finished 信号，并带有取消错误
		}
	});

	SubtitleFrame localFrame = frame;// 在这里做一次非 const 的深拷贝

    //捕获时只需要 [=] 即可，它会自动按值捕获 localFrame
    connect(reply, &QNetworkReply::finished, this, [=]() mutable {
		qint64 latency = QDateTime::currentMSecsSinceEpoch() - startTimeMs;

        if (reply->error() != QNetworkReply::NoError) {
			qDebug() << "[DeepSeekWorker] ❌ 网络/API 报错:" << reply->errorString();
            emit errorOccurred("API 请求失败: " + reply->errorString());
			fallbackFrame.tokenCost = 0;
			emit textProcessed(localFrame);
            reply->deleteLater();
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        QJsonObject rootObj = responseDoc.object();

        // 提取大模型返回的正文
        QJsonArray choices = rootObj["choices"].toArray();
        QString jsonStr = choices[0].toObject()["message"].toObject()["content"].toString();
        // 去除markdown格式头
        jsonStr = jsonStr.replace("```json", "").replace("```", "").trimmed();

        //监控净化后的 JSON 和原始文本
        qDebug() << "[LLM] 净化后的JSON:" << jsonStr << "| 原始文本:" << localFrame.rawText;

        // 解析大模型产生的 JSON
        QJsonDocument innerDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (innerDoc.isNull()) {
			qDebug() << "[LLM 错误] JSON 解析失败:" << jsonStr;
			emit textProcessed(fallbackFrame);
			reply->deleteLater();
			return;
        }
        else {
			QString emotionStr = innerDoc.object()["emotion"].toString();
			localFrame.emotion = stringToEmotion(emotionStr);
			qDebug() << "[LLM] 分析完成 | 判定情绪:" << emotionStr << "| 耗时:" << latency << "ms";
        }

        // 提取并报告 Token 消耗
        QJsonObject usage = rootObj["usage"].toObject();
        int promptTokens = usage["prompt_tokens"].toInt();
        int completionTokens = usage["completion_tokens"].toInt();
        localFrame.tokenCost = promptTokens + completionTokens;

        // 发送更新后的帧 (此时 frame.emotion 已被赋予真实的分析结果)
        emit textProcessed(localFrame);
        // 向 UI 广播监控数据
        emit performanceMetricsReported(promptTokens, completionTokens, latency);

        reply->deleteLater();
    });
}
