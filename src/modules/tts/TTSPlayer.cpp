#include "TTSPlayer.h"
#include <QDebug>

TTSPlayer::TTSPlayer(QObject* parent)
	: QObject(parent) {
	m_player.setAudioOutput(&m_audioOutput);
	connect(&m_player, &QMediaPlayer::playbackStateChanged, this,
	        [this](QMediaPlayer::PlaybackState state) {
		if (state == QMediaPlayer::StoppedState && m_playing) {
			m_playing = false;
			emit playbackFinished();
		}
	});
}

void TTSPlayer::play(const QByteArray& audioData) {
	m_player.stop();
	m_buffer.close();
	m_buffer.setData(audioData);// QBuffer 内部拷贝数据, 不依赖调用方生命周期
	m_buffer.open(QIODevice::ReadOnly);
	m_player.setSourceDevice(&m_buffer);// 格式自动识别 (WAV/MP3 同路径)
	m_playing = true;
	m_player.play();
	qDebug() << "[TTSPlayer] 播放" << audioData.size() << "字节音频";
}

void TTSPlayer::stopPlayback() {
	m_playing = false;
	m_player.stop();
}
