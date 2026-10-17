#include "fakemodbusclient.h"
#include "modbusthreadmanager.h"
#include "motionstoprecoverycontroller.h"

#include "../support/configdirguard.h"

#include <QtTest>

// 待停止只存在进程内存中。进程崩溃、掉电或网线断开后，软件测试不能证明 PLC 已停机。
// 真实 PLC 仍需要通讯看门狗，并在台架和实机 SAT 中验证急停、软限位和制动距离。

class TestMotionStopRecovery : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void normalStopClearsPending();
    void disconnectWhileActiveBecomesPending();
    void releaseWhileDisconnectedDoesNotWrite();
    void reconnectWritesZeroBeforeNextMotion();
    void failedStopStaysPendingUntilRetrySucceeds();
    void repeatedConnectionChangesDoNotDuplicateInFlight();
    void bothStopsMustSucceed();
    void diagonalNormalStopClearsPending();
    void diagonalDisconnectBecomesPending();
    void diagonalReconnectWritesZero();
    void diagonalFailedStopRetries();
    void diagonalAndJointStopsAreIndependent();
    void gateFailureKeepsPending();
    void oldDelayedNonZeroWriteIsCancelled();

private:
    ConfigDirGuard *m_config = nullptr;
};

void TestMotionStopRecovery::initTestCase()
{
    qRegisterMetaType<ModbusResult>();
    qRegisterMetaType<ModbusWriteCommand>();
    qRegisterMetaType<ModbusReadRequest>();
    qRegisterMetaType<GateDecision>();
}

void TestMotionStopRecovery::normalStopClearsPending()
{
    MotionStopRecoveryController controller;
    QVector<QPair<int, quint16>> writes;
    quint64 nextId = 0;
    controller.setMainConnected(true);
    controller.setWriteDispatcher([&](int address, quint16 value) {
        writes.append(qMakePair(address, value));
        return ++nextId;
    });
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.requestMotionStop(MainMotionStopKind::SixAxisJog613);
    const QVector<QPair<int, quint16>> expected{qMakePair(613, quint16(0))};
    QCOMPARE(writes, expected);
    QVERIFY(!controller.canStartMotion());
    controller.handleWriteFinished(nextId, true);
    QVERIFY(controller.canStartMotion());
    QVERIFY(!controller.hasPendingStop());
}

void TestMotionStopRecovery::disconnectWhileActiveBecomesPending()
{
    MotionStopRecoveryController controller;
    int calls = 0;
    controller.setWriteDispatcher([&](int, quint16) {
        ++calls;
        return quint64(1);
    });
    controller.setMainConnected(true);
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.setMainConnected(false);
    QCOMPARE(calls, 0);
    QVERIFY(controller.hasPendingStop());
    QVERIFY(!controller.canStartMotion());
}

void TestMotionStopRecovery::releaseWhileDisconnectedDoesNotWrite()
{
    MotionStopRecoveryController controller;
    int calls = 0;
    controller.setWriteDispatcher([&](int, quint16) {
        ++calls;
        return quint64(1);
    });
    controller.setMainConnected(false);
    controller.markMotionStarted(MainMotionStopKind::RobotJog514);
    controller.requestMotionStop(MainMotionStopKind::RobotJog514);
    QCOMPARE(calls, 0);
    QVERIFY(controller.hasPendingStop());
}

void TestMotionStopRecovery::reconnectWritesZeroBeforeNextMotion()
{
    MotionStopRecoveryController controller;
    QVector<quint16> values;
    quint64 nextId = 0;
    controller.setWriteDispatcher([&](int, quint16 value) {
        values.append(value);
        return ++nextId;
    });
    controller.setMainConnected(true);
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.setMainConnected(false);
    controller.setMainConnected(true);
    QCOMPARE(values, QVector<quint16>({0}));
    QVERIFY(!controller.canStartMotion());
    controller.handleWriteFinished(nextId, true);
    QVERIFY(controller.canStartMotion());
}

void TestMotionStopRecovery::failedStopStaysPendingUntilRetrySucceeds()
{
    MotionStopRecoveryController controller;
    int calls = 0;
    quint64 lastId = 0;
    controller.setMainConnected(true);
    controller.setWriteDispatcher([&](int, quint16) {
        ++calls;
        return ++lastId;
    });
    controller.markMotionStarted(MainMotionStopKind::RobotJog514);
    controller.requestMotionStop(MainMotionStopKind::RobotJog514);
    QCOMPARE(calls, 1);
    const quint64 failedId = lastId;
    controller.handleWriteFinished(failedId, false);
    QVERIFY(controller.hasPendingStop());
    QTRY_VERIFY_WITH_TIMEOUT(calls >= 2, 1500);
    QVERIFY(lastId != failedId);
    QVERIFY(!controller.canStartMotion());
    controller.handleWriteFinished(lastId, true);
    QVERIFY(controller.canStartMotion());
    QVERIFY(!controller.hasPendingStop());
}

