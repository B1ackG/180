// file name: modbusthreadmanager.cpp
#include "modbusthreadmanager.h"
#include "modbuswritegate.h"
#include "techslideredit.h"
#include "techsliderlabel.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QTimer>

namespace {
bool isGuiThread()
{
    const QCoreApplication *app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

bool sameThread(const QObject *object)
{
    return object && QThread::currentThread() == object->thread();
}
}

ModbusThreadManager* ModbusThreadManager::instance()
{
    static QThread *workerThread = new QThread();
    static ModbusThreadManager* inst = new ModbusThreadManager();
    static bool initialized = false;

    if (!initialized) {
        qRegisterMetaType<ModbusReadRequest>();
        qRegisterMetaType<ModbusWriteCommand>();
        qRegisterMetaType<ModbusResult>();
        qRegisterMetaType<GateDecision>();
        inst->moveToThread(workerThread);
        QObject::connect(workerThread, &QThread::finished,
                         workerThread, &QObject::deleteLater,
                         Qt::UniqueConnection);

        if (QCoreApplication::instance()) {
            QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                             workerThread, []() {
                ModbusThreadManager *inst = ModbusThreadManager::instance();
                QThread *workerThread = inst->thread();
                QMetaObject::invokeMethod(inst, [inst]() {
                    inst->disconnectFromDevice();
                }, Qt::BlockingQueuedConnection);
                workerThread->quit();
                if (!workerThread->wait(3000)) {
                    qWarning() << "主控 Modbus 线程未在 3 秒内退出，禁止 terminate";
                }
            }, Qt::UniqueConnection);
        }

        workerThread->start();
        initialized = true;
    }

    return inst;
}

ModbusThreadManager::ModbusThreadManager(QObject *parent)
    : QObject(parent)
    , m_modbusClient(new ModbusTCPClient(this))
{
    connect(m_modbusClient, &ModbusTCPClient::connected,
            this, &ModbusThreadManager::connected);
    connect(m_modbusClient, &ModbusTCPClient::disconnected,
            this, &ModbusThreadManager::disconnected);
    connect(m_modbusClient, &ModbusTCPClient::errorOccurred,
            this, &ModbusThreadManager::errorOccurred);
    connect(m_modbusClient, &ModbusTCPClient::registerValueChanged,
            this, &ModbusThreadManager::onRegisterValueChanged);
    connect(m_modbusClient, &ModbusTCPClient::connected, this, [this]() {
        m_connectionGeneration.fetch_add(1);
        m_connectedCached.store(true);
    });
    connect(m_modbusClient, &ModbusTCPClient::disconnected, this, [this]() {
        m_connectionGeneration.fetch_add(1);
        m_connectedCached.store(false);
    });

    qDebug() << "Modbus线程管理器已启动";
}

ModbusThreadManager::~ModbusThreadManager()
{
    if (m_modbusClient) {
        m_modbusClient->stopPolling();
        m_modbusClient->disconnectFromServer();
    }
    qDebug() << "Modbus线程管理器已销毁";
}

bool ModbusThreadManager::connectToDevice(const QString &host, quint16 port, int slaveId)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, host, port, slaveId, &ok]() {
            ok = connectToDevice(host, port, slaveId);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    if (!m_modbusClient) {
        return false;
    }

    bool result = m_modbusClient->connectToServer(host, port, slaveId);
    if (result) {
        m_modbusClient->startPolling();
    }
    return result;
}

void ModbusThreadManager::disconnectFromDevice()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { disconnectFromDevice(); }, Qt::BlockingQueuedConnection);
        return;
    }

    if (m_modbusClient) {
        m_modbusClient->stopPolling();
        m_modbusClient->disconnectFromServer();
    }
}

bool ModbusThreadManager::isConnected() const
{
    if (!sameThread(this)) {
        return m_connectedCached.load();
    }

    const bool connected = m_modbusClient && m_modbusClient->isConnected();
    m_connectedCached.store(connected);
    return connected;
}

quint64 ModbusThreadManager::connectionGeneration() const
{
    return m_connectionGeneration.load();
}

