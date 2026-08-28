#include "PythonEdgeTTS.h"
#include "../../utils/AppPaths.h"
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QDebug>

PythonEdgeTTS::PythonEdgeTTS(QObject* parent)
	: ITextToSpeech(parent),
	  m_pythonPath(AppPaths::getProjectRootDir() +
	               QStringLiteral("/.venv-edge-tts/Scripts/python.exe")),
	  m_sidecarPath(AppPaths::getProjectRootDir() +
	                QStringLiteral("/tools/tts-sidecar/tts_sidecar.py")) {
	connect(&m_process, &QProcess::finished,
	        this, &PythonEdgeTTS::onProcessFinished);
	connect(&m_process, &QProcess::errorOccurred,
	        this, &PythonEdgeTTS::onProcessError);
}

PythonEdgeTTS::~PythonEdgeTTS() {
	if (m_process.state() != QProcess::NotRunning) {
		m_process.kill();
		m_process.waitForFinished(2000);
	}
	cleanupOutput();
}

void PythonEdgeTTS::setPythonPath(const QString& path) {
	m_pythonPath = path;
}

void PythonEdgeTTS::setSidecarPath(const QString& path) {
	m_sidecarPath = path;
}

void PythonEdgeTTS::synthesize(const QString& text) {
	// 打断语义 (决策 D2): 新调用终止当前合成; 代际计数防旧进程的 finished 信号串扰
	m_processGeneration = ++m_generation;
	if (m_process.state() != QProcess::NotRunning) {
		m_process.kill();
		m_process.waitForFinished(2000);
	}
	cleanupOutput();

	m_currentText = text;
	m_outputPath = QDir::tempPath() +
	               QStringLiteral("/kotocord-tts-%1.mp3")
	                   .arg(QDateTime::currentMSecsSinceEpoch());
	m_process.start(m_pythonPath, {m_sidecarPath, text, m_outputPath});
	qDebug() << "[PyTTS] 合成请求:" << text;
}

void PythonEdgeTTS::stop() {
	m_processGeneration = ++m_generation;// 使在途结果作废
	if (m_process.state() != QProcess::NotRunning) {
		m_process.kill();
		m_process.waitForFinished(2000);
	}
	cleanupOutput();
}

void PythonEdgeTTS::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
	if (m_processGeneration != m_generation) {
		// 旧代际进程 (被新 synthesize/stop 打断) — 结果作废, 静默清理
		cleanupOutput();
		return;
	}
	if (status != QProcess::NormalExit || exitCode != 0) {
		const QString err =
		    QString::fromUtf8(m_process.readAllStandardError()).trimmed();
		qWarning() << "[PyTTS] 合成失败 exit=" << exitCode << err;
		emit error(QStringLiteral("[PyTTS] 合成失败: %1").arg(err));
		cleanupOutput();
		return;
	}
	QFile file(m_outputPath);
	if (!file.open(QIODevice::ReadOnly)) {
		emit error(QStringLiteral("[PyTTS] 产物读取失败: %1").arg(m_outputPath));
		cleanupOutput();
		return;
	}
	const QByteArray audio = file.readAll();
	file.close();
	cleanupOutput();
	qDebug() << "[PyTTS] 合成完成:" << audio.size() << "字节";
	emit audioReady(audio);
}

void PythonEdgeTTS::onProcessError(QProcess::ProcessError processError) {
	// 参数名 processError — 避免遮蔽 error 信号 (KB-006, 同日重犯记录见 log)
	if (m_processGeneration != m_generation) {
		return;// 旧代际进程的错误, 静默
	}
	if (processError == QProcess::FailedToStart) {
		emit error(QStringLiteral("[PyTTS] python 启动失败, 检查 venv 路径: %1")
		               .arg(m_pythonPath));
	} else if (processError == QProcess::Crashed) {
		emit error(QStringLiteral("[PyTTS] sidecar 进程崩溃"));
	}
}

void PythonEdgeTTS::cleanupOutput() {
	if (!m_outputPath.isEmpty() && QFile::exists(m_outputPath)) {
		QFile::remove(m_outputPath);
	}
	m_outputPath.clear();
}