void TestMotionStopRecovery::repeatedConnectionChangesDoNotDuplicateInFlight()
{
    MotionStopRecoveryController controller;
    int calls = 0;
    quint64 lastId = 0;
    controller.setWriteDispatcher([&](int, quint16) {
        ++calls;
        return ++lastId;
    });
    controller.setMainConnected(true);
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.setMainConnected(false);
    controller.setMainConnected(false);
    controller.setMainConnected(true);
    controller.setMainConnected(true);
    QCOMPARE(calls, 1);
    controller.handleWriteFinished(lastId, true);
    QVERIFY(controller.canStartMotion());
}

void TestMotionStopRecovery::bothStopsMustSucceed()
{
    MotionStopRecoveryController controller;
    QVector<int> addresses;
    QHash<int, quint64> ids;
    quint64 nextId = 0;
    controller.setMainConnected(true);
    controller.setWriteDispatcher([&](int address, quint16) {
        addresses.append(address);
        const quint64 id = ++nextId;
        ids.insert(address, id);
        return id;
    });
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.markMotionStarted(MainMotionStopKind::RobotJog514);
    controller.setMainConnected(false);
    controller.setMainConnected(true);
    QCOMPARE(addresses.size(), 2);
    QVERIFY(addresses.contains(613));
    QVERIFY(addresses.contains(514));
    controller.handleWriteFinished(ids.value(613), true);
    QVERIFY(!controller.canStartMotion());
    controller.handleWriteFinished(ids.value(514), true);
    QVERIFY(controller.canStartMotion());
}

void TestMotionStopRecovery::diagonalNormalStopClearsPending()
{
    MotionStopRecoveryController controller;
    QVector<QPair<int, quint16>> writes;
    quint64 nextId = 0;
    controller.setMainConnected(true);
    controller.setWriteDispatcher([&](int address, quint16 value) {
        writes.append(qMakePair(address, value));
        return ++nextId;
    });
    controller.markMotionStarted(MainMotionStopKind::SixAxisDiagonalJog622);
    controller.requestMotionStop(MainMotionStopKind::SixAxisDiagonalJog622);
    const QVector<QPair<int, quint16>> expected{qMakePair(622, quint16(0))};
    QCOMPARE(writes, expected);
    QVERIFY(!controller.canStartMotion());
    controller.handleWriteFinished(nextId, true);
    QVERIFY(controller.canStartMotion());
    QVERIFY(!controller.hasPendingStop());
}

void TestMotionStopRecovery::diagonalDisconnectBecomesPending()
{
    MotionStopRecoveryController controller;
    int calls = 0;
    controller.setWriteDispatcher([&](int, quint16) {
        ++calls;
        return quint64(1);
    });
    controller.setMainConnected(true);
    controller.markMotionStarted(MainMotionStopKind::SixAxisDiagonalJog622);
    controller.setMainConnected(false);
    QCOMPARE(calls, 0);
    QVERIFY(controller.hasPendingStop());
    QVERIFY(!controller.canStartMotion());
}

void TestMotionStopRecovery::diagonalReconnectWritesZero()
{
    MotionStopRecoveryController controller;
    QVector<QPair<int, quint16>> writes;
    quint64 nextId = 0;
    controller.setWriteDispatcher([&](int address, quint16 value) {
        writes.append(qMakePair(address, value));
        return ++nextId;
    });
    controller.setMainConnected(true);
    controller.markMotionStarted(MainMotionStopKind::SixAxisDiagonalJog622);
    controller.setMainConnected(false);
    controller.setMainConnected(true);
    const QVector<QPair<int, quint16>> expected{qMakePair(622, quint16(0))};
    QCOMPARE(writes, expected);
    QVERIFY(!controller.canStartMotion());
    controller.handleWriteFinished(nextId, true);
    QVERIFY(controller.canStartMotion());
}

