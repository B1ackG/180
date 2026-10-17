#include "modbuswritegate.h"

#include "../support/configdirguard.h"

#include <QtTest>

class TestModbusWriteGate : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void exemptionsStayClosed();
    void readFailureRejectsOrdinaryWrite();
    void historyTruthTable();
    void deniedMessagePrefix();

private:
    ConfigDirGuard *m_config = nullptr;
};

void TestModbusWriteGate::init()
{
    m_config = new ConfigDirGuard();
    QVERIFY(m_config->isValid());
    m_config->setTeachingDeviceId(7);
    ModbusWriteGate::setInterlockRegisterAddress(8192);
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
}

void TestModbusWriteGate::cleanup()
{
    ModbusWriteGate::setInterlockRegisterAddress(8192);
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
    delete m_config;
    m_config = nullptr;
}

void TestModbusWriteGate::exemptionsStayClosed()
{
    QCOMPARE(ModbusWriteGate::configuredTeachingDeviceId(), quint16(7));
    QVERIFY(ModbusWriteGate::isExemptMainSingleWrite(8192));
    QVERIFY(ModbusWriteGate::isExemptMainSingleWrite(8193));
    QVERIFY(ModbusWriteGate::isExemptMainSingleWrite(8194));
    QVERIFY(!ModbusWriteGate::isExemptMainSingleWrite(8195));
    QVERIFY(ModbusWriteGate::isExemptMainMultipleWrite(8193, 1));
    QVERIFY(!ModbusWriteGate::isExemptMainMultipleWrite(8192, 2));

    QVERIFY(ModbusWriteGate::allowMainDeviceSingleWrite(nullptr, 8192));
    QVERIFY(ModbusWriteGate::allowMainDeviceSingleWrite(nullptr, 8193));
    QVERIFY(ModbusWriteGate::allowMainDeviceSingleWrite(nullptr, 8194));
    QVERIFY(ModbusWriteGate::allowMainDeviceMultipleWrite(nullptr, 8193, 1));
    QVERIFY(ModbusWriteGate::allowMainDeviceMultipleWrite(nullptr, 8193, 2));
    QVERIFY(!ModbusWriteGate::allowMainDeviceMultipleWrite(nullptr, 8192, 2));
    QVERIFY(!ModbusWriteGate::allowMainDeviceMultipleWrite(nullptr, 100, 1));
}

void TestModbusWriteGate::readFailureRejectsOrdinaryWrite()
{
    QVERIFY(!ModbusWriteGate::allowMainDeviceSingleWrite(nullptr, 100));
    QVERIFY(!ModbusWriteGate::allowMainDeviceMultipleWrite(nullptr, 601, 2));
}

void TestModbusWriteGate::historyTruthTable()
{
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 7);
    QVERIFY(!ModbusWriteGate::hasInterlockSnapshot());
    QVERIFY(!ModbusWriteGate::cachedWriteAllowed());
    QVERIFY(ModbusWriteGate::shouldAppendOperationHistoryRecord());

    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(true, 7);
    QVERIFY(ModbusWriteGate::hasInterlockSnapshot());
    QVERIFY(ModbusWriteGate::cachedWriteAllowed());
    QVERIFY(ModbusWriteGate::shouldAppendOperationHistoryRecord());

    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(true, 8);
    QVERIFY(ModbusWriteGate::hasInterlockSnapshot());
    QVERIFY(!ModbusWriteGate::cachedWriteAllowed());
    QVERIFY(!ModbusWriteGate::shouldAppendOperationHistoryRecord());
}

void TestModbusWriteGate::deniedMessagePrefix()
{
    QVERIFY(ModbusWriteGate::messageIndicatesTeachingGateDenied(ModbusWriteGate::deniedReason()));
    QVERIFY(!ModbusWriteGate::messageIndicatesTeachingGateDenied(QStringLiteral("写入地址100失败")));
}

QTEST_APPLESS_MAIN(TestModbusWriteGate)
#include "tst_modbuswritegate.moc"
