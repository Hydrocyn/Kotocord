#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "modules/tts/PythonEdgeTTS.h"
#include "modules/tts/TTSPlayer.h"

// ==========================================
// TTS CLI 联调客户端 (L2-3g; D9 起驱动 Python 侧车引擎)
// ==========================================
// 用法: tts_client_cli <文本> [output.mp3]
//   无输出路径 → 合成后直接播放 (E2 试听)
//   有输出路径 → 保存 mp3 文件 (E1 产物检查)
// 引擎: PythonEdgeTTS (QProcess → venv edge-tts 库 → 临时 mp3)
int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);

	if (argc < 2) {
		qCritical() << "用法: tts_client_cli <文本> [output.mp3]";
		return 1;
	}
	const QString text = QString::fromLocal8Bit(argv[1]);
	const QString outPath =
	    argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();

	PythonEdgeTTS tts;

	// 合成完成 → 保存或播放
	QObject::connect(&tts, &PythonEdgeTTS::audioReady, [&](const QByteArray& audio) {
		if (audio.isEmpty()) {
			qCritical() << "合成结果为空";
			app.exit(1);
			return;
		}
		if (!outPath.isEmpty()) {
			QFile file(outPath);
			if (!file.open(QIODevice::WriteOnly)) {
				qCritical() << "写入失败:" << outPath;
				app.exit(1);
				return;
			}
			file.write(audio);
			qInfo() << "已保存" << audio.size() << "字节 →" << outPath;
			app.exit(0);
		} else {
			auto* player = new TTSPlayer(&app);
			QObject::connect(player, &TTSPlayer::playbackFinished,
			                 &app, &QCoreApplication::quit);
			player->play(audio);
		}
	});

	// 错误 → 打印并退出 (E3/E4 场景)
	QObject::connect(&tts, &PythonEdgeTTS::error, [&app](const QString& msg) {
		qCritical() << msg;
		app.exit(1);
	});

	tts.synthesize(text);
	return app.exec();
}
