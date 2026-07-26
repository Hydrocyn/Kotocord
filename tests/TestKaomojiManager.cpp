#include <QtTest>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QList>

#include "modules/llm/KaomojiManager.h"
#include "core/DataTypes.h"

class TestKaomojiManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        m_manager = new KaomojiManager(this);
    }

    // 1. 加载有效 JSON 文件
    void testLoadValidJson() {
        QTemporaryFile tmpFile;
        tmpFile.setFileTemplate(QDir::tempPath() + "/kotocord_test_XXXXXX.json");
        QVERIFY(tmpFile.open());

        QJsonObject root;
        QJsonArray joyArr; joyArr.append("(◕‿◕)"); joyArr.append("(｡◕‿◕｡)");
        QJsonArray sadArr; sadArr.append("(╥﹏╥)"); sadArr.append("(｡•́︿•̀｡)");
        QJsonArray angArr; angArr.append("(╬ Ò﹏Ó)");
        QJsonArray surArr; surArr.append("(⊙_⊙)");
        QJsonArray neuArr; neuArr.append("(￣▽￣)");
        root["Joy"] = joyArr;
        root["Sadness"] = sadArr;
        root["Anger"] = angArr;
        root["Surprise"] = surArr;
        root["Neutral"] = neuArr;

        QJsonDocument doc(root);
        tmpFile.write(doc.toJson());
        QString path = tmpFile.fileName();
        tmpFile.flush();
        tmpFile.close();

        QVERIFY(m_manager->loadFromFile(path));

        // 验证每个情绪都有返回值
        QString joy = m_manager->getKaomoji(EmotionType::Joy);
        QVERIFY(!joy.isEmpty());
    }

    // 2. 不存在文件 → 自动生成默认 JSON, 加载后数据可用
    void testLoadNonExistentFile() {
        QVERIFY(m_manager->loadFromFile("/nonexistent/path/kaomoji.json"));

        // 默认数据中 Joy 有 3 个非空颜文字
        QString joy = m_manager->getKaomoji(EmotionType::Joy);
        QVERIFY(!joy.isEmpty());
    }

    // 3. loadFromFile 后, 非 Neutral 的每种情绪都有非空颜文字
    void testAllEmotionsHaveKaomoji() {
        m_manager->loadFromFile("/nonexistent/test_for_all.json");

        QList<EmotionType> nonNeutral = {
            EmotionType::Joy, EmotionType::Sadness,
            EmotionType::Anger, EmotionType::Surprise
        };

        for (auto emotion : nonNeutral) {
            QString kaomoji = m_manager->getKaomoji(emotion);
            QVERIFY2(!kaomoji.isEmpty(),
                     qPrintable(QString("Emotion %1 has empty kaomoji")
                                .arg(static_cast<int>(emotion))));
        }
    }

    // 4. 随机抽取 — 多次调用应能返回不同结果
    void testRandomSelection() {
        m_manager->loadFromFile("/nonexistent/test_random.json");

        QSet<QString> seen;
        for (int i = 0; i < 20; ++i) {
            seen.insert(m_manager->getKaomoji(EmotionType::Joy));
        }
        // 默认数据 Joy 有 3 个条目, 20 次应该能抽到至少 2 个不同的
        QVERIFY2(seen.size() >= 2,
                 qPrintable(QString("Expected ≥2 different kaomoji, got %1").arg(seen.size())));
    }

    // 5. 保存后再用新实例加载 — 数据一致 (非空验证)
    void testSaveAndLoadRoundtrip() {
        m_manager->loadFromFile("/nonexistent/default.json");

        QTemporaryFile tmpFile;
        tmpFile.setFileTemplate(QDir::tempPath() + "/kotocord_save_XXXXXX.json");
        QVERIFY(tmpFile.open());
        QString savePath = tmpFile.fileName();
        tmpFile.close();

        QVERIFY(m_manager->saveToFile(savePath));

        KaomojiManager another;
        QVERIFY(another.loadFromFile(savePath));

        // 验证结构有效 — Joy 应有非空条目
        QString after = another.getKaomoji(EmotionType::Joy);
        QVERIFY(!after.isEmpty());
    }

    // 6. 未加载文件的新实例 — 有构造函数保底数据
    void testFallbackDataInConstructor() {
        KaomojiManager fresh;

        // Neutral 的保底条目为空字符串 (Line 13: m_kaomojiDict[Neutral] = {""})
        QCOMPARE(fresh.getKaomoji(EmotionType::Neutral), QString(""));

        // Joy 的保底条目为非空 (Line 14: m_kaomojiDict[Joy] = {"(*^▽^*)"})
        QCOMPARE(fresh.getKaomoji(EmotionType::Joy), QString("(*^▽^*)"));
    }

private:
    KaomojiManager* m_manager = nullptr;
};

QTEST_MAIN(TestKaomojiManager)
#include "TestKaomojiManager.moc"
