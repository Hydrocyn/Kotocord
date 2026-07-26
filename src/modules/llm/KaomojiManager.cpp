#include "KaomojiManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

KaomojiManager::KaomojiManager(QObject* parent) : QObject(parent) {
    // 构造时先塞一点保底数据，防止 JSON 加载彻底失败时没东西可用
    m_kaomojiDict[EmotionType::Neutral] = { "" };
    m_kaomojiDict[EmotionType::Joy] = { "(*^▽^*)" };
}

bool KaomojiManager::loadFromFile(const QString& filePath) {
    QFile file(filePath);

    // 防呆机制：如果文件不存在，自动帮你建一个
    if (!file.exists()) {
        qDebug() << "[KaomojiManager] 词库文件不存在，自动生成默认配置:" << filePath;
        generateDefaultJson(filePath);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[KaomojiManager] 无法打开词库文件:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "[KaomojiManager] JSON 格式解析失败！请检查是否有语法错误。";
        return false;
    }

    QJsonObject rootObj = doc.object();
    m_kaomojiDict.clear();

    // 遍历 JSON 的键值对
    for (auto it = rootObj.begin(); it != rootObj.end(); ++it) {
        EmotionType type = stringToEmotion(it.key());
        QVector<QString> kaomojis;

        if (it.value().isArray()) {
            QJsonArray arr = it.value().toArray();
            for (const QJsonValue& val : arr) {
                if (val.isString()) {
                    kaomojis.append(val.toString());
                }
            }
        }

        if (!kaomojis.isEmpty()) {
            m_kaomojiDict[type] = kaomojis;
        }
    }

    // 确保 Neutral 永远有一个空字符串垫底
    if (!m_kaomojiDict.contains(EmotionType::Neutral)) {
        m_kaomojiDict[EmotionType::Neutral] = { "" };
    }

    qDebug() << "[KaomojiManager] 成功加载颜文字库，包含" << m_kaomojiDict.size() << "种情绪维度。";
    return true;
}

bool KaomojiManager::saveToFile(const QString& filePath) {
    // 为以后用户在 UI 上点击“保存修改”预留的接口
    QJsonObject rootObj;

    for (auto it = m_kaomojiDict.begin(); it != m_kaomojiDict.end(); ++it) {
        QJsonArray arr;
        for (const QString& k : it.value()) {
            arr.append(k);
        }
        // 简单写死反向映射 (为了保证存回去的 Key 也是英文)
        QString keyStr = "Neutral";
        if (it.key() == EmotionType::Joy) keyStr = "Joy";
        else if (it.key() == EmotionType::Sadness) keyStr = "Sadness";
        else if (it.key() == EmotionType::Anger) keyStr = "Anger";
        else if (it.key() == EmotionType::Surprise) keyStr = "Surprise";

        rootObj[keyStr] = arr;
    }

    QJsonDocument doc(rootObj);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QString KaomojiManager::getKaomoji(EmotionType emotion) const {
    if (!m_kaomojiDict.contains(emotion) || m_kaomojiDict[emotion].isEmpty()) {
        return "";
    }

    const QVector<QString>& list = m_kaomojiDict[emotion];

    // Qt 自带的随机数生成器，在数组范围内抽卡
    int index = QRandomGenerator::global()->bounded(list.size());
    return list[index];
}

void KaomojiManager::generateDefaultJson(const QString& filePath) {
    // 确保 resources 文件夹存在
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 填充默认数据并走一次 save
    m_kaomojiDict.clear();
    m_kaomojiDict[EmotionType::Neutral] = { "" };
    m_kaomojiDict[EmotionType::Joy] = { "(≧∇≦)ﾉ", "(*^▽^*)", "ヾ(✿ﾟ▽ﾟ)ノ" };
    m_kaomojiDict[EmotionType::Sadness] = { "(；´д｀)ゞ", "(T_T)", "o(╥﹏╥)o" };
    m_kaomojiDict[EmotionType::Anger] = { "(╬▔皿▔)凸", "(｀⌒´メ)", "(╯°Д°)╯︵ ┻━┻" };
    m_kaomojiDict[EmotionType::Surprise] = { "(°Д°)", "Σ(っ °Д °;)っ", "w(ﾟДﾟ)w" };

    saveToFile(filePath);
}
