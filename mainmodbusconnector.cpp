#include "mainmodbusconnector.h"

#include "modbusthreadmanager.h"

MainModbusEndpoint MainModbusConnector::selectEndpoint(bool localSimulatorEnabled,
                                                       bool remoteSimulatorEnabled,
                                                       const QString &remoteSimulatorHost)
{
    MainModbusEndpoint endpoint;
    endpoint.host = QStringLiteral("192.168.1.88");
    endpoint.port = 502;

    if (localSimulatorEnabled) {
        endpoint.host = QStringLiteral("127.0.0.1");
        endpoint.port = 5020;
    } else if (remoteSimulatorEnabled) {
        endpoint.host = remoteSimulatorHost;
        endpoint.port = 5020;
    }

    return endpoint;
}

bool MainModbusConnector::connectAndConfigure(ModbusThreadManager *manager,
                                              const MainModbusEndpoint &endpoint,
                                              int pollIntervalMs,
                                              int reconnectIntervalMs)
{
    if (!manager) {
        return false;
    }

    const bool connected = manager->connectToDevice(endpoint.host, endpoint.port);
    manager->setPollInterval(pollIntervalMs);
    manager->setAutoReconnect(true, reconnectIntervalMs);
    return connected;
}
