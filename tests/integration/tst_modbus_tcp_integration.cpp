#include "modbusregistercodec.h"
#include "modbustcpclient.h"
#include "modbusthreadmanager.h"
#include "modbuswritegate.h"
#include "motionstoprecoverycontroller.h"
#include "motionwriteplan.h"
#include "modbusslave.h"

#include "../support/configdirguard.h"

#include <QCoreApplication>
#include <QThread>
#include <QtTest>

#include <functional>
#include <utility>

// 本测试只证明本机 TCP 软件从站上的读写、顺序、门禁和重连补停。
// 它不能代替 PLC 通讯看门狗，也不能代替实机急停、软限位和制动距离验收。

namespace {
constexpr int kTimeoutMs = 2000;

bool startSlave(ModbusSlave &slave)
{
    if (slave.start(0, 1, QStringLiteral("127.0.0.1")) && slave.serverPort() != 0) {
        return true;
    }
    slave.stop();
    return slave.start(0, 1, QStringLiteral("127.0.0.1")) && slave.serverPort() != 0;
}

}

class TestModbusTcpIntegration : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void readsHoldingRegister();
    void writesSingleAndMultipleRegisters();
    void fixedDelayWriteCompletes();
    void gateMatchMismatchAndReadFailure();
    void sixAxisChainOrder();
    void reconnectWritesStopBeforeNextJog();
    void diagonalPlaneThenJogPressRelease();
    void diagonalReconnectWritesStopBeforeNextJog();

private:
    bool connectManager();
    void onSlave(const std::function<void()> &fn);
    void collectWrites();
    ConfigDirGuard *m_config = nullptr;
    QThread m_slaveThread;
    ModbusSlave *m_slave = nullptr;
    quint16 m_port = 0;
    int m_writeEpoch = 0;
    ModbusTCPClient *m_client = nullptr;
    ModbusThreadManager *m_manager = nullptr;
    QVector<int> m_writeAddresses;
    QVector<quint16> m_writeValues;
};

void TestModbusTcpIntegration::initTestCase()
{
    qRegisterMetaType<ModbusResult>();
    qRegisterMetaType<ModbusWriteCommand>();
    qRegisterMetaType<ModbusReadRequest>();
    qRegisterMetaType<GateDecision>();
    qRegisterMetaType<QVector<QPair<quint16, quint16>>>();
    const QByteArray backend = qgetenv("MODBUS_BACKEND_LIB");
    QVERIFY2(!backend.isEmpty() && QFile::exists(QString::fromLocal8Bit(backend)),
             "MODBUS_BACKEND_LIB 不存在，集成测试不能退化为假客户端");
}

void TestModbusTcpIntegration::onSlave(const std::function<void()> &fn)
{
    QMetaObject::invokeMethod(m_slave, fn, Qt::BlockingQueuedConnection);
}

void TestModbusTcpIntegration::init()
{
    m_config = new ConfigDirGuard();
    QVERIFY(m_config->isValid());
    m_config->setTeachingDeviceId(7);
    m_slave = new ModbusSlave();
    m_slave->moveToThread(&m_slaveThread);
    m_slaveThread.start();
    bool started = false;
    onSlave([this, &started]() {
        started = startSlave(*m_slave);
        m_port = m_slave->serverPort();
        m_slave->setRegister(8192, 7);
    });
    QVERIFY2(started, "动态端口从站启动失败");
    QVERIFY(m_port != 0);
    QVERIFY(m_port != 5020);
    QVERIFY(m_port != 5021);
    m_writeAddresses.clear();
    m_writeValues.clear();
    collectWrites();
    m_client = new ModbusTCPClient();
    m_client->setAutoReconnect(false);
    m_manager = new ModbusThreadManager(m_client, true);
}

