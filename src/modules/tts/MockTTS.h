#ifndef MOCKTTS_H
#define MOCKTTS_H

#include "ITextToSpeech.h"

// 假 TTS 引擎 — 不依赖任何外部服务 (L1 保守闭环)
// synthesize() 同步生成 0.3s 16kHz 单声道 440Hz 正弦波 WAV 并经 audioReady 发出 (决策 D6)
// 用途: 闭环占位引擎 + 单测基准; L2 被 EdgeTTS 替换
class MockTTS : public ITextToSpeech {
	Q_OBJECT
public:
	explicit MockTTS(QObject* parent = nullptr);
	~MockTTS() override = default;

	void synthesize(const QString& text) override;
	void stop() override;

	QString lastText() const { return m_lastText; }// 供测试断言

private:
	QByteArray generateToneWav() const;

	QString m_lastText;
	static constexpr int SAMPLE_RATE = 16000;
	static constexpr double TONE_SECONDS = 0.3;
};

#endif // MOCKTTS_H
