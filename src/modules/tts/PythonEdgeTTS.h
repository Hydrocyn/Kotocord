#ifndef PYTHONEDGETTS_H
#define PYTHONEDGETTS_H

#include "ITextToSpeech.h"
#include <QProcess>
#include <QString>

// Python 侧车 TTS 引擎 (决策 D9, 2026-08-16) — QProcess 驱动 venv 中的 edge-tts 库
// 合成链路: 文本 → sidecar 脚本 (tools/tts-sidecar) → 临时 mp3 → audioReady
// 打断语义 (决策 D2): 新 synthesize terminate 当前进程 (代际计数防旧进程信号串扰)
// C++ 直连版 (EdgeTTS) 因 Qt TLS 栈阻抗保留为学习产出与未来优化路线 (T4 路标)
class PythonEdgeTTS : public ITextToSpeech {
	Q_OBJECT
public:
	explicit PythonEdgeTTS(QObject* parent = nullptr);
	~PythonEdgeTTS() override;

	void setPythonPath(const QString& path);  // 默认 <项目根>/.venv-edge-tts/Scripts/python.exe
	void setSidecarPath(const QString& path); // 默认 <项目根>/tools/tts-sidecar/tts_sidecar.py

	void synthesize(const QString& text) override;
	void stop() override;

private slots:
	void onProcessFinished(int exitCode, QProcess::ExitStatus status);
	void onProcessError(QProcess::ProcessError processError);

private:
	void cleanupOutput();

	QString m_pythonPath;
	QString m_sidecarPath;
	QString m_outputPath;   // 当前句的临时 mp3
	QString m_currentText;
	QProcess m_process;
	int m_generation = 0;        // synthesize 代际计数
	int m_processGeneration = 0; // 当前进程所属代际
};

#endif // PYTHONEDGETTS_H
