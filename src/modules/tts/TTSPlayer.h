#ifndef TTSPLAYER_H
#define TTSPLAYER_H

#include <QObject>
#include <QByteArray>
#include <QBuffer>
#include <QMediaPlayer>
#include <QAudioOutput>

// TTS 音频播放器 — 包装 QMediaPlayer, 从内存缓冲播放整句音频 (WAV/MP3) (决策 D1)
// 新 play() 调用打断当前播放 (与 ITextToSpeech 的打断语义对齐, 决策 D2)
// 不单测 (依赖音频后端, 决策 D5), 经联调/试听验证
class TTSPlayer : public QObject {
	Q_OBJECT
public:
	explicit TTSPlayer(QObject* parent = nullptr);
	~TTSPlayer() override = default;

public slots:
	void play(const QByteArray& audioData);// 整句编码音频; 打断当前播放
	void stopPlayback();

signals:
	void playbackFinished();// 一次 play 的音频播放完毕 (供 CLI/联调观测)

private:
	QMediaPlayer m_player;
	QAudioOutput m_audioOutput;
	QBuffer m_buffer;
	bool m_playing = false;// 区分「真实播完」与「play 内部 stop」的 StoppedState
};

#endif // TTSPLAYER_H
