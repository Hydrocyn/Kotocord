#ifndef SUBTITLERENDERER_H
#define SUBTITLERENDERER_H

#include <QWidget>
#include <QString>
#include <QPoint>
#include <QPainterPath>
#include "../../core/DataTypes.h"

class SubtitleRenderer : public QWidget {
    Q_OBJECT
public:
    explicit SubtitleRenderer(QWidget* parent = nullptr);

public slots:
	void updateFrame(const SubtitleFrame& frame);

protected:
    void paintEvent(QPaintEvent* event) override;// 核心绘制事件
	void resizeEvent(QResizeEvent* event) override;//窗口大小改变时触发

    // 鼠标事件，用于拖拽无边框窗口
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QPoint m_dragPosition;
	SubtitleFrame m_currentFrame; // 存下当前的完整状态帧
	QPainterPath m_textPath;// 将算好的文字图形缓存起来

	void buildTextPath();// 独立抽出的排版计算引擎
};

#endif // SUBTITLERENDERER_H
