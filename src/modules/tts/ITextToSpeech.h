#ifndef ITEXTTOSPEECH_H
#define ITEXTTOSPEECH_H

#include <QObject>
#include <QByteArray>
#include <QString>

// 所有 TTS 语音合成引擎的抽象基类
// 打断语义 (2026-08-16 决策 D2): synthesize() 的新调用中断当前合成, 最新请求优先
class ITextToSpeech : public QObject {
	Q_OBJECT
public:
	explicit ITextToSpeech(QObject* parent = nullptr) : QObject(parent) {}
	virtual ~ITextToSpeech() = default;

	virtual void synthesize(const QString& text) = 0;// 合成整句; 新调用打断当前合成
	virtual void stop() = 0;                          // 静默停止

signals:
	// 整句编码音频 (WAV/MP3), 由播放器消费 (决策 D1)
	void audioReady(const QByteArray& audioData);
	void error(const QString& message);
};

#endif // ITEXTTOSPEECH_H
