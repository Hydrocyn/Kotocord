#ifndef VOSKTRANSCRIBER_H
#define VOSKTRANSCRIBER_H

#include "IAudioTranscriber.h"
#include <QByteArray>
#include "../../core/RAII.h"  // UniqueVoskModel, UniqueVoskRecognizer

class VoskTranscriber : public IAudioTranscriber {
    Q_OBJECT
public:
    explicit VoskTranscriber(QObject* parent = nullptr);
    ~VoskTranscriber() override;

    bool start() override;
    void stop() override;

public slots:
    void onAudioDataReady(const QByteArray& data) override; //接收声音PCM数据
    void onAudioStreamFinished() override;//声音数据输入完成

private:
    // 使用 unique_ptr + 自定义删除器封装 C API（RAII 自动化资源管理）
    // 声明顺序: 先 recognizer → 后 model.
    // C++ 保证析构顺序为声明逆序 → recognizer 先释放, model 后释放 (满足依赖关系).
    UniqueVoskRecognizer m_recognizer;
    UniqueVoskModel m_model;
    bool m_isRunning;

    // 解析 Vosk 返回的 JSON 字符串
    void parseAndEmitResult(const char* jsonStr, bool isFinal);
};

#endif // VOSKTRANSCRIBER_H
