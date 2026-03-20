// file name: modbusthreadmanager.cpp
#include "modbusthreadmanager.h"
#include "techslideredit.h"
#include "techsliderlabel.h"
#include <QDebug>

ModbusThreadManager* ModbusThreadManager::instance()
{
    static ModbusThreadManager* inst = []() {
        return new ModbusThreadManager();
    }();
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
    if (m_modbusClient) {
        m_modbusClient->stopPolling();
        m_modbusClient->disconnectFromServer();
    }
}

bool ModbusThreadManager::isConnected() const
{
    return m_modbusClient ? m_modbusClient->isConnected() : false;
}

void ModbusThreadManager::registerSlider(TechSliderEdit *slider, int address)
{
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
    slider->setModbusAddress(address);

    connect(slider, &QObject::destroyed, this, &ModbusThreadManager::onSliderDestroyed);

    if (m_modbusClient) {
        QString sliderName = QString("%1_%2").arg(slider->labelText()).arg((quintptr)slider);
        m_modbusClient->addRegisterToPoll(address, sliderName);
    }
}

void ModbusThreadManager::unregisterSlider(TechSliderEdit *slider)
{
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
    if (!m_addressToSlider.contains(address)) {
        return;
    }
    unregisterSlider(m_addressToSlider[address]);
}

void ModbusThreadManager::registerSliderLabel(TechSliderLabel *sliderLabel, int address)
{
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
    sliderLabel->setModbusAddress(address);

    connect(sliderLabel, &QObject::destroyed, this, &ModbusThreadManager::onSliderLabelDestroyed);

    if (m_modbusClient) {
        QString sliderName = QString("%1_%2").arg(sliderLabel->labelText()).arg((quintptr)sliderLabel);
        m_modbusClient->addRegisterToPoll(address, sliderName);
    }
}

void ModbusThreadManager::unregisterSliderLabel(TechSliderLabel *sliderLabel)
{
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
            slider->updateFromModbus(static_cast<double>(value));
        }
    }

    if (m_addressToSliderLabel.contains(address)) {
        TechSliderLabel *sliderLabel = m_addressToSliderLabel[address];
        if (sliderLabel) {
            sliderLabel->updateFromModbus(static_cast<double>(value));
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
    if (m_modbusClient) {
        m_modbusClient->setPollInterval(ms);
    }
}

void ModbusThreadManager::setAutoReconnect(bool enable, int interval)
{
    if (m_modbusClient) {
        m_modbusClient->setAutoReconnect(enable, interval);
    }
}

bool ModbusThreadManager::readSingleRegister(int address, quint16 &value)
{
    Q_UNUSED(value);
    return readHoldingRegisters(address, 1);
}

void ModbusThreadManager::readAndDebugAddress(int address)
{
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
    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法批量读取输入寄存器";
        return;
    }
    m_modbusClient->readInputRegisters(startAddress, count);
}

bool ModbusThreadManager::writeSingleRegister(int address, quint16 value)
{
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
    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取保持寄存器";
        return false;
    }
    return m_modbusClient->readHoldingRegisters(startAddress, count);
}

bool ModbusThreadManager::readInputRegisters(int startAddress, int count)
{
    if (!m_modbusClient || !m_modbusClient->isConnected()) {
        qWarning() << "Modbus客户端未连接，无法读取输入寄存器";
        return false;
    }
    return m_modbusClient->readInputRegisters(startAddress, count);
}

void ModbusThreadManager::readMultipleHoldingRegisters(int startAddress, int count)
{
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
