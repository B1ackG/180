#include "modbusstringregisters.h"

#include <QtTest>

class TestModbusStringRegisters : public QObject
{
    Q_OBJECT
private slots:
    void emptyInput();
    void nullPointer();
    void asciiRoundTrip();
    void chineseUtf8RoundTrip();
    void oddByteCount();
    void truncatesOverflow();
    void truncatesInsideUtf8Character();
    void padsRemainingRegisters();
    void stopsAtFirstZero();
};

void TestModbusStringRegisters::emptyInput()
{
    const QVector<quint16> encoded = encodeUtf8ToRegisters(QString(), 4);
    QCOMPARE(encoded, QVector<quint16>({0, 0, 0, 0}));
    QCOMPARE(decodeUtf8FromRegisters(encoded), QString());
    QCOMPARE(decodeUtf8FromRegisters(nullptr, 0), QString());
}

void TestModbusStringRegisters::nullPointer()
{
    QCOMPARE(decodeUtf8FromRegisters(static_cast<const quint16 *>(nullptr), 3), QString());
    encodeUtf8ToRegisters(QStringLiteral("ABC"), 2, nullptr);
}

void TestModbusStringRegisters::asciiRoundTrip()
{
    const QVector<quint16> encoded = encodeUtf8ToRegisters(QStringLiteral("Hi"), 4);
    QCOMPARE(encoded.at(0), quint16(0x4869));
    QCOMPARE(encoded.at(1), quint16(0));
    QCOMPARE(decodeUtf8FromRegisters(encoded), QStringLiteral("Hi"));
}

void TestModbusStringRegisters::chineseUtf8RoundTrip()
{
    const QString text = QStringLiteral("中文");
    const QVector<quint16> encoded = encodeUtf8ToRegisters(text, 15);
    QCOMPARE(decodeUtf8FromRegisters(encoded), text);
}

void TestModbusStringRegisters::oddByteCount()
{
    const QVector<quint16> encoded = encodeUtf8ToRegisters(QStringLiteral("A"), 2);
    QCOMPARE(encoded.at(0), quint16(0x4100));
    QCOMPARE(encoded.at(1), quint16(0));
    QCOMPARE(decodeUtf8FromRegisters(encoded), QStringLiteral("A"));
}

void TestModbusStringRegisters::truncatesOverflow()
{
    const QString text(40, QLatin1Char('A'));
    const QVector<quint16> encoded = encodeUtf8ToRegisters(text, 1);
    QCOMPARE(encoded.size(), 1);
    QCOMPARE(decodeUtf8FromRegisters(encoded), QString(2, QLatin1Char('A')));
}

void TestModbusStringRegisters::truncatesInsideUtf8Character()
{
    const QString text = QStringLiteral("中文");
    const QByteArray truncated = text.toUtf8().left(2);
    const QVector<quint16> encoded = encodeUtf8ToRegisters(text, 1);
    QCOMPARE(encoded.size(), 1);
    QCOMPARE(decodeUtf8FromRegisters(encoded), QString::fromUtf8(truncated));
}

void TestModbusStringRegisters::padsRemainingRegisters()
{
    const QVector<quint16> encoded = encodeUtf8ToRegisters(QStringLiteral("OK"), 3);
    QCOMPARE(encoded.size(), 3);
    QCOMPARE(encoded.at(2), quint16(0));
    QCOMPARE(decodeUtf8FromRegisters(encoded), QStringLiteral("OK"));
}

void TestModbusStringRegisters::stopsAtFirstZero()
{
    const quint16 regs[] = {0x4100, 0x4200};
    QCOMPARE(decodeUtf8FromRegisters(regs, 2), QStringLiteral("A"));
}

QTEST_APPLESS_MAIN(TestModbusStringRegisters)
#include "tst_modbusstringregisters.moc"
