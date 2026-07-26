#include "VoskTranscriber.h"
#include "../../utils/AppPaths.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QCoreApplication>
#include <QDir>
#include "vosk_api.h"  // vosk_model_new, vosk_recognizer_*, vosk_recognizer_accept_waveform 等

VoskTranscriber::VoskTranscriber(QObject* parent)
    : IAudioTranscriber(parent), m_isRunning(false) {
    // unique_ptr 默认初始化为 nullptr, 无需显式写出
    // m_model / m_recognizer 将在 start() 中延迟创建
}

VoskTranscriber::~VoskTranscriber() {
    stop();
    // m_model / m_recognizer 由 unique_ptr 的删除器自动 vosk_*_free
}

bool VoskTranscriber::start() {
    if (m_isRunning) return true;

    // 加载模型，注意路径
    if (!m_model) {
        //qDebug() << "[Vosk] 正在加载语言模型，可能需要几秒钟...";
		QString modelPath = AppPaths::getVoskModelPath();
        //qDebug() << "[Vosk] 尝试加载模型的绝对路径:" << modelPath;

        // Vosk 接收的是 C 语言风格的字符串 (const char*)，转换一下
        m_model.reset(vosk_model_new(modelPath.toLocal8Bit().constData()));

        if (!m_model) {
            emit errorOccurred("无法加载 Vosk 模型，请检查路径是否正确: " + modelPath);
            return false;
        }
        qDebug() << "[Vosk] 模型加载成功！";
    }

    // 初始化识别器，采样率设定为黄金标准 16000.0f
    if (!m_recognizer) {
        m_recognizer.reset(vosk_recognizer_new(m_model.get(), 16000.0f));
    }

    m_isRunning = true;
    return true;
}

void VoskTranscriber::stop() {
    m_isRunning = false;
}

void VoskTranscriber::onAudioDataReady(const QByteArray& data) {
    //qDebug() << "[Vosk - 入口] 收到信号! 数据大小:" << data.size()
    //    << " | m_isRunning:" << m_isRunning
    //    << " | 识别器非空:" << (m_recognizer != nullptr);

    if(!m_isRunning || !m_recognizer) {
        return;
    }

    // 把数据喂给 Vosk
    int isFinal = vosk_recognizer_accept_waveform(m_recognizer.get(),data.constData(),data.size());

    if(isFinal) {
        const char* result = vosk_recognizer_result(m_recognizer.get());
        //qDebug() << "[Vosk - 最终结果] 原始 JSON:" << result;
        parseAndEmitResult(result,true);
    } else {
        const char* partial = vosk_recognizer_partial_result(m_recognizer.get());
        //qDebug() << "[Vosk - 实时中间] 原始 JSON:" << partial;
        parseAndEmitResult(partial,false);
    }
}

void VoskTranscriber::onAudioStreamFinished() {
    if(!m_isRunning || !m_recognizer) return;

    //qDebug() << "[Vosk] 收到音频流结束信号，强制结算剩余缓冲...";

    // 调用 final_result 强制清空 Vosk 内部的残余音频
    const char* final_result = vosk_recognizer_final_result(m_recognizer.get());

    //qDebug() << "[Vosk - 强制最终结果] 原始 JSON:" << final_result;

    // 把最后这句话当作最终结果 (isFinal = true) 发送出去
    parseAndEmitResult(final_result,true);

    // 结算完成后，如果你希望引擎重置状态迎接下一段语音，可以调用：
    // vosk_recognizer_reset(m_recognizer); 
}

void VoskTranscriber::parseAndEmitResult(const char* jsonStr, bool isFinal) {
    // 将 C 字符串转换为 Qt 的 JSON 对象
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonStr));
    if(doc.isNull() || !doc.isObject()) {
        //qDebug() << "[Vosk] 警告：JSON 解析失败！";
        return;
    }

    QJsonObject obj = doc.object();
    QString parsedText;

    if (isFinal) {
        // 如果是最终结果，Vosk 的 JSON 键名是 "text"
        parsedText = obj.value("text").toString();
    }
    else {
        // 如果是中间结果，Vosk 的 JSON 键名是 "partial"
        parsedText = obj.value("partial").toString();
    }

    // 去除两端空格，如果有文字，就发送给 AppController
    parsedText = parsedText.trimmed();
    if (!parsedText.isEmpty()) {
        emit textReady(parsedText, isFinal);
    }
}
