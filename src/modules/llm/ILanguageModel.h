#ifndef ILANGUAGEMODEL_H
#define ILANGUAGEMODEL_H

#include <QObject>
#include "../../core/DataTypes.h"

class ILanguageModel : public QObject {
    Q_OBJECT
public:
    explicit ILanguageModel(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ILanguageModel() = default;

    virtual void processText(const SubtitleFrame& frame) = 0;

signals:
    // 处理完成后，发出包含情绪和新文本的完整 Frame
    void textProcessed(const SubtitleFrame& processedFrame);

    // 性能与资源监控信号 (提示词Token, 生成Token, API耗时毫秒)
    void performanceMetricsReported(int promptTokens, int completionTokens, qint64 latencyMs);

    //网络错误信息
    void errorOccurred(const QString& errorMsg);
};

#endif // ILANGUAGEMODEL_H