void ModbusThreadManager::registerSlider(TechSliderEdit *slider, int address)
{
    if (QThread::currentThread() != thread()) {
        if (slider && slider->thread() == QThread::currentThread()) {
            slider->setModbusAddress(address);
        }
        QMetaObject::invokeMethod(this, [this, slider, address]() {
            registerSlider(slider, address);
        }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!slider || address < 0) {
        return;
    }

    if (m_sliderToAddress.contains(slider)) {
        int oldAddress = m_sliderToAddress[slider];
        m_addressToSlider.remove(oldAddress);
        m_sliderToAddress.remove(slider);
        if (m_modbusClient) {
            m_modbusClient->removeRegisterFromPoll(oldAddress);
        }
    }

    m_addressToSlider[address] = slider;
    m_sliderToAddress[slider] = address;
    if (slider->thread() == QThread::currentThread()) {
        slider->setModbusAddress(address);
    }

    disconnect(this, &ModbusThreadManager::registerValueChanged, slider, nullptr);
    connect(this, &ModbusThreadManager::registerValueChanged, slider,
            [slider, address](int changedAddress, quint16 value) {
                if (!slider || changedAddress != address) {
                    return;
                }
                slider->updateFromModbus(static_cast<double>(value));
            }, Qt::QueuedConnection);

    connect(slider, &QObject::destroyed, this, &ModbusThreadManager::onSliderDestroyed);
}

void ModbusThreadManager::unregisterSlider(TechSliderEdit *slider)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, slider]() { unregisterSlider(slider); }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!slider || !m_sliderToAddress.contains(slider)) {
        return;
    }

    int address = m_sliderToAddress[slider];
    m_addressToSlider.remove(address);
    m_sliderToAddress.remove(slider);

    if (m_modbusClient) {
        m_modbusClient->removeRegisterFromPoll(address);
    }

    disconnect(slider, &QObject::destroyed, this, &ModbusThreadManager::onSliderDestroyed);
}

void ModbusThreadManager::unregisterSlider(int address)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, address]() { unregisterSlider(address); }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!m_addressToSlider.contains(address)) {
        return;
    }
    unregisterSlider(m_addressToSlider.value(address).data());
}

void ModbusThreadManager::registerSliderLabel(TechSliderLabel *sliderLabel, int address)
{
    if (QThread::currentThread() != thread()) {
        if (sliderLabel && sliderLabel->thread() == QThread::currentThread()) {
            sliderLabel->setModbusAddress(address);
        }
        QMetaObject::invokeMethod(this, [this, sliderLabel, address]() {
            registerSliderLabel(sliderLabel, address);
        }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!sliderLabel || address < 0) {
        return;
    }

    if (m_sliderLabelToAddress.contains(sliderLabel)) {
        int oldAddress = m_sliderLabelToAddress[sliderLabel];
        m_addressToSliderLabel.remove(oldAddress);
        m_sliderLabelToAddress.remove(sliderLabel);
        if (m_modbusClient) {
            m_modbusClient->removeRegisterFromPoll(oldAddress);
        }
    }

    m_addressToSliderLabel[address] = sliderLabel;
    m_sliderLabelToAddress[sliderLabel] = address;
    if (sliderLabel->thread() == QThread::currentThread()) {
        sliderLabel->setModbusAddress(address);
    }

    disconnect(this, &ModbusThreadManager::registerValueChanged, sliderLabel, nullptr);
    connect(this, &ModbusThreadManager::registerValueChanged, sliderLabel,
            [sliderLabel, address](int changedAddress, quint16 value) {
                if (!sliderLabel || changedAddress != address) {
                    return;
                }
                sliderLabel->updateFromModbus(static_cast<double>(value));
            }, Qt::QueuedConnection);

    connect(sliderLabel, &QObject::destroyed, this, &ModbusThreadManager::onSliderLabelDestroyed);
}

void ModbusThreadManager::unregisterSliderLabel(TechSliderLabel *sliderLabel)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, sliderLabel]() {
            unregisterSliderLabel(sliderLabel);
        }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!sliderLabel || !m_sliderLabelToAddress.contains(sliderLabel)) {
        return;
    }

    int address = m_sliderLabelToAddress[sliderLabel];
    m_addressToSliderLabel.remove(address);
    m_sliderLabelToAddress.remove(sliderLabel);

    if (m_modbusClient) {
        m_modbusClient->removeRegisterFromPoll(address);
    }

    disconnect(sliderLabel, &QObject::destroyed, this, &ModbusThreadManager::onSliderLabelDestroyed);
}

void ModbusThreadManager::unregisterSliderLabel(int address)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, address]() { unregisterSliderLabel(address); }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!m_addressToSliderLabel.contains(address)) {
        return;
    }
    unregisterSliderLabel(m_addressToSliderLabel.value(address).data());
}

