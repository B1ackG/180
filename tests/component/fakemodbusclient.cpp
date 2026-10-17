#include "fakemodbusclient.h"

FakeModbusClient::FakeModbusClient(QObject *parent)
    : IModbusClient(parent)
{
}

bool FakeModbusClient::addressValid(int address) const
{
    return address >= 0 && address <= 65535;
}

bool FakeModbusClient::consumeReadFailure()
{
    if (m_failReads <= 0) {
        return false;
    }
    --m_failReads;
    return true;
}

bool FakeModbusClient::consumeWriteFailure(int start, int count)
{
    for (int i = 0; i < count; ++i) {
        const int address = start + i;
        const auto it = m_failWrites.find(address);
        if (it == m_failWrites.end() || it.value() <= 0) {
            continue;
        }
        *it -= 1;
        if (it.value() <= 0) {
            m_failWrites.remove(address);
        }
        return true;
    }
    return false;
}

bool FakeModbusClient::connectToServer(const QString &host, quint16 port, int slaveId)
{
    Q_UNUSED(port);
    Q_UNUSED(slaveId);
    if (host.isEmpty()) {
        return false;
    }
    if (!m_connected) {
        m_connected = true;
        emit connected();
    }
    return true;
}

void FakeModbusClient::disconnectFromServer()
{
    if (!m_connected) {
        return;
    }
    m_connected = false;
    emit disconnected();
}

bool FakeModbusClient::isConnected() const
{
    return m_connected;
}

bool FakeModbusClient::readHoldingRegisters(int start, int count)
{
    QVector<quint16> values;
    if (!readHoldingRegistersSync(start, count, values)) {
        return false;
    }
    for (int i = 0; i < values.size(); ++i) {
        emit registerValueChanged(start + i, values.at(i));
    }
    return true;
}

bool FakeModbusClient::readInputRegisters(int start, int count)
{
    return readHoldingRegisters(start, count);
}

bool FakeModbusClient::readHoldingRegisterSync(int address, quint16 &value)
{
    QVector<quint16> values;
    if (!readHoldingRegistersSync(address, 1, values) || values.isEmpty()) {
        return false;
    }
    value = values.first();
    return true;
}

bool FakeModbusClient::readHoldingRegistersSync(int start, int count, QVector<quint16> &values)
{
    values.clear();
    if (!m_connected || count <= 0 || !addressValid(start) || !addressValid(start + count - 1)) {
        return false;
    }
    if (consumeReadFailure()) {
        return false;
    }
    values.resize(count);
    for (int i = 0; i < count; ++i) {
        values[i] = m_registers.value(start + i, 0);
    }
    return true;
}

bool FakeModbusClient::writeSingleRegister(int address, quint16 value)
{
    return writeMultipleRegisters(address, {value});
}

bool FakeModbusClient::writeMultipleRegisters(int start, const QVector<quint16> &values)
{
    if (!m_connected || values.isEmpty() || !addressValid(start)
        || !addressValid(start + values.size() - 1)) {
        return false;
    }
    if (consumeWriteFailure(start, values.size())) {
        return false;
    }
    for (int i = 0; i < values.size(); ++i) {
        m_registers.insert(start + i, values.at(i));
    }
    m_writes.append(qMakePair(start, values));
    return true;
}

void FakeModbusClient::setConnectedForTest(bool connectedState)
{
    if (m_connected == connectedState) {
        return;
    }
    m_connected = connectedState;
    if (connectedState) {
        emit connected();
    } else {
        emit disconnected();
    }
}

void FakeModbusClient::setRegisterForTest(int address, quint16 value)
{
    if (!addressValid(address)) {
        return;
    }
    m_registers.insert(address, value);
}

void FakeModbusClient::failNextRead()
{
    ++m_failReads;
}

void FakeModbusClient::failWriteAtAddress(int address, int times)
{
    if (!addressValid(address) || times <= 0) {
        return;
    }
    m_failWrites[address] += times;
}

QVector<QPair<int, QVector<quint16>>> FakeModbusClient::writes() const
{
    return m_writes;
}

void FakeModbusClient::clearWrites()
{
    m_writes.clear();
}
