#include "AudioFileSimulator.h"
#include <QDebug>

AudioFileSimulator::AudioFileSimulator(QObject* parent) : QObject(parent) {
    m_timer.setInterval(100);// 设置定时器，每 100 毫秒触发一次 (模拟真实的音频缓冲延迟)
    connect(&m_timer, &QTimer::timeout, this, &AudioFileSimulator::readNextChunk);
}

AudioFileSimulator::~AudioFileSimulator() {
    stop();
}

bool AudioFileSimulator::start(const QString& filePath) {
    if (m_timer.isActive()) return true;

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("无法打开测试音频文件: " + filePath);
        return false;
    }

    // 跳过前 44 字节的 WAV 格式头：Vosk 只需要纯粹的 PCM 数据，防止识别乱码
    m_file.seek(44);

    m_timer.start();
    qDebug() << "[AudioSim] 开始模拟音频数据流...";
    return true;
}

void AudioFileSimulator::stop() {
    m_timer.stop();
    if (m_file.isOpen()) {
        m_file.close();
    }
    qDebug() << "[AudioSim] 音频模拟结束。";
}

void AudioFileSimulator::readNextChunk() {
    if(!m_file.isOpen() || m_file.atEnd()) {
        qDebug() << "[AudioSim] 文件读取完毕或未打开，停止模拟。";
        stop();
        emit finished();
        return;
    }

    QByteArray chunk = m_file.read(CHUNK_SIZE);

    //qDebug() << "[AudioSim] 成功读取一块音频，大小:" << chunk.size() << "bytes";

    if(chunk.size() > 0) {
        emit audioDataReady(chunk);
    }
}