void ModbusThreadManager::onRegisterValueChanged(int address, quint16 value)
{
    emit registerValueChanged(address, value);
    Q_UNUSED(address);
    Q_UNUSED(value);
}

void ModbusThreadManager::onSliderDestroyed(QObject *obj)
{
    TechSliderEdit *slider = static_cast<TechSliderEdit*>(obj);
    if (slider && m_sliderToAddress.contains(slider)) {
        int address = m_sliderToAddress[slider];
        m_addressToSlider.remove(address);
        m_sliderToAddress.remove(slider);
        if (m_modbusClient) {
            m_modbusClient->removeRegisterFromPoll(address);
        }
    }
}

void ModbusThreadManager::onSliderLabelDestroyed(QObject *obj)
{
    TechSliderLabel *sliderLabel = static_cast<TechSliderLabel*>(obj);
    if (sliderLabel && m_sliderLabelToAddress.contains(sliderLabel)) {
        int address = m_sliderLabelToAddress[sliderLabel];
        m_addressToSliderLabel.remove(address);
        m_sliderLabelToAddress.remove(sliderLabel);
        if (m_modbusClient) {
            m_modbusClient->removeRegisterFromPoll(address);
        }
    }
}

void ModbusThreadManager::setPollInterval(int ms)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, ms]() { setPollInterval(ms); }, Qt::BlockingQueuedConnection);
        return;
    }

    if (m_modbusClient) {
        m_modbusClient->setPollInterval(ms);
    }
}

void ModbusThreadManager::setAutoReconnect(bool enable, int interval)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, enable, interval]() {
            setAutoReconnect(enable, interval);
        }, Qt::BlockingQueuedConnection);
        return;
    }

    if (m_modbusClient) {
        m_modbusClient->setAutoReconnect(enable, interval);
    }
}

bool ModbusThreadManager::readSingleRegister(int address, quint16 &value)
{
    if (!sameThread(this)) {
        if (isGuiThread()) {
            qWarning() << "拒绝在 GUI 线程同步读取主控寄存器" << address;
            return false;
        }
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, address, &value, &ok]() {
            ok = readSingleRegister(address, value);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法同步读取地址" << address;
        return false;
    }
    return m_modbusClient->readHoldingRegisterSync(address, value);
}

void ModbusThreadManager::readAndDebugAddress(int address)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, address]() { readAndDebugAddress(address); }, Qt::QueuedConnection);
        return;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取地址" << address;
        return;
    }

    bool result = m_modbusClient->readHoldingRegisters(address, 1);
    qDebug() << "正在读取Modbus地址 &MB" << (address + 1)
             << "（实际地址：" << (40000 + address + 1) << ")"
             << "读取请求状态：" << (result ? "成功" : "失败");
}

quint16 ModbusThreadManager::readSingleRegister(int address)
{
    if (QThread::currentThread() != thread()) {
        quint16 result = 0;
        QMetaObject::invokeMethod(this, [this, address, &result]() {
            result = readSingleRegister(address);
        }, Qt::BlockingQueuedConnection);
        return result;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取地址" << address;
        return 0;
    }

    m_modbusClient->readHoldingRegisters(address, 1);
    qDebug() << "已发送异步读取地址" << address << "的请求";
    return 0;
}

void ModbusThreadManager::readMultipleRegisters(int startAddress, int count)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, startAddress, count]() {
            readMultipleRegisters(startAddress, count);
        }, Qt::QueuedConnection);
        return;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法批量读取输入寄存器";
        return;
    }
    m_modbusClient->readInputRegisters(startAddress, count);
}

bool ModbusThreadManager::writeSingleRegister(int address, quint16 value)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, address, value, &ok]() {
            ok = writeSingleRegister(address, value);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法写入地址" << address;
        emit writeOperationComplete(false, QString("Modbus未连接"));
        return false;
    }

    if (!ModbusWriteGate::allowMainDeviceSingleWrite(address)) {
        emit writeOperationComplete(false, ModbusWriteGate::deniedReason());
        return false;
    }

    bool result = m_modbusClient->writeSingleRegister(address, value);
    if (result) {
        emit registerWritten(address, value);
        emit writeOperationComplete(true, QString("写入地址%1成功").arg(address));
    } else {
        emit writeOperationComplete(false, QString("写入地址%1失败").arg(address));
    }
    return result;
}

