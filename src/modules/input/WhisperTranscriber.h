#ifndef WHISPERTRANSCRIBER_H
#define WHISPERTRANSCRIBER_H

#include "IAudioTranscriber.h"
#include "../../core/RAII.h"  // UniqueWhisperContext
#include <QThread>
#include <QMutex>
#include <atomic>
#include <QPointer>

class WhisperTranscriber: public IAudioTranscriber {
	Q_OBJECT
public:
	explicit WhisperTranscriber(QObject* parent = nullptr);
	~WhisperTranscriber() override;

	bool start() override;
	void stop() override;

public slots:
	void onAudioDataReady(const QByteArray& data) override;
	void onAudioStreamFinished() override;

private:
	UniqueWhisperContext m_ctx; // unique_ptr + 自定义删除器, 自动 whisper_free
	bool m_isRunning;

	// 暂存音频数据的内部缓冲区，积累一段完整的音频供Whisper进行有效推理
	QByteArray m_audioBuffer;
	QMutex m_bufferMutex;

	// VAD 断句算法所需变量
	bool m_isSpeaking = false;
	int m_silenceBytes = 0;
	// 能量阈值 (16bit音频最大振幅是 32768，500 左右相当于极其微弱的底噪)
	const int VAD_THRESHOLD = 500;
	// 断句停顿：16kHz采样率，16位位深(2字节)，0.8秒 = 16000 * 2 * 0.8 = 25600 字节
	const int MAX_SILENCE_BYTES = 25600;

	// 后台异步推理状态控制
	std::atomic<bool> m_isInferencing; // 防止多次重复触发
	QPointer<QThread> m_inferenceThread;        // 追踪后台线程，防止安全释放时崩溃

	// VAD (断句检测) 和后台推理线程的逻辑
	void processBufferInference();
};

#endif // WHISPERTRANSCRIBER_H
