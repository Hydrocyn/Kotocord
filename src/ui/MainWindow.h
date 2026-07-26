#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "../modules/render/SubtitleRenderer.h"
#include "../core/DataTypes.h"

class AppController;

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
	MainWindow(AppController* controller,QWidget* parent = nullptr);//传入外部AppController
    ~MainWindow();

signals:
	//生成UI点击信号
	void asrToggleRequested(bool enabled);
	void asrEngineSwitched(bool isWhisper); // false: Vosk, true: Whisper
	void llmEngineSwitched(bool isDeepSeek); // false: Mock, true: DeepSeek
	//假如使用的并非两个模型，bool型就不能用了
	void apiKeyChanged(const QString& key);

public slots:
	// 接收来自外部的数据以刷新 UI
	void onSubtitleReady(const SubtitleFrame& frame);
	void updateCpuMem(double cpu,double mem);
	void updateLatencyAndTokens(int prompt,int completion,qint64 latency);
	void updateEmotionLabel(const SubtitleFrame& frame);
	void onSystemReady(bool ready); // 用于更新那个绿色/红色的状态文本

private:
    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<SubtitleRenderer> m_overlayWidget; // 独立顶层窗口, 无 QObject parent, unique_ptr 管理
    AppController* m_appController;
};
#endif // MAINWINDOW_H
