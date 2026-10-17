#include "modbusregistercodec.h"

#include <cmath>
#include <limits>

#include <QtTest>

class TestModbusRegisterCodec : public QObject
{
    Q_OBJECT
private slots:
    void floatGoldenOne();
    void floatRoundTrip();
    void doubleGoldenOne();
    void doubleRoundTrip();
};

void TestModbusRegisterCodec::floatGoldenOne()
{
    const auto regs = ModbusRegisterCodec::floatToCdab(1.0f);
    QCOMPARE(regs[0], quint16(0x0000));
    QCOMPARE(regs[1], quint16(0x3F80));
    QCOMPARE(ModbusRegisterCodec::floatFromCdab(regs[0], regs[1]), 1.0f);
}

void TestModbusRegisterCodec::floatRoundTrip()
{
    const float values[] = {
        0.0f,
        -0.0f,
        1.0f,
        -2.5f,
        62.9192f,
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };
    for (float value : values) {
        const auto regs = ModbusRegisterCodec::floatToCdab(value);
        const float back = ModbusRegisterCodec::floatFromCdab(regs[0], regs[1]);
        if (std::isinf(value)) {
            QVERIFY(std::isinf(back));
            QCOMPARE(std::signbit(back), std::signbit(value));
        } else {
            QCOMPARE(back, value);
        }
    }
    const auto nanRegs = ModbusRegisterCodec::floatToCdab(std::numeric_limits<float>::quiet_NaN());
    QVERIFY(std::isnan(ModbusRegisterCodec::floatFromCdab(nanRegs[0], nanRegs[1])));
}

void TestModbusRegisterCodec::doubleGoldenOne()
{
    const auto regs = ModbusRegisterCodec::doubleToGhefCdab(1.0);
    QCOMPARE(regs[0], quint16(0));
    QCOMPARE(regs[1], quint16(0));
    QCOMPARE(regs[2], quint16(0));
    QCOMPARE(regs[3], quint16(0x3FF0));
    QCOMPARE(ModbusRegisterCodec::doubleFromGhefCdab(regs), 1.0);
}

void TestModbusRegisterCodec::doubleRoundTrip()
{
    const double values[] = {
        0.0,
        -0.0,
        1.0,
        -2.5,
        62.9192,
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double value : values) {
        const auto regs = ModbusRegisterCodec::doubleToGhefCdab(value);
        const double back = ModbusRegisterCodec::doubleFromGhefCdab(regs);
        if (std::isinf(value)) {
            QVERIFY(std::isinf(back));
            QCOMPARE(std::signbit(back), std::signbit(value));
        } else {
            QCOMPARE(back, value);
        }
    }
    const auto nanRegs = ModbusRegisterCodec::doubleToGhefCdab(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(std::isnan(ModbusRegisterCodec::doubleFromGhefCdab(nanRegs)));
}

QTEST_APPLESS_MAIN(TestModbusRegisterCodec)
#include "tst_modbusregistercodec.moc"