void TestMotionStopRecovery::diagonalFailedStopRetries()
{
    MotionStopRecoveryController controller;
    int calls = 0;
    quint64 lastId = 0;
    controller.setMainConnected(true);
    QVector<QPair<int, quint16>> writes;
    controller.setWriteDispatcher([&](int address, quint16 value) {
        writes.append(qMakePair(address, value));
        ++calls;
        return ++lastId;
    });
    controller.markMotionStarted(MainMotionStopKind::SixAxisDiagonalJog622);
    controller.requestMotionStop(MainMotionStopKind::SixAxisDiagonalJog622);
    QCOMPARE(writes.size(), 1);
    QCOMPARE(writes.at(0).first, 622);
    QCOMPARE(writes.at(0).second, quint16(0));
    QCOMPARE(calls, 1);
    const quint64 failedId = lastId;
    controller.handleWriteFinished(failedId, false);
    QVERIFY(controller.hasPendingStop());
    QTRY_VERIFY_WITH_TIMEOUT(calls >= 2, 1500);
    QVERIFY(lastId != failedId);
    controller.handleWriteFinished(lastId, true);
    QVERIFY(controller.canStartMotion());
    QVERIFY(!controller.hasPendingStop());
}

void TestMotionStopRecovery::diagonalAndJointStopsAreIndependent()
{
    MotionStopRecoveryController controller;
    QVector<int> addresses;
    QHash<int, quint64> ids;
    quint64 nextId = 0;
    controller.setMainConnected(true);
    QVector<quint16> values;
    controller.setWriteDispatcher([&](int address, quint16 value) {
        values.append(value);
        addresses.append(address);
        const quint64 id = ++nextId;
        ids.insert(address, id);
        return id;
    });
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.markMotionStarted(MainMotionStopKind::SixAxisDiagonalJog622);
    controller.setMainConnected(false);
    controller.setMainConnected(true);
    QCOMPARE(addresses.size(), 2);
    QCOMPARE(values, QVector<quint16>({0, 0}));
    QVERIFY(addresses.contains(613));
    QVERIFY(addresses.contains(622));
    QVERIFY(!addresses.contains(514));
    controller.handleWriteFinished(ids.value(613), true);
    QVERIFY(!controller.canStartMotion());
    QVERIFY(controller.hasPendingStop());
    controller.handleWriteFinished(ids.value(622), true);
    QVERIFY(controller.canStartMotion());
    QVERIFY(!controller.hasPendingStop());
}

void TestMotionStopRecovery::gateFailureKeepsPending()
{
    ConfigDirGuard config;
    QVERIFY(config.isValid());
    config.setTeachingDeviceId(7);

    FakeModbusClient client;
    ModbusThreadManager manager(&client, false);
    QVERIFY(manager.connectToDevice(QStringLiteral("127.0.0.1"), 15020, 1));
    client.failNextRead();

    MotionStopRecoveryController controller;
    controller.setWriteDispatcher([&](int address, quint16 value) {
        ModbusWriteCommand command;
        command.id = 40;
        command.start = address;
        command.values = {value};
        manager.requestWrite(command);
        return command.id;
    });
    QObject::connect(&manager, &ModbusThreadManager::writeFinished, &controller,
                     [&controller](const ModbusResult &result) {
                         controller.handleWriteFinished(result.id, result.ok);
                     },
                     Qt::QueuedConnection);
    controller.setMainConnected(true);
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    controller.requestMotionStop(MainMotionStopKind::SixAxisJog613);
    QCoreApplication::processEvents();
    QVERIFY(client.writes().isEmpty());
    QVERIFY(controller.hasPendingStop());
    QVERIFY(!controller.canStartMotion());
}

void TestMotionStopRecovery::oldDelayedNonZeroWriteIsCancelled()
{
    ConfigDirGuard config;
    QVERIFY(config.isValid());
    config.setTeachingDeviceId(7);

    FakeModbusClient client;
    ModbusThreadManager manager(&client, false);
    QVERIFY(manager.connectToDevice(QStringLiteral("127.0.0.1"), 15020, 1));
    client.setRegisterForTest(8192, 7);
    manager.setWriteGeneration(ModbusThreadManager::kRobotExternalWriteChannel, 5);
    manager.requestDelayedWrite(ModbusThreadManager::kRobotExternalWriteChannel, 5, 514, 4, 25);
    client.setConnectedForTest(false);
    quint64 robotSeq = 5;
    quint64 sixAxisSeq = 1;
    invalidateExternalMotionWritesOnDisconnect(&manager, robotSeq, sixAxisSeq);
    QTest::qWait(120);
    QVERIFY(client.writes().isEmpty());
}

QTEST_MAIN(TestMotionStopRecovery)
#include "tst_motionstoprecovery.moc"
