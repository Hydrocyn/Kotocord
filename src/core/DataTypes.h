#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QMetaType>

// 定义支持的情绪类型 (未来可以通过 OSC 映射到虚拟形象的 Blendshapes/Morphs)
enum class EmotionType {
    Neutral = 0, // 平静
    Joy,         // 高兴
    Sadness,     // 悲伤
    Anger,       // 愤怒
    Surprise     // 惊讶
};

// 提供字符串转枚举的工具函数 (兼容中文和英文，方便对接大模型)
inline EmotionType stringToEmotion(const QString& str) {
    if (str == "Joy" || str == "高兴") return EmotionType::Joy;
    if (str == "Sadness" || str == "悲伤") return EmotionType::Sadness;
    if (str == "Anger" || str == "愤怒") return EmotionType::Anger;
    if (str == "Surprise" || str == "惊讶") return EmotionType::Surprise;
    return EmotionType::Neutral; // 默认或匹配失败均为平静
}

// 3. 贯穿整个程序的数据包
struct SubtitleFrame {
	qint64 frameId;         // 帧ID,每句话的唯一标识符
    QString rawText;
    QString displayText;
    EmotionType emotion;
    bool isFinal;
	bool isLlmProcessed;    // 大模型是否已经处理完毕？
    int tokenCost;          // 记录这层转换花掉的 Token 消耗

    // 默认构造函数
	SubtitleFrame(): frameId(0),emotion(EmotionType::Neutral),
		isFinal(true),isLlmProcessed(false),tokenCost(0) {}
};

Q_DECLARE_METATYPE(SubtitleFrame)

#endif // DATATYPES_H
