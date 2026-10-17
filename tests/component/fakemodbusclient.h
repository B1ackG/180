#ifndef FAKEMODBUSCLIENT_H
#define FAKEMODBUSCLIENT_H

#include "imodbusclient.h"

#include <QHash>
#include <QPair>
#include <QVector>

class FakeModbusClient : public IModbusClient
{
public:
    explicit FakeModbusClient(QObject *parent = nullptr);

    bool connectToServer(const QString &host, quint16 port, int slaveId = 1) override;
    void disconnectFromServer() override;
    bool isConnected() const override;
    bool readHoldingRegisters(int start, int count) override;
    bool readInputRegisters(int start, int count) override;
    bool readHoldingRegisterSync(int address, quint16 &value) override;
    bool readHoldingRegistersSync(int start, int count, QVector<quint16> &values) override;
    bool writeSingleRegister(int address, quint16 value) override;
    bool writeMultipleRegisters(int start, const QVector<quint16> &values) override;

    void setConnectedForTest(bool connected);
    void setRegisterForTest(int address, quint16 value);
    void failNextRead();
    void failWriteAtAddress(int address, int times = 1);
    QVector<QPair<int, QVector<quint16>>> writes() const;
    void clearWrites();

private:
    bool addressValid(int address) const;
    bool consumeReadFailure();
    bool consumeWriteFailure(int start, int count);

    bool m_connected = false;
    int m_failReads = 0;
    QHash<int, quint16> m_registers;
    QHash<int, int> m_failWrites;
    QVector<QPair<int, QVector<quint16>>> m_writes;
};

#endif
