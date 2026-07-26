#include "MockLLMWorker.h"
#include <QDebug>

MockLLMWorker::MockLLMWorker(QObject* parent) : ILanguageModel(parent) {}

void MockLLMWorker::processText(const SubtitleFrame& frame) {
    qDebug() << "[MockLLM] 收到原始文本，开始处理:" << frame.rawText;

    SubtitleFrame processedFrame = frame;

    // 模拟大模型的情绪分析和颜文字润色
    processedFrame.emotion = EmotionType::Joy; // 假设分析出是开心的

    // 使用 QTimer 模拟 1000 毫秒的网络延迟 (异步非阻塞)
    QTimer::singleShot(1000, this, [this, processedFrame]() {
        qDebug() << "[MockLLM] 处理完成，发出信号!";
        emit textProcessed(processedFrame);
    });
}
