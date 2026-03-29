#include "180_agv_app.h"
#include "agvmodbusmanager.h"
#include "matrixkeymonitor.h"
#include "operationrecorder.h"
#include "mappingconfig.h"
#include <QDebug>

EightyAgvApp::EightyAgvApp(IModbusClient* modbus, IInputDevice* keyboard, OperationRecorder* recorder, MappingConfig* mapping, QObject *parent)
    : QObject(parent)
    , m_modbus(modbus)
    , m_keyboard(keyboard)
    , m_recorder(recorder)
    , m_mapping(mapping)
{
    // 如果是 AGVModbusManager，连接其特有信号
    AGVModbusManager* agvModbus = dynamic_cast<AGVModbusManager*>(m_modbus);
    if (agvModbus) {
        connect(agvModbus, &AGVModbusManager::connected, this, &EightyAgvApp::onAgvConnected);
        connect(agvModbus, &AGVModbusManager::disconnected, this, &EightyAgvApp::onAgvDisconnected);
        connect(agvModbus, &AGVModbusManager::errorOccurred, this, &EightyAgvApp::onAgvError);
        connect(agvModbus, &AGVModbusManager::registerValueChanged, this, &EightyAgvApp::onWordVariableChanged);
    }
}

EightyAgvApp::~EightyAgvApp()
{
}

void EightyAgvApp::requestJog(int direction, bool start)
{
    if (!m_modbus || !m_modbus->isConnected()) return;
    
    // 业务逻辑：根据方向发送 Modbus 命令
    // direction: 0-前, 1-后, 2-左, 3-右
    int address = 100 + direction; 
    m_modbus->writeSingleRegister(address, start ? 1 : 0);
    
    qDebug() << "点动作业: 方向" << direction << "状态" << (start ? "开始" : "停止");
}

void EightyAgvApp::connectToAgv(const QString& ip, int port)
{
    if (m_modbus) {
        m_modbus->connectTo(ip, port);
    }
}

void EightyAgvApp::disconnectAgv()
{
    if (m_modbus) {
        m_modbus->disconnect();
    }
}

bool EightyAgvApp::isAgvConnected() const
{
    return m_modbus && m_modbus->isConnected();
}

void EightyAgvApp::onAgvConnected() {
    emit agvStateChanged(true);
}

void EightyAgvApp::onAgvDisconnected() {
    emit agvStateChanged(false);
}

void EightyAgvApp::onAgvError(const QString& error) {
    emit alarmTriggered(error);
}

void EightyAgvApp::onWordVariableChanged(int address, quint16 value) {
    // 映射逻辑：将寄存器地址映射为语义信号
    if (address == 48) { // 假设 48 是电池1
        emit batteryUpdated(value, -1);
    } else if (address == 52) { // 假设 52 是电池2
        emit batteryUpdated(-1, value);
    }
}

void EightyAgvApp::onBitVariableChanged(int address, int bitPosition, bool value) {
    Q_UNUSED(address);
    Q_UNUSED(bitPosition);
    Q_UNUSED(value);
}
