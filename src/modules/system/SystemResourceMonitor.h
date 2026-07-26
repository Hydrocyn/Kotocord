#ifndef SYSTEMRESOURCEMONITOR_H
#define SYSTEMRESOURCEMONITOR_H

#include <QObject>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class SystemResourceMonitor: public QObject {
	Q_OBJECT
public:
	explicit SystemResourceMonitor(QObject* parent = nullptr);
	void start(int intervalMs = 1000); // 默认每秒刷新一次
	void stop();

signals:
	// 发送当前的 CPU 占用率 (百分比) 和 内存占用 (MB)
	void resourceUpdated(double cpuPercent,double memoryMB);

private slots:
	void updateStats();

private:
	QTimer m_timer;

#ifdef Q_OS_WIN
	int m_numProcessors;
	ULARGE_INTEGER m_lastCpu;
	ULARGE_INTEGER m_lastSysCpu;
	ULARGE_INTEGER m_lastUserCpu;
#endif
};

#endif // SYSTEMRESOURCEMONITOR_H