bool ModbusThreadManager::writeMultipleRegisters(int startAddress, const QVector<quint16> &values)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, startAddress, values, &ok]() {
            ok = writeMultipleRegisters(startAddress, values);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法批量写入地址" << startAddress;
        emit writeOperationComplete(false, QString("Modbus未连接"));
        return false;
    }

    if (values.isEmpty()) {
        qWarning() << "批量写入值为空，已忽略，起始地址" << startAddress;
        return false;
    }

    if (!ModbusWriteGate::allowMainDeviceMultipleWrite(startAddress, values.size())) {
        emit writeOperationComplete(false, ModbusWriteGate::deniedReason());
        return false;
    }

    bool result = m_modbusClient->writeMultipleRegisters(startAddress, values);
    if (!result) {
        result = true;
        for (int i = 0; i < values.size(); ++i) {
            if (!m_modbusClient->writeSingleRegister(startAddress + i, values.at(i))) {
                result = false;
                break;
            }
        }
    }
    if (result) {
        for (int i = 0; i < values.size(); ++i) {
            emit registerWritten(startAddress + i, values.at(i));
        }
        emit writeOperationComplete(true,
                                    QString("批量写入地址%1~%2成功")
                                        .arg(startAddress)
                                        .arg(startAddress + values.size() - 1));
    } else {
        emit writeOperationComplete(false,
                                    QString("批量写入地址%1失败").arg(startAddress));
    }
    return result;
}

bool ModbusThreadManager::readHoldingRegisters(int startAddress, int count)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, startAddress, count]() {
            readHoldingRegisters(startAddress, count);
        }, Qt::QueuedConnection);
        return true;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取保持寄存器";
        return false;
    }
    return m_modbusClient->readHoldingRegisters(startAddress, count);
}

bool ModbusThreadManager::readHoldingRegistersSync(int startAddress, int count, QVector<quint16> &values)
{
    if (!sameThread(this)) {
        if (isGuiThread()) {
            qWarning() << "拒绝在 GUI 线程同步读取主控寄存器" << startAddress;
            values.clear();
            return false;
        }
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, startAddress, count, &values, &ok]() {
            ok = readHoldingRegistersSync(startAddress, count, values);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    values.clear();
    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法同步读取保持寄存器";
        return false;
    }
    return m_modbusClient->readHoldingRegistersSync(startAddress, count, values);
}

bool ModbusThreadManager::readInputRegisters(int startAddress, int count)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, startAddress, count]() {
            readInputRegisters(startAddress, count);
        }, Qt::QueuedConnection);
        return true;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取输入寄存器";
        return false;
    }
    return m_modbusClient->readInputRegisters(startAddress, count);
}

void ModbusThreadManager::readMultipleHoldingRegisters(int startAddress, int count)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, startAddress, count]() {
            readMultipleHoldingRegisters(startAddress, count);
        }, Qt::QueuedConnection);
        return;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法批量读取保持寄存器";
        return;
    }

    const int maxReadCount = 125;
    for (int i = 0; i < count; i += maxReadCount) {
        int currentStart = startAddress + i;
        int currentCount = qMin(maxReadCount, count - i);
        m_modbusClient->readHoldingRegisters(currentStart, currentCount);
    }
}

void ModbusThreadManager::requestRead(const ModbusReadRequest &request)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, request]() {
            requestRead(request);
        }, Qt::QueuedConnection);
        return;
    }

    ModbusResult result;
    result.id = request.id;
    result.start = request.start;
    result.generation = request.generation;
    const quint64 session = m_connectionGeneration.load();
    if (request.generation != 0 && request.generation != session) {
        result.ok = false;
        result.error = ModbusAsyncError::StaleGeneration;
        emit readFinished(result);
        return;
    }

    QVector<quint16> values;
    const bool ok = readHoldingRegistersSync(request.start, request.count, values);
    if (m_connectionGeneration.load() != session) {
        result.ok = false;
        result.error = ModbusAsyncError::StaleGeneration;
        emit readFinished(result);
        return;
    }
    result.ok = ok;
    result.error = ok ? ModbusAsyncError::None : ModbusAsyncError::IoFailed;
    result.values = values;
    emit readFinished(result);
}

