#ifndef IMODBUSCLIENT_H
#define IMODBUSCLIENT_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

class IModbusClient : public QObject
{
    Q_OBJECT
public:
    explicit IModbusClient(QObject *parent = nullptr);
    ~IModbusClient() override = default;

    virtual bool connectToServer(const QString &host, quint16 port, int slaveId = 1) = 0;
    virtual void disconnectFromServer() = 0;
    virtual bool isConnected() const = 0;
    virtual bool readHoldingRegisters(int start, int count) = 0;
    virtual bool readInputRegisters(int start, int count) = 0;
    virtual bool readHoldingRegisterSync(int address, quint16 &value) = 0;
    virtual bool readHoldingRegistersSync(int start, int count, QVector<quint16> &values) = 0;
    virtual bool writeSingleRegister(int address, quint16 value) = 0;
    virtual bool writeMultipleRegisters(int start, const QVector<quint16> &values) = 0;
    virtual void startPolling() {}
    virtual void stopPolling() {}
    virtual void removeRegisterFromPoll(int address) { Q_UNUSED(address); }
    virtual void setPollInterval(int ms) { Q_UNUSED(ms); }
    virtual void setAutoReconnect(bool enable, int interval = 5000)
    {
        Q_UNUSED(enable);
        Q_UNUSED(interval);
    }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void registerValueChanged(int address, quint16 value);
};

inline IModbusClient::IModbusClient(QObject *parent)
    : QObject(parent)
{
}

#endif
