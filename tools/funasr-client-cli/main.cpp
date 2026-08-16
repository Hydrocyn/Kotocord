#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>
#include "modules/input/FunASRTranscriber.h"
#include "modules/capture/AudioFileSimulator.h"

// ==========================================
// FunASR CLI 联调客户端 (L2-2g)
// ==========================================
// 用法: funasr_client_cli <wav文件> [server-url 默认 ws://127.0.0.1:10095]
// 链路: AudioFileSimulator (0.1s/块 实时节奏) → FunASRTranscriber → 控制台打印
// 价值: ① UI 集成前给 FunASR 客户端一个真实驱动入口
//      ② 与 funasr_dev_server 构成端到端联调闭环
//      ③ 未来官方服务端就绪后换 URL 即用

int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);

	if (argc < 2) {
		qCritical() << "用法: funasr_client_cli <wav文件> [server-url]";
		return 1;
	}
	const QString wavPath = QString::fromLocal8Bit(argv[1]);
	const QString serverUrl = argc > 2
	                              ? QString::fromLocal8Bit(argv[2])
	                              : QStringLiteral("ws://127.0.0.1:10095");

	AudioFileSimulator simulator;
	FunASRTranscriber transcriber;
	transcriber.setServerUrl(serverUrl);

	// 音频流 → transcriber
	QObject::connect(&simulator, &AudioFileSimulator::audioDataReady,
	                 &transcriber, &FunASRTranscriber::onAudioDataReady);
	QObject::connect(&simulator, &AudioFileSimulator::finished,
	                 &transcriber, &FunASRTranscriber::onAudioStreamFinished);
	QObject::connect(&simulator, &AudioFileSimulator::errorOccurred,
	                 [](const QString& msg) { qCritical() << msg; });

	// 音频流是否已读完——真实 2pass 服务端每句一个最终帧,
	// 只有「流已结束之后」到达的最终帧才代表全部识别完成 (品读 T5 修复)
	bool streamFinished = false;
	QObject::connect(&simulator, &AudioFileSimulator::finished,
	                 [&streamFinished]() { streamFinished = true; });

	// 识别结果 → 控制台; 流结束后收到最终结果才退出
	QObject::connect(&transcriber, &FunASRTranscriber::textReady,
	                 [&app, &streamFinished](const QString& text, bool isFinal) {
		QTextStream out(stdout);
		out << (isFinal ? QStringLiteral("[最终] ") : QStringLiteral("[中间] "))
		    << text << Qt::endl;
		if (isFinal && streamFinished) {
			app.exit(0);
		}
	});

	// 连接失败 → 报错退出
	QObject::connect(&transcriber, &FunASRTranscriber::errorOccurred,
	                 [&app](const QString& msg) {
		qCritical() << msg;
		app.exit(1);
	});

	// 连接成功后再开始喂音频 (与主程序 start() → micCapture.start() 的顺序一致;
	// 同时避免连接期间喂入的音频被丢弃)
	QObject::connect(&transcriber, &FunASRTranscriber::connectionEstablished,
	                 [&]() {
		if (!simulator.start(wavPath)) {
			qCritical() << "WAV 文件打开失败:" << wavPath;
			app.exit(1);
		}
	});

	if (!transcriber.start()) {
		qCritical() << "连接发起失败";
		return 1;
	}

	return app.exec();
}