void TestModbusTcpIntegration::cleanup()
{
    if (m_manager) {
        m_manager->disconnectFromDevice();
    }
    delete m_manager;
    m_manager = nullptr;
    m_client = nullptr;
    if (m_slave) {
        onSlave([this]() {
            m_slave->disconnectAllClients();
            m_slave->stop();
            m_slave->moveToThread(thread());
        });
    }
    m_slaveThread.quit();
    QVERIFY(m_slaveThread.wait(kTimeoutMs));
    delete m_slave;
    m_slave = nullptr;
    m_port = 0;
    ++m_writeEpoch;
    QCoreApplication::processEvents();
    delete m_config;
    m_config = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    ModbusWriteGate::setInterlockRegisterAddress(8192);
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
}

void TestModbusTcpIntegration::collectWrites()
{
    const int epoch = ++m_writeEpoch;
    QObject::connect(m_slave, &ModbusSlave::registerOperation, this,
                     [this, epoch](quint16 address, quint16 value, const QString &op) {
                         if (epoch != m_writeEpoch || op != QLatin1String("write")) {
                             return;
                         }
                         m_writeAddresses.append(address);
                         m_writeValues.append(value);
                     },
                     Qt::QueuedConnection);
    QObject::connect(m_slave, &ModbusSlave::registersChanged, this,
                     [this, epoch](const QVector<QPair<quint16, quint16>> &ops, const QString &op) {
                         if (epoch != m_writeEpoch || op != QLatin1String("write")) {
                             return;
                         }
                         for (const QPair<quint16, quint16> &item : ops) {
                             m_writeAddresses.append(item.first);
                             m_writeValues.append(item.second);
                         }
                     },
                     Qt::QueuedConnection);
}

bool TestModbusTcpIntegration::connectManager()
{
    return m_manager->connectToDevice(QStringLiteral("127.0.0.1"), m_port, 1);
}

void TestModbusTcpIntegration::readsHoldingRegister()
{
    m_slave->setRegister(100, 0xABCD);
    QVERIFY(connectManager());
    quint16 value = 0;
    QVERIFY(m_manager->readSingleRegister(100, value));
    QCOMPARE(value, quint16(0xABCD));
}

void TestModbusTcpIntegration::writesSingleAndMultipleRegisters()
{
    QVERIFY(connectManager());
    QVERIFY(m_manager->writeSingleRegister(101, 1));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(101), quint16(1), kTimeoutMs);

    QVERIFY(m_manager->writeMultipleRegisters(200, {1, 2, 3}));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(200), quint16(1), kTimeoutMs);
    QCOMPARE(m_slave->getRegister(201), quint16(2));
    QCOMPARE(m_slave->getRegister(202), quint16(3));
    QTRY_VERIFY_WITH_TIMEOUT(m_writeAddresses.contains(101), kTimeoutMs);
    QVERIFY(m_writeAddresses.contains(200));
    QVERIFY(m_writeAddresses.contains(201));
    QVERIFY(m_writeAddresses.contains(202));
}

void TestModbusTcpIntegration::fixedDelayWriteCompletes()
{
    onSlave([this]() { m_slave->setFixedDelayMs(30); });
    QVERIFY(connectManager());
    QVERIFY(m_manager->writeSingleRegister(110, 9));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(110), quint16(9), kTimeoutMs);
    onSlave([this]() { m_slave->setFixedDelayMs(0); });
}

void TestModbusTcpIntegration::gateMatchMismatchAndReadFailure()
{
    QVERIFY(connectManager());
    QVERIFY(m_manager->writeSingleRegister(120, 5));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(120), quint16(5), kTimeoutMs);

    onSlave([this]() { m_slave->setRegister(8192, 1); });
    m_writeAddresses.clear();
    QVERIFY(!m_manager->writeSingleRegister(121, 6));
    QCOMPARE(m_slave->getRegister(121), quint16(0));
    QVERIFY(!m_writeAddresses.contains(121));

    onSlave([this]() {
        m_slave->setRegister(8192, 7);
        m_slave->injectExceptionForFunction(3, 2);
    });
    QVERIFY(m_manager->writeSingleRegister(8192, 7));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(8192), quint16(7), kTimeoutMs);
    QVERIFY(!m_manager->writeSingleRegister(122, 7));
    QCOMPARE(m_slave->getRegister(122), quint16(0));
    onSlave([this]() { m_slave->clearInjectedExceptions(); });
}

