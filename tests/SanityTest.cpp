#include <QtTest>
#include <QDebug>

class SanityTest : public QObject {
    Q_OBJECT
private slots:
    void alwaysPasses() {
        qDebug() << "Sanity: inside alwaysPasses";
        QVERIFY(1 + 1 == 2);
    }

    void alsoPasses() {
        qDebug() << "Sanity: inside alsoPasses";
        QCOMPARE(42, 42);
    }
};

QTEST_MAIN(SanityTest)
#include "SanityTest.moc"
