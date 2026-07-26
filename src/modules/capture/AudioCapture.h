#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QObject>
#include <QByteArray>
#include <QAudioSource>
#include <QIODevice>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>

class AudioCapture: public QObject {
	Q_OBJECT
public:
	explicit AudioCapture(QObject* parent = nullptr);
	~AudioCapture();

	void setAudioFormat(const QAudioFormat& format);// 允许外部在 start() 之前动态注入/修改音频格式

	bool start();
	void stop();

signals:
	void audioDataReady(const QByteArray& data);// 源源不断吐出 PCM 原始数据的信号
	void errorOccurred(const QString& errorMsg);// 报错信号

private slots:
	void onReadyRead();// 当麦克风缓冲区有新数据时触发
	void onStateChanged(QAudio::State state); // 监听设备状态变化 (如麦克风被突然拔出)

private:
	QAudioSource* m_audioSource;
	QIODevice* m_audioDevice; // 麦克风的实时数据流句柄
	QAudioFormat m_format; // 保存当前的音频配置要求
};

#endif // AUDIOCAPTURE_H
