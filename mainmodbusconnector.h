#ifndef MAINMODBUSCONNECTOR_H
#define MAINMODBUSCONNECTOR_H

#include <QString>
#include <QtGlobal>

class ModbusThreadManager;

/**
 * @brief 主设备 Modbus 连接目标描述。
 *
 * 该结构体封装主设备连接所需的主机与端口信息，可用于真实设备、
 * 本地仿真器或远程仿真器之间的切换。
 */
struct MainModbusEndpoint
{
    QString host;
    quint16 port = 502;
};

/**
 * @brief 主设备 Modbus 连接与配置辅助类。
 *
 * 负责根据功能开关选择目标端点，并在连接成功后统一配置轮询与自动重连参数。
 * 这使得主界面的启动逻辑更简洁，也便于在不同部署环境之间切换。
 */
class MainModbusConnector
{
public:
    /**
     * @brief 根据运行模式选择主设备连接端点。
     * @param localSimulatorEnabled 是否启用本地仿真器。
     * @param remoteSimulatorEnabled 是否启用远程仿真器。
     * @param remoteSimulatorHost 远程仿真器主机地址。
     * @return 选定的端点信息。
     *
     * 选择规则:
     * - 默认使用生产环境地址 `192.168.1.88:502`
     * - 若启用本地仿真，则切换到 `127.0.0.1:5020`
     * - 若启用远程仿真，则切换到指定主机的 `5020`
     *
     * 使用示例:
     * @code
     * const auto endpoint = MainModbusConnector::selectEndpoint(true, false);
     * @endcode
     */
    static MainModbusEndpoint selectEndpoint(bool localSimulatorEnabled,
                                             bool remoteSimulatorEnabled,
                                             const QString &remoteSimulatorHost = QStringLiteral("192.168.1.70"));

    /**
     * @brief 连接指定端点并应用默认轮询/重连配置。
     * @param manager Modbus 线程管理器。
     * @param endpoint 目标连接端点。
     * @param pollIntervalMs 轮询间隔（毫秒）。
     * @param reconnectIntervalMs 自动重连间隔（毫秒）。
     * @return 连接请求是否成功发起。
     *
     * 使用示例:
     * @code
     * auto endpoint = MainModbusConnector::selectEndpoint(false, false);
     * MainModbusConnector::connectAndConfigure(ModbusThreadManager::instance(),
     *                                          endpoint,
     *                                          500,
     *                                          5000);
     * @endcode
     */
    static bool connectAndConfigure(ModbusThreadManager *manager,
                                     const MainModbusEndpoint &endpoint,
                                     int pollIntervalMs,
                                     int reconnectIntervalMs);
};

#endif // MAINMODBUSCONNECTOR_H
