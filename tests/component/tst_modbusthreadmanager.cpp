#include "fakemodbusclient.h"
#include "modbusthreadmanager.h"
#include "modbuswritegate.h"
#include "motionwriteplan.h"
#include "modbusasync.h"

#include "../support/configdirguard.h"

#include <QtTest>

namespace {
QVector<int> expandedAddresses(const QVector<QPair<int, QVector<quint16>>> &writes)
{
    QVector<int> addresses;
    for (const QPair<int, QVector<quint16>> &write : writes) {
        for (int i = 0; i < write.second.size(); ++i) {
            addresses.append(write.first + i);
        }
    }
    return addresses;
}
}

class TestModbusThreadManager : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void chainWritesInOrder();
    void chainStopsWhenStepFails();
    void chainStopsWhenGenerationChangesMidway();
    void emptyChainFails();
    void emptyStepFails();
    void staleGenerationWritesNothing();
    void delayedWriteCancelledByGeneration();
    void maskedWriteKeepsOtherBits();
    void gateReadFailureRejectsWrite();
    void gateMismatchRejectsWrite();
    void gateReadFailureRejectsMultipleWrite();
    void gateMismatchRejectsMultipleWrite();
    void gateMatchAllowsWrite();
    void exemptWriteDoesNotNeedGateRead();

private:
    void connectReady(quint16 interlockValue);
    ConfigDirGuard *m_config = nullptr;
    FakeModbusClient *m_client = nullptr;
    ModbusThreadManager *m_manager = nullptr;
};

void TestModbusThreadManager::initTestCase()
{
    qRegisterMetaType<ModbusReadRequest>();
    qRegisterMetaType<ModbusWriteCommand>();
    qRegisterMetaType<ModbusResult>();
    qRegisterMetaType<GateDecision>();
}

void TestModbusThreadManager::init()
{
    m_config = new ConfigDirGuard();
    QVERIFY(m_config->isValid());
    m_config->setTeachingDeviceId(7);
    m_client = new FakeModbusClient();
    m_manager = new ModbusThreadManager(m_client, false);
    QVERIFY(m_manager->connectToDevice(QStringLiteral("127.0.0.1"), 15020, 1));
}

void TestModbusThreadManager::cleanup()
{
    delete m_manager;
    m_manager = nullptr;
    delete m_client;
    m_client = nullptr;
    delete m_config;
    m_config = nullptr;
    ModbusWriteGate::setInterlockRegisterAddress(8192);
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
}

void TestModbusThreadManager::connectReady(quint16 interlockValue)
{
    m_client->setRegisterForTest(8192, interlockValue);
}

void TestModbusThreadManager::chainWritesInOrder()
{
    connectReady(7);
    m_manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, 4);
    SixAxisStepWritePlan plan;
    QVERIFY(buildSixAxisStepWritePlan(2, 1.25f, 4, plan));
    QSignalSpy spy(m_manager, &ModbusThreadManager::writeFinished);
    m_manager->requestWriteChain(11, plan.steps);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ModbusResult>().ok, true);
    QCOMPARE(expandedAddresses(m_client->writes()), QVector<int>({614, 601, 602, 615}));
}

void TestModbusThreadManager::chainStopsWhenStepFails()
{
    connectReady(7);
    m_client->failWriteAtAddress(601, 8);
    m_manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, 4);
    SixAxisStepWritePlan plan;
    QVERIFY(buildSixAxisStepWritePlan(2, 1.25f, 4, plan));
    m_manager->requestWriteChain(12, plan.steps);
    const QVector<int> addresses = expandedAddresses(m_client->writes());
    QVERIFY(addresses.contains(614));
    QVERIFY(!addresses.contains(601));
    QVERIFY(!addresses.contains(602));
    QVERIFY(!addresses.contains(615));
}

void TestModbusThreadManager::chainStopsWhenGenerationChangesMidway()
{
    connectReady(7);
    m_manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, 4);
    SixAxisStepWritePlan plan;
    QVERIFY(buildSixAxisStepWritePlan(2, 1.25f, 4, plan));
    QObject::connect(m_manager, &ModbusThreadManager::registerWritten, m_manager,
                     [this](int address, quint16) {
                         if (address == 614) {
                             m_manager->setWriteGeneration(
                                 ModbusThreadManager::kSixAxisExternalWriteChannel, 5);
                         }
                     });
    QSignalSpy spy(m_manager, &ModbusThreadManager::writeFinished);
    m_manager->requestWriteChain(14, plan.steps);
    QCOMPARE(spy.count(), 1);
    const ModbusResult result = spy.at(0).at(0).value<ModbusResult>();
    QVERIFY(!result.ok);
    QCOMPARE(result.error, ModbusAsyncError::StaleGeneration);
    const QVector<int> addresses = expandedAddresses(m_client->writes());
    QVERIFY(addresses.contains(614));
    QVERIFY(!addresses.contains(601));
    QVERIFY(!addresses.contains(602));
    QVERIFY(!addresses.contains(615));
}

