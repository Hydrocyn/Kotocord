#ifndef KAOMOJIMANAGER_H
#define KAOMOJIMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include "../../core/DataTypes.h"

class KaomojiManager : public QObject {
    Q_OBJECT
public:
    explicit KaomojiManager(QObject* parent = nullptr);

    bool loadFromFile(const QString& filePath);// 加载 JSON 文件
    bool saveToFile(const QString& filePath);// 保存到 JSON 文件 (为以后的 UI 编辑器预留)

    QString getKaomoji(EmotionType emotion) const;// 根据情绪，从池子里随机抽选一个颜文字

private:
    QMap<EmotionType, QVector<QString>> m_kaomojiDict;// 内存中的颜文字字典

    void generateDefaultJson(const QString& filePath);// 如果找不到文件，自动生成一份默认的保底文件
};

#endif // KAOMOJIMANAGER_H