void ModbusThreadManager::requestReadRanges(quint64 id, quint64 generation, const QVector<QPair<int, int>> &ranges)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, id, generation, ranges]() {
            requestReadRanges(id, generation, ranges);
        }, Qt::QueuedConnection);
        return;
    }

    ModbusResult result;
    result.id = id;
    result.generation = generation;
    const quint64 session = m_connectionGeneration.load();
    if (generation != 0 && generation != session) {
        result.ok = false;
        result.error = ModbusAsyncError::StaleGeneration;
        emit readFinished(result);
        return;
    }

    bool ok = true;
    for (const QPair<int, int> &range : ranges) {
        if (m_connectionGeneration.load() != session) {
            ok = false;
            result.error = ModbusAsyncError::StaleGeneration;
            break;
        }
        if (!readHoldingRegisters(range.first, range.second)) {
            ok = false;
            result.error = ModbusAsyncError::IoFailed;
            break;
        }
    }
    result.ok = ok;
    if (ok) {
        result.error = ModbusAsyncError::None;
    }
    emit readFinished(result);
}

void ModbusThreadManager::requestWrite(const ModbusWriteCommand &command)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, command]() {
            requestWrite(command);
        }, Qt::QueuedConnection);
        return;
    }

    ModbusResult result;
    result.id = command.id;
    result.start = command.start;
    result.generation = command.generation;
    if (command.generationChannel != 0
        && m_writeGeneration.value(command.generationChannel) != command.generation) {
        result.ok = false;
        result.error = ModbusAsyncError::StaleGeneration;
        emit writeFinished(result);
        return;
    }

    bool ok = false;
    if (command.values.size() == 1) {
        ok = writeSingleRegister(command.start, command.values.at(0));
    } else if (!command.values.isEmpty()) {
        ok = writeMultipleRegisters(command.start, command.values);
    }
    result.ok = ok;
    result.values = command.values;
    result.error = ok ? ModbusAsyncError::None : ModbusAsyncError::IoFailed;
    emit writeFinished(result);
}

void ModbusThreadManager::requestWriteChain(quint64 id, const QVector<ModbusWriteCommand> &steps)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, id, steps]() {
            requestWriteChain(id, steps);
        }, Qt::QueuedConnection);
        return;
    }

    bool ok = !steps.isEmpty();
    for (const ModbusWriteCommand &step : steps) {
        if (step.values.isEmpty()) {
            ok = false;
            break;
        }
        if (step.values.size() == 1) {
            ok = writeSingleRegister(step.start, step.values.at(0));
        } else {
            ok = writeMultipleRegisters(step.start, step.values);
        }
        if (!ok) {
            break;
        }
    }

    ModbusResult result;
    result.id = id;
    result.ok = ok;
    result.error = ok ? ModbusAsyncError::None : ModbusAsyncError::IoFailed;
    emit writeFinished(result);
}

void ModbusThreadManager::setWriteGeneration(int channel, quint64 generation)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, channel, generation]() {
            setWriteGeneration(channel, generation);
        }, Qt::QueuedConnection);
        return;
    }
    m_writeGeneration.insert(channel, generation);
}

void ModbusThreadManager::requestDelayedWrite(int channel, quint64 generation, int address, quint16 value, int delayMs)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, channel, generation, address, value, delayMs]() {
            requestDelayedWrite(channel, generation, address, value, delayMs);
        }, Qt::QueuedConnection);
        return;
    }

    QTimer::singleShot(qMax(0, delayMs), this, [this, channel, generation, address, value]() {
        if (m_writeGeneration.value(channel) != generation) {
            return;
        }
        writeSingleRegister(address, value);
    });
}

void ModbusThreadManager::queueMaskedWrite(quint64 id, int address, quint16 orMask, quint16 andMask)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, id, address, orMask, andMask]() {
            queueMaskedWrite(id, address, orMask, andMask);
        }, Qt::QueuedConnection);
        return;
    }

    quint16 current = 0;
    if (!readSingleRegister(address, current)) {
        emit maskedWriteFinished(id, address, false, 0);
        return;
    }
    const quint16 next = static_cast<quint16>((current | orMask) & andMask);
    bool ok = true;
    if (next != current) {
        ok = writeSingleRegister(address, next);
    }
    emit maskedWriteFinished(id, address, ok, ok ? next : current);
}

void ModbusThreadManager::authorizeAgvWrite(quint64 commandId)
{
    if (!sameThread(this)) {
        QMetaObject::invokeMethod(this, [this, commandId]() {
            authorizeAgvWrite(commandId);
        }, Qt::QueuedConnection);
        return;
    }

    const bool allowed = ModbusWriteGate::allowAgvWrite(this);
    emit agvGateDecision(commandId, allowed);
}
