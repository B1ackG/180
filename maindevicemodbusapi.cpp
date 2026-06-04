#include "maindevicemodbusapi.h"

#include "featureswitchmanager.h"
#include "modbusthreadmanager.h"

namespace {
bool mainModbusReadEnabled()
{
    return FeatureSwitchManager::instance()->isFeatureEnabled("modbus_main", "modbus_main.read_enabled");
}

bool mainModbusWriteEnabled()
{
    return FeatureSwitchManager::instance()->isFeatureEnabled("modbus_main", "modbus_main.write_enabled");
}
}

bool MainDeviceModbusApi::isReady(const ModbusThreadManager *manager)
{
    return manager && manager->isConnected();
}

bool MainDeviceModbusApi::writeRegister(ModbusThreadManager *manager,
                                        int address,
                                        int value,
                                        QString *errorMessage)
{
    if (!mainModbusWriteEnabled()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Main Modbus 写功能已关闭");
        }
        return false;
    }
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
    if (!mainModbusWriteEnabled()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Main Modbus 写功能已关闭");
        }
        return false;
    }
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
    if (!mainModbusReadEnabled()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Main Modbus 读功能已关闭");
        }
        return false;
    }
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
    if (!mainModbusReadEnabled()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Main Modbus 读功能已关闭");
        }
        return false;
    }
    if (!isReady(manager)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("主Modbus未连接");
        }
        return false;
    }
    manager->readAndDebugAddress(address);
    return true;
}
