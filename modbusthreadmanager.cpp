// file name: modbusthreadmanager.cpp
#include "modbusthreadmanager.h"
#include "techslideredit.h"
#include "techsliderlabel.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

ModbusThreadManager* ModbusThreadManager::instance()
{
    static QThread *workerThread = new QThread();
    static ModbusThreadManager* inst = new ModbusThreadManager();
    static bool initialized = false;

    if (!initialized) {
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
                workerThread->wait();
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
    if (QThread::currentThread() != thread()) {
        bool connected = false;
        QMetaObject::invokeMethod(const_cast<ModbusThreadManager *>(this), [this, &connected]() {
            connected = m_modbusClient ? m_modbusClient->isConnected() : false;
        }, Qt::BlockingQueuedConnection);
        return connected;
    }

    return m_modbusClient ? m_modbusClient->isConnected() : false;
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
    unregisterSlider(m_addressToSlider[address]);
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
    unregisterSliderLabel(m_addressToSliderLabel[address]);
}

void ModbusThreadManager::onRegisterValueChanged(int address, quint16 value)
{
    emit registerValueChanged(address, value);

    if (m_addressToSlider.contains(address)) {
        TechSliderEdit *slider = m_addressToSlider[address];
        if (slider) {
            QMetaObject::invokeMethod(slider, [slider, value]() {
                slider->updateFromModbus(static_cast<double>(value));
            }, Qt::QueuedConnection);
        }
    }

    if (m_addressToSliderLabel.contains(address)) {
        TechSliderLabel *sliderLabel = m_addressToSliderLabel[address];
        if (sliderLabel) {
            QMetaObject::invokeMethod(sliderLabel, [sliderLabel, value]() {
                sliderLabel->updateFromModbus(static_cast<double>(value));
            }, Qt::QueuedConnection);
        }
    }
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
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, address, &value, &ok]() {
            ok = readSingleRegister(address, value);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    Q_UNUSED(value);
    return readHoldingRegisters(address, 1);
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

    bool result = m_modbusClient->writeSingleRegister(address, value);
    if (result) {
        emit registerWritten(address, value);
        emit writeOperationComplete(true, QString("写入地址%1成功").arg(address));
    } else {
        emit writeOperationComplete(false, QString("写入地址%1失败").arg(address));
    }
    return result;
}

bool ModbusThreadManager::readHoldingRegisters(int startAddress, int count)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, startAddress, count, &ok]() {
            ok = readHoldingRegisters(startAddress, count);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取保持寄存器";
        return false;
    }
    return m_modbusClient->readHoldingRegisters(startAddress, count);
}

bool ModbusThreadManager::readInputRegisters(int startAddress, int count)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, startAddress, count, &ok]() {
            ok = readInputRegisters(startAddress, count);
        }, Qt::BlockingQueuedConnection);
        return ok;
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
