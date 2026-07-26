#ifndef MOCKLLMWORKER_H
#define MOCKLLMWORKER_H

#include "ILanguageModel.h"
#include <QTimer>

class MockLLMWorker : public ILanguageModel {
    Q_OBJECT
public:
    explicit MockLLMWorker(QObject* parent = nullptr);
    ~MockLLMWorker() override = default;

    void processText(const SubtitleFrame& frame) override;// 实现接口：处理文本
};

#endif // MOCKLLMWORKER_H