void TestModbusTcpIntegration::sixAxisChainOrder()
{
    QVERIFY(connectManager());
    m_manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, 8);
    SixAxisStepWritePlan plan;
    QVERIFY(buildSixAxisStepWritePlan(4, 1.5f, 8, plan));
    m_writeAddresses.clear();
    m_manager->requestWriteChain(30, plan.steps);
    QTRY_VERIFY_WITH_TIMEOUT(m_writeAddresses.contains(615), kTimeoutMs);
    QCoreApplication::processEvents();
    const QVector<int> expectedOrder{614, 601, 602, 615};
    QCOMPARE(m_writeAddresses, expectedOrder);
    const auto regs = ModbusRegisterCodec::floatToCdab(1.5f);
    QCOMPARE(m_slave->getRegister(614), quint16(4));
    QCOMPARE(m_slave->getRegister(601), regs[0]);
    QCOMPARE(m_slave->getRegister(602), regs[1]);
    QCOMPARE(m_slave->getRegister(615), quint16(1));
}

void TestModbusTcpIntegration::reconnectWritesStopBeforeNextJog()
{
    MotionStopRecoveryController controller;
    quint64 nextId = 0;
    controller.setWriteDispatcher([this, &nextId](int address, quint16 value) {
        ModbusWriteCommand command;
        command.id = ++nextId;
        command.start = address;
        command.values = {value};
        m_manager->requestWrite(command);
        return command.id;
    });
    QObject::connect(m_manager, &ModbusThreadManager::connected, &controller, [&controller]() {
        controller.setMainConnected(true);
    }, Qt::QueuedConnection);
    QObject::connect(m_manager, &ModbusThreadManager::disconnected, &controller, [&controller]() {
        controller.setMainConnected(false);
    });
    QObject::connect(m_manager, &ModbusThreadManager::writeFinished, &controller,
                     [&controller](const ModbusResult &result) {
                         controller.handleWriteFinished(result.id, result.ok);
                     },
                     Qt::QueuedConnection);

    QVERIFY(connectManager());
    QTRY_VERIFY_WITH_TIMEOUT(m_manager->isConnected(), kTimeoutMs);
    controller.markMotionStarted(MainMotionStopKind::SixAxisJog613);
    QVERIFY(m_manager->writeSingleRegister(613, 1));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(613), quint16(1), kTimeoutMs);

    QCoreApplication::processEvents();
    m_writeAddresses.clear();
    m_writeValues.clear();
    onSlave([this]() { m_slave->disconnectAllClients(); });
    quint16 ignored = 0;
    QVERIFY(!m_manager->readSingleRegister(8192, ignored));
    QTRY_VERIFY_WITH_TIMEOUT(!m_manager->isConnected(), kTimeoutMs);
    QVERIFY(controller.hasPendingStop());
    QVERIFY(!controller.canStartMotion());
    QVERIFY(!externalJogPressAllowed(&controller));
    m_writeAddresses.clear();
    m_writeValues.clear();
    if (externalJogPressAllowed(&controller)) {
        m_manager->writeSingleRegister(613, 1);
    }
    QCoreApplication::processEvents();
    for (int i = 0; i < m_writeValues.size(); ++i) {
        if (m_writeAddresses.at(i) == 613) {
            QCOMPARE(m_writeValues.at(i), quint16(0));
        }
    }

    QVERIFY(connectManager());
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(613), quint16(0), kTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStartMotion(), kTimeoutMs);
    QVERIFY(externalJogPressAllowed(&controller));
    bool sawStop = false;
    for (int i = 0; i < m_writeAddresses.size(); ++i) {
        if (m_writeAddresses.at(i) != 613) {
            continue;
        }
        QVERIFY2(!sawStop || m_writeValues.at(i) == 0, "停止值确认前出现了新的非零点动写");
        QCOMPARE(m_writeValues.at(i), quint16(0));
        sawStop = true;
    }
    QVERIFY(sawStop);
}

