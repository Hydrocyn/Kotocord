#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QVBoxLayout>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QRect>
#include <QTimer>
#include <QFile>

#include "../core/AppController.h"
#include "../utils/AppPaths.h"

//简单的异或混淆(XOR) + Base64 加密算法
static const char CRYPT_KEY = 0x5A; // 随便定一个魔法数字作为密钥
QString obfuscateKey(const QString& plainText) {
    if (plainText.isEmpty()) return "";
    QByteArray data = plainText.toUtf8();
    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ CRYPT_KEY; // 异或操作
    }
    return QString(data.toBase64()); // 转成 Base64 字母串
}
QString deobfuscateKey(const QString& cipherText) {
    if (cipherText.isEmpty()) return "";
    QByteArray data = QByteArray::fromBase64(cipherText.toUtf8());
    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ CRYPT_KEY; // 再次异或即可还原
    }
    return QString(data);
}

MainWindow::MainWindow(AppController* controller,QWidget* parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
	,m_appController(controller)
{
    ui->setupUi(this);
    qRegisterMetaType<SubtitleFrame>("SubtitleFrame");//传达事件循环，说明自定义结构体

	//UI内部窗口管理
    m_overlayWidget = std::make_unique<SubtitleRenderer>(nullptr);
    m_overlayWidget->setAttribute(Qt::WA_QuitOnClose, false);

    //设置字幕初始窗口位置：屏幕中心靠下
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - m_overlayWidget->width()) / 2;// X坐标居中
    int y = screenGeometry.height() - m_overlayWidget->height() - 150;// Y坐标屏幕底部往上抬 150 像素
    m_overlayWidget->move(x, y);
    m_overlayWidget->show();

    // ==========================================
    // 信号槽连接：打通 UI 和中枢神经
    // ==========================================

    // 向Controller发送文本
    connect(ui->btnSend, &QPushButton::clicked, this, [=]() {
        QString text = ui->inputBox->text();
        m_appController->onManualTextEntered(text);
        ui->inputBox->clear();
        });
    connect(ui->inputBox, &QLineEdit::returnPressed, ui->btnSend, &QPushButton::click);//同步手动输入
    connect(ui->chkEnableLLM, &QCheckBox::toggled, m_appController, &AppController::setLLMEnabled);//同步LLM启动

	connect(ui->comboModel,&QComboBox::currentIndexChanged,this,[=](int index) {//处理引擎切换
		emit llmEngineSwitched(index == 1);
	});

	connect(ui->chkEnableASR,&QCheckBox::toggled,this,&MainWindow::asrToggleRequested);//处理麦克风启停
	connect(ui->chkEnableASR,&QCheckBox::toggled,this,[=](bool checked) {
		ui->radioVosk->setEnabled(checked);
		ui->radioWhisper->setEnabled(checked);
	});

	connect(ui->radioVosk,&QRadioButton::toggled,this,[=](bool checked){ if(checked) emit asrEngineSwitched(false); });
	connect(ui->radioWhisper,&QRadioButton::toggled,this,[=](bool checked){ if(checked) emit asrEngineSwitched(true); });

	connect(ui->radioVosk,&QRadioButton::toggled,this,[=](bool checked){ if(checked) emit asrEngineSwitched(false); });
	connect(ui->radioWhisper,&QRadioButton::toggled,this,[=](bool checked){ if(checked) emit asrEngineSwitched(true); });

	//API Key 实时同步
	connect(ui->inputApiKey,&QLineEdit::textChanged,this,[=](const QString& key) {
		emit apiKeyChanged(key); // 通知外部
		QSettings settings("MyStudio","Kotocord");// 加密并存入本地 (如果输入为空则清空本地记录)
		if(key.isEmpty()) {
			settings.remove("API/DeepSeekKey");
		} else {
			settings.setValue("API/DeepSeekKey",obfuscateKey(key));
		}
	});

	// 读取本地配置并触发信号
	// 优先级: apikey.txt (分发版) > QSettings (开发机缓存)
	QString plainKey;
	QString apiKeyPath = AppPaths::getApiKeyFilePath();
	if (QFile::exists(apiKeyPath)) {
		QFile keyFile(apiKeyPath);
		if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			plainKey = QString::fromUtf8(keyFile.readAll()).trimmed();
			keyFile.close();
		}
	}
	if (plainKey.isEmpty()) {
		// 文件不存在或为空 → 回退到 QSettings
		QSettings settings("MyStudio","Kotocord");
		plainKey = deobfuscateKey(settings.value("API/DeepSeekKey","").toString());
	}

	// 使用单次定时器将执行时机推迟到事件循环开始后，确保 main.cpp 已经完成了 connect
	QTimer::singleShot(0,this,[this,plainKey]() {
		if(!plainKey.isEmpty()) {
			ui->inputApiKey->setText(plainKey); // 此时发射信号，外部一定能收到
			onSystemReady(true);
		} else {
			onSystemReady(false);
		}
	});
}

MainWindow::~MainWindow() = default;
// unique_ptr 成员自动析构:
//   1. m_overlayWidget 先析构 (SubtitleRenderer 顶层窗口关闭)
//   2. ui 后析构 (Ui::MainWindow 释放所有控件)
// 析构顺序 = 声明顺序的逆序, 编译器保证

// 字幕准备完成
void MainWindow::onSubtitleReady(const SubtitleFrame& frame) {
	if(m_overlayWidget) m_overlayWidget->updateFrame(frame);
}

// 刷新cpu使用率
void MainWindow::updateCpuMem(double cpu,double mem) {
	ui->lblCpu->setText(QString::number(cpu,'f',2) + " %");
	ui->lblMem->setText(QString::number(mem,'f',2) + " MB");
}

//连接监控看板 (大模型性能与消耗) 
void MainWindow::updateLatencyAndTokens(int prompt,int completion,qint64 latency) {
	ui->lblLatency->setText(QString::number(latency) + " ms");
	ui->lblTokens->setText(QString::number(prompt + completion) +
		" (提示: " + QString::number(prompt) + ", 生成: " + QString::number(completion) + ")");
}

void MainWindow::updateEmotionLabel(const SubtitleFrame& frame) {
	QString emotionName;
	switch(frame.emotion) {
	case EmotionType::Joy: emotionName = "高兴 (Joy)"; break;
	case EmotionType::Sadness: emotionName = "悲伤 (Sadness)"; break;
	case EmotionType::Anger: emotionName = "愤怒 (Anger)"; break;
	case EmotionType::Surprise: emotionName = "惊讶 (Surprise)"; break;
	default: emotionName = "平静 (Neutral)"; break;
	}
	ui->lblCurrentEmotion->setText(emotionName);
}

void MainWindow::onSystemReady(bool ready) {
	if(ready) {
		//qDebug() << "[System] 成功从本地加载 API Key 并同步至大脑！";
		ui->lblCurrentEmotion->setText("已就绪，等待输入...");
		ui->lblCurrentEmotion->setStyleSheet("color: #28A745;");
	} else {
		ui->lblCurrentEmotion->setText("未配置 API Key");
		ui->lblCurrentEmotion->setStyleSheet("color: #DC3545;");
	}
}
