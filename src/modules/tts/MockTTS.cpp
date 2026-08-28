#include "MockTTS.h"
#include <QDebug>
#include <cmath>

MockTTS::MockTTS(QObject* parent)
	: ITextToSpeech(parent) {
}

void MockTTS::synthesize(const QString& text) {
	m_lastText = text;
	qDebug() << "[MockTTS] 合成:" << text;
	// 假引擎同步生成 — 新调用天然打断旧合成 (决策 D2 在真实引擎中需显式实现)
	emit audioReady(generateToneWav());
}

void MockTTS::stop() {
	// 同步引擎无进行中状态, 空实现
}

QByteArray MockTTS::generateToneWav() const {
	const int sampleCount = static_cast<int>(SAMPLE_RATE * TONE_SECONDS);
	const int dataSize = sampleCount * 2;// 16bit mono

	// 小端写入辅助 (WAV 格式为小端)
	auto le16 = [](QByteArray& b, quint16 v) {
		b.append(char(v & 0xFF)).append(char((v >> 8) & 0xFF));
	};
	auto le32 = [](QByteArray& b, quint32 v) {
		b.append(char(v & 0xFF)).append(char((v >> 8) & 0xFF))
		    .append(char((v >> 16) & 0xFF)).append(char((v >> 24) & 0xFF));
	};

	// WAV 头 (44 字节)
	QByteArray wav;
	wav.append("RIFF");
	le32(wav, quint32(36 + dataSize));
	wav.append("WAVE");
	wav.append("fmt ");
	le32(wav, 16);                    // fmt 块大小
	le16(wav, 1);                     // PCM 格式
	le16(wav, 1);                     // 单声道
	le32(wav, quint32(SAMPLE_RATE));  // 采样率
	le32(wav, quint32(SAMPLE_RATE * 2));// 字节率
	le16(wav, 2);                     // 块对齐
	le16(wav, 16);                    // 位深
	wav.append("data");
	le32(wav, quint32(dataSize));

	// 440Hz 正弦波数据, 振幅 8000 (满幅 32768 的 ~24%, 听感舒适)
	for (int i = 0; i < sampleCount; ++i) {
		const double t = static_cast<double>(i) / SAMPLE_RATE;
		const qint16 sample = static_cast<qint16>(
		    8000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * t));
		le16(wav, static_cast<quint16>(sample));
	}
	return wav;
}
