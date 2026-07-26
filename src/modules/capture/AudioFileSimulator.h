#ifndef AUDIOFILESIMULATOR_H
#define AUDIOFILESIMULATOR_H

#include <QObject>
#include <QFile>
#include <QTimer>
#include <QByteArray>

class AudioFileSimulator : public QObject {
    Q_OBJECT
public:
    explicit AudioFileSimulator(QObject* parent = nullptr);
    ~AudioFileSimulator();

    bool start(const QString& filePath);// 传入WAV 文件路径开始模拟
    void stop();

signals:
    void audioDataReady(const QByteArray& data);//PCM数据准备完成
    void finished(); // 文件读取完毕
    void errorOccurred(const QString& errorMsg);

private slots:
    void readNextChunk();

private:
    QFile m_file;
    QTimer m_timer;
    const int CHUNK_SIZE = 3200; // 16kHz * 16bit(2bytes) * 1ch * 0.1s = 3200 bytes
};

#endif // AUDIOFILESIMULATOR_H