void TestModbusThreadManager::emptyChainFails()
{
    connectReady(7);
    QSignalSpy spy(m_manager, &ModbusThreadManager::writeFinished);
    m_manager->requestWriteChain(15, {});
    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.at(0).at(0).value<ModbusResult>().ok);
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::emptyStepFails()
{
    connectReady(7);
    m_manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, 4);
    ModbusWriteCommand step;
    step.start = 614;
    step.generationChannel = ModbusThreadManager::kSixAxisExternalWriteChannel;
    step.generation = 4;
    QSignalSpy spy(m_manager, &ModbusThreadManager::writeFinished);
    m_manager->requestWriteChain(16, {step});
    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.at(0).at(0).value<ModbusResult>().ok);
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::staleGenerationWritesNothing()
{
    connectReady(7);
    m_manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, 1);
    SixAxisStepWritePlan plan;
    QVERIFY(buildSixAxisStepWritePlan(1, 1.0f, 9, plan));
    QSignalSpy spy(m_manager, &ModbusThreadManager::writeFinished);
    m_manager->requestWriteChain(13, plan.steps);
    QCOMPARE(spy.count(), 1);
    const ModbusResult result = spy.at(0).at(0).value<ModbusResult>();
    QVERIFY(!result.ok);
    QCOMPARE(result.error, ModbusAsyncError::StaleGeneration);
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::delayedWriteCancelledByGeneration()
{
    connectReady(7);
    m_manager->setWriteGeneration(ModbusThreadManager::kRobotExternalWriteChannel, 3);
    m_manager->requestDelayedWrite(ModbusThreadManager::kRobotExternalWriteChannel, 2, 514, 4, 25);
    QTest::qWait(60);
    QVERIFY(m_client->writes().isEmpty());

    m_manager->requestDelayedWrite(ModbusThreadManager::kRobotExternalWriteChannel, 3, 514, 4, 25);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->writes().size(), 1, 500);
    QCOMPARE(m_client->writes().at(0).first, 514);
    QCOMPARE(m_client->writes().at(0).second, QVector<quint16>({4}));
}

void TestModbusThreadManager::maskedWriteKeepsOtherBits()
{
    connectReady(7);
    m_client->setRegisterForTest(615, 0xF00F);
    QSignalSpy spy(m_manager, &ModbusThreadManager::maskedWriteFinished);
    m_manager->queueMaskedWrite(21, 615, 0x0002, 0xFFFF);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(2).toBool(), true);
    QCOMPARE(spy.at(0).at(3).value<quint16>(), quint16(0xF00F | 0x0002));
}

void TestModbusThreadManager::gateReadFailureRejectsWrite()
{
    m_client->failNextRead();
    QVERIFY(!m_manager->writeSingleRegister(100, 1));
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::gateMismatchRejectsWrite()
{
    connectReady(1);
    QVERIFY(!m_manager->writeSingleRegister(100, 1));
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::gateReadFailureRejectsMultipleWrite()
{
    m_client->failNextRead();
    QVERIFY(!m_manager->writeMultipleRegisters(601, {1, 2}));
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::gateMismatchRejectsMultipleWrite()
{
    connectReady(1);
    QVERIFY(!m_manager->writeMultipleRegisters(601, {1, 2}));
    QVERIFY(m_client->writes().isEmpty());
}

void TestModbusThreadManager::gateMatchAllowsWrite()
{
    connectReady(7);
    QVERIFY(m_manager->writeSingleRegister(100, 1));
    QCOMPARE(m_client->writes().size(), 1);
    QCOMPARE(m_client->writes().at(0).first, 100);
    QCOMPARE(m_client->writes().at(0).second, QVector<quint16>({1}));
}

void TestModbusThreadManager::exemptWriteDoesNotNeedGateRead()
{
    m_client->failNextRead();
    QVERIFY(m_manager->writeSingleRegister(8192, 7));
    QCOMPARE(m_client->writes().size(), 1);
    QCOMPARE(m_client->writes().at(0).first, 8192);
}

QTEST_MAIN(TestModbusThreadManager)
#include "tst_modbusthreadmanager.moc"