void TestModbusTcpIntegration::diagonalPlaneThenJogPressRelease()
{
    QVERIFY(connectManager());
    m_writeAddresses.clear();
    m_writeValues.clear();
    QVERIFY(m_manager->writeSingleRegister(621, 1));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(621), quint16(1), kTimeoutMs);
    QVERIFY(m_manager->writeSingleRegister(622, static_cast<quint16>(-1)));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(622), quint16(0xFFFF), kTimeoutMs);
    QVERIFY(m_manager->writeSingleRegister(622, 0));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(622), quint16(0), kTimeoutMs);

    const QVector<int> expected{621, 622, 622};
    QTRY_COMPARE_WITH_TIMEOUT(m_writeAddresses.size(), expected.size(), kTimeoutMs);
    QCOMPARE(m_writeAddresses, expected);
    QCOMPARE(m_writeValues.at(0), quint16(1));
    QCOMPARE(m_writeValues.at(1), quint16(0xFFFF));
    QCOMPARE(m_writeValues.at(2), quint16(0));
    QCOMPARE(static_cast<quint16>(-1), quint16(0xFFFF));
}

void TestModbusTcpIntegration::diagonalReconnectWritesStopBeforeNextJog()
{
    MotionStopRecoveryController controller;
    quint64 nextId = 0;
    controller.setWriteDispatcher([this, &nextId](int address, quint16 value) {
        ModbusWriteCommand command;
        command.id = ++nextId;
        command.start = address;
        command.values = {value};
        m_manager->requestWrite(command);
        return command.id;
    });
    QObject::connect(m_manager, &ModbusThreadManager::connected, &controller, [&controller]() {
        controller.setMainConnected(true);
    }, Qt::QueuedConnection);
    QObject::connect(m_manager, &ModbusThreadManager::disconnected, &controller, [&controller]() {
        controller.setMainConnected(false);
    });
    QObject::connect(m_manager, &ModbusThreadManager::writeFinished, &controller,
                     [&controller](const ModbusResult &result) {
                         controller.handleWriteFinished(result.id, result.ok);
                     },
                     Qt::QueuedConnection);

    QVERIFY(connectManager());
    QTRY_VERIFY_WITH_TIMEOUT(m_manager->isConnected(), kTimeoutMs);
    QVERIFY(m_manager->writeSingleRegister(621, 2));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(621), quint16(2), kTimeoutMs);
    controller.markMotionStarted(MainMotionStopKind::SixAxisDiagonalJog622);
    QVERIFY(m_manager->writeSingleRegister(622, 1));
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(622), quint16(1), kTimeoutMs);

    QCoreApplication::processEvents();
    m_writeAddresses.clear();
    m_writeValues.clear();
    onSlave([this]() { m_slave->disconnectAllClients(); });
    quint16 ignored = 0;
    QVERIFY(!m_manager->readSingleRegister(8192, ignored));
    QTRY_VERIFY_WITH_TIMEOUT(!m_manager->isConnected(), kTimeoutMs);
    QVERIFY(controller.hasPendingStop());
    QVERIFY(!externalJogPressAllowed(&controller));

    QVERIFY(connectManager());
    QTRY_COMPARE_WITH_TIMEOUT(m_slave->getRegister(622), quint16(0), kTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStartMotion(), kTimeoutMs);
    bool sawStop = false;
    for (int i = 0; i < m_writeAddresses.size(); ++i) {
        if (m_writeAddresses.at(i) != 622) {
            continue;
        }
        QCOMPARE(m_writeValues.at(i), quint16(0));
        sawStop = true;
    }
    QVERIFY(sawStop);
}

QTEST_MAIN(TestModbusTcpIntegration)
#include "tst_modbus_tcp_integration.moc"
