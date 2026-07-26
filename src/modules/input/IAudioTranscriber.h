#ifndef IAUDIOTRANSCRIBER_H
#define IAUDIOTRANSCRIBER_H

#include <QObject>
#include <QString>

// 所有语音识别引擎的抽象基类
class IAudioTranscriber : public QObject {
    Q_OBJECT
public:
    explicit IAudioTranscriber(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IAudioTranscriber() = default;

    // 启动/停止识别
    virtual bool start() = 0;
    virtual void stop() = 0;

public slots:
	virtual void onAudioDataReady(const QByteArray& data) = 0; //音频准备完成时调用
	virtual void onAudioStreamFinished() = 0; //音频读取完毕时调用

signals:
	//当引擎识别出文字时触发：isFinal = true，文字准备完成，可发给大模型
    void textReady(const QString& text, bool isFinal); 

    // 报错信号
    void errorOccurred(const QString& errorMsg);
};

#endif // IAUDIOTRANSCRIBER_H
