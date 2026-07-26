#include "SubtitleRenderer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QWindow>
#include <QDebug>

SubtitleRenderer::SubtitleRenderer(QWidget* parent)
    : QWidget(parent) {

    // 设置窗口标志：无边框 | 保持置顶 | 作为工具窗口（不在任务栏显示独立图标，可选）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);// 允许背景透明
    resize(1000, 200);// 设置一个比较宽的初始大小，适合放字幕

	// 初始化第一帧结构体
	m_currentFrame.frameId = -1;//-1，防止与真实的第一句(frameId=0)冲突
	m_currentFrame.displayText = "Kotocord Ready!";
	m_currentFrame.isFinal = true; // 设为 true 才能显示渐变色
	m_currentFrame.isLlmProcessed = true; // 假设以处理，强行激活渐变色

	buildTextPath(); // 初始化时生成一次图形
}

void SubtitleRenderer::updateFrame(const SubtitleFrame& frame) {
	// 简单的时序保护：丢弃旧结果
	if(m_currentFrame.frameId > frame.frameId) {
		return;
	}

	m_currentFrame = frame;
	buildTextPath();//重新计算图形轮廓
	update();
}

void SubtitleRenderer::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	buildTextPath();//窗口大小被改变时重新排版
}

// 将重度排版计算从paintevent中剥离
void SubtitleRenderer::buildTextPath() {
	m_textPath = QPainterPath(); // 清空旧缓存
	if(m_currentFrame.displayText.isEmpty()) return;

	// 定义安全渲染区域 (留出 20 像素的内边距，防止描边被切断)
	QRect renderBox = rect().adjusted(20,20,-20,-20);
	int fontSize = 60;// 初始最大字体
	QFont font("Arial",fontSize,QFont::Black);
	QStringList lines;
	int lineHeight = 0;

	// 排版循环：动态换行与缩放
	while(fontSize > 12) {// 字体最小不能小于 12
		font.setPointSize(fontSize);
		QFontMetrics fm(font);
		lineHeight = fm.height();
		lines.clear();
		// 智能分词 (Tokenization) 算法
		QStringList tokens;
		QString currentToken;
		for(int i = 0; i < m_currentFrame.displayText.length(); ++i) {
			QChar c = m_currentFrame.displayText[i];
			// 如果是空格，或者是中日韩统一表意文字 (CJK)，视为独立的断句点
			if(c.isSpace() || (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FA5)) {
				if(!currentToken.isEmpty()) {// 把汉字或空格作为一个独立的 token
					tokens.append(currentToken);
					currentToken.clear();
				}
				tokens.append(QString(c));
			} else {// 字母、数字、颜文字符号，全部“粘”在一起作为一个整体 Token
				currentToken.append(c);
			}
		}
		if(!currentToken.isEmpty()) tokens.append(currentToken);

		QString currentLine = "";// 按 Token 拼装行
		for(const QString& token : tokens) {
			QString testLine = currentLine + token;
			// 如果加上这个 Token 超宽了，并且当前行不是空的，就强制换行
			if(fm.horizontalAdvance(testLine) > renderBox.width() && !currentLine.isEmpty()) {
				lines.append(currentLine);
				currentLine = token.trimmed().isEmpty() ? "" : token;
			} else {// 如果导致换行的是个空格，下一行就不需要以空格开头了
				currentLine = testLine;
			}
		}
		lines.append(currentLine); // 把最后一行加进去

		if(lines.size() * lineHeight <= renderBox.height()) {// 检查总高度是否能放进窗口
			break;// 完美容纳，跳出循环
		}
		fontSize -= 2;// 放不下，缩小字体继续算
	}

	QFontMetrics finalMetrics(font);
	int totalTextHeight = lines.size() * lineHeight;
	// 计算 Y 轴整体居中的起始位置
	int startY = renderBox.top() + (renderBox.height() - totalTextHeight) / 2 + finalMetrics.ascent();

	for(int i = 0; i < lines.size(); ++i) {
		int lineWidth = finalMetrics.horizontalAdvance(lines[i]);
		int startX = renderBox.left() + (renderBox.width() - lineWidth) / 2;// 计算每一行 X 轴居中的位置
		// 将计算好的轮廓写入缓存
		m_textPath.addText(startX,startY + i * lineHeight,font,lines[i]);
	}
}

void SubtitleRenderer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
	if(m_currentFrame.displayText.isEmpty() || m_textPath.isEmpty()) return;

    QPainter painter(this);
    // 每次绘制前清空背景，防止 OBS 捕获到残影
    painter.fillRect(rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 底层：黑色半透明阴影
	painter.translate(4,4);
	painter.setPen(QPen(QColor(0,0,0,150),6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
	painter.setBrush(Qt::NoBrush);
	painter.drawPath(m_textPath);
	painter.translate(-4,-4);

    // 中层：白色粗描边
	painter.setPen(QPen(QColor(255,255,255),4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
	painter.setBrush(Qt::NoBrush);
	painter.drawPath(m_textPath);

    // 顶层：文字内部渐变填充
	painter.setPen(Qt::NoPen);
	if(!m_currentFrame.isLlmProcessed && m_currentFrame.isFinal) {
		// 【第一阶段：大模型正在思考中】
		// 比如用纯白色，并且在句尾悄悄加上 "..." 提示观众 AI 正在算
		painter.setBrush(QColor(255,255,255));
		// 你甚至可以在上面 path.addText 的时候，给这段文字加上一个淡入淡出的动画
	} else {
		// 【第二阶段：大模型分析完毕】
		// 使用你之前酷炫的粉蓝渐变色
		QRect renderBox = rect().adjusted(20,20,-20,-20);
		QLinearGradient gradient(0,renderBox.top(),0,renderBox.bottom());
		gradient.setColorAt(0.0,QColor(255,175,204));
		gradient.setColorAt(1.0,QColor(162,210,255));
		painter.setBrush(gradient);
	}
	painter.drawPath(m_textPath);
}

// 鼠标拖拽逻辑实现 
void SubtitleRenderer::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Wayland 支持
		if(!windowHandle() || !windowHandle()->startSystemMove()) {
			// 如果不支持原生拖拽，降级为记录手动鼠标坐标
			m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
		}
		event->accept();
    }
}

void SubtitleRenderer::mouseMoveEvent(QMouseEvent* event) {
    // 仅非 Wayland
    if ((event->buttons() & Qt::LeftButton) && !m_dragPosition.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}
