#ifndef DEEPSEEKAPIWORKER_H
#define DEEPSEEKAPIWORKER_H

#include "ILanguageModel.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

class DeepSeekAPIWorker : public ILanguageModel {
    Q_OBJECT
public:
    explicit DeepSeekAPIWorker(QObject* parent = nullptr);
    ~DeepSeekAPIWorker() override;

    // 如果你想用本地的 Ollama，只需改成 "http://localhost:11434/v1/chat/completions" 和对应的本地模型名即可！
    void setApiConfig(const QString& apiKey,
        const QString& apiUrl = "https://api.deepseek.com/chat/completions",
        const QString& modelName = "deepseek-chat");

    void processText(const SubtitleFrame& frame) override;

private:
    QNetworkAccessManager* m_netManager;
    QString m_apiKey;
    QString m_apiUrl;
    QString m_modelName;

    QString buildSystemPrompt() const;// 构造发给大模型的系统级 Prompt
};

#endif // DEEPSEEKAPIWORKER_H
