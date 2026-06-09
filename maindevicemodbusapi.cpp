#include "maindevicemodbusapi.h"

#include "modbusthreadmanager.h"

bool MainDeviceModbusApi::isReady(const ModbusThreadManager *manager)
{
    return manager && manager->isConnected();
}

bool MainDeviceModbusApi::writeRegister(ModbusThreadManager *manager,
                                        int address,
                                        int value,
                                        QString *errorMessage)
{
    if (!isReady(manager)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("主Modbus未连接");
        }
        return false;
    }
    return manager->writeSingleRegister(address, static_cast<quint16>(value));
}

bool MainDeviceModbusApi::writeRegisters(ModbusThreadManager *manager,
                                         int startAddress,
                                         const QVector<quint16> &values,
                                         QString *errorMessage)
{
    if (!isReady(manager)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("主Modbus未连接");
        }
        return false;
    }
    if (values.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入值为空");
        }
        return false;
    }
    return manager->writeMultipleRegisters(startAddress, values);
}

bool MainDeviceModbusApi::readHoldingRegisters(ModbusThreadManager *manager,
                                               int startAddress,
                                               int count,
                                               QString *errorMessage)
{
    if (!isReady(manager)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("主Modbus未连接");
        }
        return false;
    }
    return manager->readHoldingRegisters(startAddress, count);
}

bool MainDeviceModbusApi::readHoldingRegistersSync(ModbusThreadManager *manager,
                                                   int startAddress,
                                                   int count,
                                                   QVector<quint16> &values,
                                                   QString *errorMessage)
{
    values.clear();
    if (!isReady(manager)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("主Modbus未连接");
        }
        return false;
    }
    return manager->readHoldingRegistersSync(startAddress, count, values);
}

bool MainDeviceModbusApi::readAndDebugAddress(ModbusThreadManager *manager,
                                              int address,
                                              QString *errorMessage)
{
    if (!isReady(manager)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("主Modbus未连接");
        }
        return false;
    }
    manager->readAndDebugAddress(address);
    return true;
}
