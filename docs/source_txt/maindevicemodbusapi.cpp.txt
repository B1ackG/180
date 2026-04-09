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
