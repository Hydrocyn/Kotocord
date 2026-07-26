#include "AudioCapture.h"
#include <QDebug>

AudioCapture::AudioCapture(QObject* parent)
	: QObject(parent),m_audioSource(nullptr),m_audioDevice(nullptr) 
{
	// 构造函数中设置默认的格式 (Vosk/Whisper 均需要 16kHz)
	m_format.setSampleRate(16000);
	m_format.setChannelConfig(QAudioFormat::ChannelConfigMono);
	m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioCapture::~AudioCapture() {
	stop();
}

void AudioCapture::setAudioFormat(const QAudioFormat& format) {
	if(m_audioSource) {
		qDebug() << "[AudioCapture] 警告：录音进行中，无法动态更改音频格式，请先 stop()！";
		return;
	}
	m_format = format;
}

bool AudioCapture::start() {
	if(m_audioSource) return true; // 已经在录音了

	// 检查系统是否有可用的麦克风输入设备
	QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
	if(defaultDevice.isNull()) {
		emit errorOccurred("未检测到任何麦克风设备，请检查硬件连接！");
		return false;
	}

	qDebug() << "[AudioCapture] 找到默认麦克风:" << defaultDevice.description();

	// 成员变量 m_format 进行验证音频格式
	if(!defaultDevice.isFormatSupported(m_format)) {
		qDebug() << "[AudioCapture] 警告：麦克风原生不支持目标格式，Qt 将尝试系统级重采样！";
	}

	m_audioSource = new QAudioSource(defaultDevice,m_format,this);

	// 动态计算内部缓冲区大小 (设定为 200ms 的数据量)
	// 公式：采样率 * 声道数 * 每个采样的字节数 / 5 (即 1/5 秒)
	int bytesPerSample = m_format.bytesPerSample();
	int channelCount = m_format.channelCount();
	int bufferSize = (m_format.sampleRate() * channelCount * bytesPerSample) / 5;

	m_audioSource->setBufferSize(bufferSize);

	// 监听状态变化
	connect(m_audioSource,&QAudioSource::stateChanged,this,&AudioCapture::onStateChanged);

	// 启动录音(返回一个 QIODevice 以读取实时数据)
	m_audioDevice = m_audioSource->start();

	if(!m_audioDevice) {
		emit errorOccurred("无法打开麦克风！可能是权限被拒绝，或被其他程序独占。");
		m_audioSource->deleteLater();
		m_audioSource = nullptr;
		return false;
	}

	// 绑定实时读取信号
	connect(m_audioDevice,&QIODevice::readyRead,this,&AudioCapture::onReadyRead);

	qDebug() << "[AudioCapture] 麦克风已开启，当前采样率:" << m_format.sampleRate();
	return true;
}

void AudioCapture::stop() {
	if(m_audioSource) {
		m_audioSource->stop();
		// 使用 deleteLater() 替代 delete，防止信号回调引发的内存访问崩溃
		m_audioSource->deleteLater();
		m_audioSource = nullptr;// 置空，防止重入
		m_audioDevice = nullptr;
		qDebug() << "[AudioCapture] 麦克风已关闭。";
	}
}

void AudioCapture::onReadyRead() {
	if(!m_audioDevice) return;

	QByteArray data = m_audioDevice->readAll();// 读出当前缓冲区里所有的声音切片

	if(!data.isEmpty()) {
		emit audioDataReady(data);// 将 PCM 数据发射给下游的双引擎
	}
}

void AudioCapture::onStateChanged(QAudio::State state) {
	if(state == QAudio::StoppedState) {
		if(m_audioSource && m_audioSource->error() != QAudio::NoError) {
			emit errorOccurred("麦克风工作异常，错误代码: " + QString::number(m_audioSource->error()));
		}
	}
}
