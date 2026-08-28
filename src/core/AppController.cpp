#include "AppController.h"
#include "../modules/llm/KaomojiManager.h"
#include "../modules/tts/ITextToSpeech.h"
#include <QDebug>

AppController::AppController(QObject* parent)
    : QObject(parent), 
	m_llm(nullptr), 
	m_llmEnabled(true),
	m_kaomojiManager (nullptr),
	m_currentFrameId(1),
	m_isScreenLocked(false)//初始化未锁屏
{
	// 配置定时器：单次触发
	m_unlockTimer.setSingleShot(true);
	connect(&m_unlockTimer,&QTimer::timeout,this,&AppController::processNextInQueue);
}

void AppController::setLanguageModel(ILanguageModel* llm) {
    m_llm = llm;
    if (m_llm) {// 当 LLM 处理完毕时，连接到控制器的槽函数
        connect(m_llm, &ILanguageModel::textProcessed, this, &AppController::onLLMTextProcessed);
    }
}

void AppController::setLLMEnabled(bool enabled) {
    m_llmEnabled = enabled;
    qDebug() << "[AppController] LLM 润色状态变更为:" << (enabled ? "开启" : "关闭");
}

void AppController::setKaomojiManager(KaomojiManager* manager) {
	m_kaomojiManager = manager;
}

void AppController::setTTS(ITextToSpeech* tts) {
	m_tts = tts;
}

void AppController::resetState() {
	m_unlockTimer.stop();
	m_isScreenLocked = false;
	m_pendingQueue.clear();
}

void AppController::onManualTextEntered(const QString& text) {
	if(text.isEmpty()) return;
	qDebug() << "[AppController] 收到手动输入:" << text;

	onASRTextReady(text,true);//手动输入接入统一的处理管道
}

void AppController::onASRTextReady(const QString& text, bool isFinal) {
    if (text.isEmpty()) return;

    //qDebug() << "[AppController - ASR入口] 收到文本:" << text << "| 是否完结(isFinal):" << isFinal;

	if(m_isScreenLocked) {//渲染冲突上锁时
		if(!isFinal) return;//屏幕锁定，语句未结束，丢弃新内容

		if(m_pendingQueue.size() >= 2) {//控制队列规模
			qDebug() << "[AppController] 队列拥堵，丢弃最老的句子";
			m_pendingQueue.dequeue();
		}
		SubtitleFrame queuedFrame;
		queuedFrame.frameId = m_currentFrameId++;//分配ID
		queuedFrame.rawText = text;//设置未处理文本
		queuedFrame.displayText = text;//设置初始渲染文本
		queuedFrame.isFinal = true;//语句已完成
		queuedFrame.isLlmProcessed = false;//未经大模型分析
		m_pendingQueue.enqueue(queuedFrame);
		//qDebug() << "[AppController] 屏幕锁屏中，新句子进入排队。当前队列长度:" << m_pendingQueue.size();
		return;
	}

	// 正常无锁状态
	//qDebug() << "[AppController - ASR入口] 收到文本:" << text << "| 是否完结(isFinal):" << isFinal;
	SubtitleFrame frame;
	frame.frameId = m_currentFrameId;
	frame.rawText = text;
	frame.displayText = text;
	frame.isFinal = isFinal;
	frame.isLlmProcessed = false;

	emit subtitleReadyForRender(frame);//上屏信号

	if(isFinal) {//送大模型处理并上锁
		m_isScreenLocked = true; // 上锁，防止覆盖

		if(m_llm && m_llmEnabled) {
			m_llm->processText(frame);
		} else {
			onLLMTextProcessed(frame);
		}
		m_currentFrameId++;
	}
}

void AppController::onLLMTextProcessed(const SubtitleFrame& frame) {
	SubtitleFrame finalFrame = frame;
	finalFrame.isLlmProcessed = true;

	if(finalFrame.emotion != EmotionType::Neutral && m_kaomojiManager) {
		QString kaomoji = m_kaomojiManager->getKaomoji(finalFrame.emotion);
		finalFrame.displayText = !kaomoji.isEmpty() ? finalFrame.rawText + " " + kaomoji : finalFrame.rawText;
	} else {
		finalFrame.displayText = finalFrame.rawText;
	}
	//qDebug() << "[AppController] 附魔完成，准备刷新屏幕:" << finalFrame.displayText;
	emit subtitleReadyForRender(finalFrame);//发射渲染信号
	if (m_tts) {
		m_tts->synthesize(finalFrame.displayText);// Phase 3: LLM 处理后触发 TTS (决策 D3, 打断语义 D2)
	}
	m_unlockTimer.start(1500);//保留文本时间为1.5秒，再处理下一句
}

void AppController::processNextInQueue() {// 定时器触发的提货函数
	m_isScreenLocked = false; // 解锁屏幕

	if(!m_pendingQueue.isEmpty()) {//等待队列非空，进行帧渲染
		SubtitleFrame nextFrame = m_pendingQueue.dequeue();
		//qDebug() << "[AppController] 从队列提取句子上屏:" << nextFrame.rawText;

		emit subtitleReadyForRender(nextFrame);//发送渲染信号

		m_isScreenLocked = true; // 再次上锁
		if(m_llm && m_llmEnabled) {
			m_llm->processText(nextFrame);
		} else {
			onLLMTextProcessed(nextFrame);
		}
	}
}
