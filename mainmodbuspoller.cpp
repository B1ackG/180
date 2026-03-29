#include "mainmodbuspoller.h"

#include <QTimer>

#include "maindevicemodbusapi.h"
#include "modbusvariables.h"

bool MainModbusPoller::shouldSkipStart(QTimer *pollTimer)
{
    if (pollTimer && pollTimer->isActive()) {
        pollTimer->stop();
    }
    return true;
}

bool MainModbusPoller::shouldSkipPoll()
{
    static bool warned = false;
    if (!warned) {
        warned = true;
    }
    return true;
}

bool MainModbusPoller::pollNextVariable(ModbusThreadManager *manager,
                                        ModbusVariables *variables,
                                        int &currentIndex)
{
    if (!MainDeviceModbusApi::isReady(manager) || !variables) {
        return false;
    }

    const QList<ModbusVariable> allVariables = variables->getAllVariables();
    if (allVariables.isEmpty()) {
        return false;
    }

    if (currentIndex >= allVariables.size()) {
        currentIndex = 0;
    }

    const ModbusVariable &var = allVariables[currentIndex];
    int modbusAddress = 0;
    int bitPos = -1;
    bool polled = false;
    if (ModbusVariables::parseAddress(var.address, modbusAddress, bitPos)) {
        polled = MainDeviceModbusApi::readAndDebugAddress(manager, modbusAddress);
    }

    ++currentIndex;
    return polled;
}
