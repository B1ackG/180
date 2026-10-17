#include "registerbitutils.h"

#include <QtTest>

class TestRegisterBitUtils : public QObject
{
    Q_OBJECT
private slots:
    void maskedWritePreservesUnrelatedBits();
    void bitUpdatesPreserveUnrelatedBits();
    void repeatedBitUsesLastUpdate();
    void emptyUpdatesKeepCurrent();
    void invalidBitRejectsWholeUpdate();
};

void TestRegisterBitUtils::maskedWritePreservesUnrelatedBits()
{
    quint16 out = 0;
    QVERIFY(RegisterBitUtils::applyMaskedWrite(0xF00F, 0x0002, 0xFFFF, out));
    QCOMPARE(out, quint16(0xF00F | 0x0002));

    QVERIFY(RegisterBitUtils::applyMaskedWrite(0x00F0, 0x0001, 0xFF0F, out));
    QCOMPARE(out, quint16(0x0001));
}

void TestRegisterBitUtils::bitUpdatesPreserveUnrelatedBits()
{
    quint16 out = 0xFFFF;
    QVERIFY(RegisterBitUtils::applyBitUpdates(
        0b1010000000001000,
        {qMakePair(1, true), qMakePair(3, false), qMakePair(15, false)},
        out));
    QCOMPARE(out, quint16(0b0010000000000010));

    QVERIFY(RegisterBitUtils::applyBitUpdates(
        0xA5A5,
        {qMakePair(0, true), qMakePair(8, false)},
        out));
    QCOMPARE(out, quint16((0xA5A5 | 0x0001) & static_cast<quint16>(~0x0100)));
}

void TestRegisterBitUtils::repeatedBitUsesLastUpdate()
{
    quint16 out = 0;
    QVERIFY(RegisterBitUtils::applyBitUpdates(
        0x0001,
        {qMakePair(0, false), qMakePair(0, true)},
        out));
    QCOMPARE(out, quint16(0x0001));
}

void TestRegisterBitUtils::emptyUpdatesKeepCurrent()
{
    quint16 out = 0;
    QVERIFY(RegisterBitUtils::applyBitUpdates(0x1234, {}, out));
    QCOMPARE(out, quint16(0x1234));
}

void TestRegisterBitUtils::invalidBitRejectsWholeUpdate()
{
    quint16 out = 0x1111;
    QVERIFY(!RegisterBitUtils::applyBitUpdates(
        0x2222,
        {qMakePair(1, true), qMakePair(16, true)},
        out));
    QCOMPARE(out, quint16(0x2222));

    QVERIFY(!RegisterBitUtils::applyBitUpdates(0x3333, {qMakePair(-1, false)}, out));
    QCOMPARE(out, quint16(0x3333));
}

QTEST_APPLESS_MAIN(TestRegisterBitUtils)
#include "tst_registerbitutils.moc"
