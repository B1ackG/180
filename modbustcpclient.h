// file name: modbustcpclient.h
#ifndef MODBUSTCPCLIENT_H
/**
 * @file modbustcpclient.h
 * @brief 基于 TCP 的 Modbus 客户端声明，用于与 Modbus TCP 设备进行通信。
 *
 * 详细说明: 提供连接、读写寄存器与异常处理的高层封装，适用于网络环境下的 Modbus 通信。
 *
 * 使用示例:
 * @code
 * #include "modbustcpclient.h"
 * ModbusTcpClient client;
 * client.connectToHost("192.168.0.10", 502);
 * @endcode
 */
#define MODBUSTCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QDateTime>

class ModbusTCPClient : public QObject
{
    Q_OBJECT

public:
    struct ModbusRegister {
        int address;        // 寄存器地址
        quint16 value;      // 寄存器值
        QString name;       // 寄存器名称（可选）
    };

    

    /**
     * @brief 构造 ModbusTCPClient
     *
     * 初始化网络组件、计时器与轮询状态，准备与 Modbus TCP 从站通信。
     *
     * @param parent 父对象。
     * @note 构造不会立即建立连接，需调用 `connectToServer()`。
     * @warning 网络操作涉及阻塞 IO，请在非主线程或使用异步回调处理大量数据。
     * @since 1.0.0
     */
    explicit ModbusTCPClient(QObject *parent = nullptr);

    /**
     * @brief 析构 ModbusTCPClient
     *
     * 关闭活动连接、停止轮询并释放线程/计时器资源。
     */
    ~ModbusTCPClient();

    /**
     * 使用示例:
     * @code
     * ModbusTCPClient *c = new ModbusTCPClient(parent);
     * c->connectToServer("192.168.1.10", 502);
     * c->addRegisterToPoll(100, "battery_level");
     * c->startPolling();
     * @endcode
     */

    // 连接管理
    /**
     * @brief 连接到 Modbus TCP 服务器
     *
     * 建立到指定 `host:port` 的 TCP 连接并设置从站 ID。
     *
     * @param host 目标主机 IP 或域名。
     * @param port 目标端口（通常为 502）。
     * @param slaveId Modbus 从站 ID，默认 1。
     * @return true 表示发起连接成功（不等于已完成握手），连接建立后会发出 `connected()`。
     * @note 若需要 DNS 支持或加密通道，请在实现中扩展。
     */
    bool connectToServer(const QString &host, quint16 port, int slaveId = 1);

    /**
     * 使用示例:
     * @code
     * ModbusTCPClient *c = new ModbusTCPClient(parent);
     * c->connectToServer("192.168.1.10", 502);
     * @endcode
     */

    /**
     * @brief 断开与服务器的连接
     *
     * 立即关闭 socket 并停止自动重连逻辑。
     */
    void disconnectFromServer();

    /**
     * @brief 查询连接状态
     * @return true 表示当前 socket 已连接
     */
    bool isConnected() const;


    // 寄存器操作 - 修改这里
    /**
     * @brief 读取保持寄存器（功能码 0x03）
     *
     * @param startAddress 起始寄存器地址
     * @param count 要读取的寄存器数量
     * @return true 表示请求已发送并进入等待响应阶段，最终结果通过 `registerValueChanged()` 等信号汇报
     * @note 大批量读取建议分片以避免超过设备/协议帧长度限制。
     * @warning count 过大可能导致设备拒绝或通信超时。
     */
    bool readHoldingRegisters(int startAddress, int count);  // 功能码 0x03

    /**
     * 使用示例:
     * @code
     * client->readHoldingRegisters(100, 4);
     * @endcode
     */

    /**
     * @brief 读取输入寄存器（功能码 0x04）
     * @param startAddress 起始地址
     * @param count 读取数量
     * @return 请求是否成功发出
     */
    bool readInputRegisters(int startAddress, int count);    // 功能码 0x04

    /**
     * @brief 写单个寄存器
     *
     * @param address 目标寄存器地址
     * @param value 要写入的 16 位值
     * @return true 表示请求已发送，写入结果以响应或信号反馈
     */
    bool writeSingleRegister(int address, quint16 value);

    /**
     * 使用示例:
     * @code
     * client->writeSingleRegister(200, 123);
     * @endcode
     */

    /**
     * @brief 写多个寄存器
     *
     * @param startAddress 起始地址
     * @param values 要写入的值向量
     * @return true 表示请求已发出
     * @note 对于大量数据会自动分包以符合帧长度限制（实现层）。
     */
    bool writeMultipleRegisters(int startAddress, const QVector<quint16> &values);

    // 批量管理
    /**
     * @brief 将寄存器加入轮询列表
     *
     * @param address 寄存器地址
     * @param name 可选的名称（用于 registerValueChangedNamed 信号）
     * @note 轮询通过定时器触发，轮询间隔由 `setPollInterval()` 控制。
     */
    void addRegisterToPoll(int address, const QString &name = "");

    /**
     * 使用示例:
     * @code
     * client->addRegisterToPoll(100, "battery_level");
     * @endcode
     */

    /**
     * @brief 从轮询列表移除寄存器
     * @param address 寄存器地址
     */
    void removeRegisterFromPoll(int address);

    /**
     * @brief 清空所有轮询寄存器
     */
    void clearPollList();

    // 配置
    /**
     * @brief 设置轮询间隔（毫秒）
     * @param ms 间隔毫秒数
     * @note 较短的间隔增加网络与处理开销，需权衡实时性与性能。
     */
    void setPollInterval(int ms);

    /**
     * 使用示例:
     * @code
     * client->setPollInterval(1000); // 每秒轮询一次
     * @endcode
     */

    /**
     * @brief 配置自动重连
     *
     * @param enable 是否启用
     * @param interval 重连间隔（毫秒），默认 5000
     * @note 可扩展为指数退避策略以避免持续重连。
     */
    void setAutoReconnect(bool enable, int interval = 5000);

    /**
     * 使用示例:
     * @code
     * client->setAutoReconnect(true, 5000);
     * @endcode
     */

    /**
     * @brief 解析单次 Modbus 响应（供测试/调试使用）
     *
     * @param response 原始字节流
     * @param transactionId 对应的事务 ID
     * @note 此方法通常由内部 onReadyRead 调用，外部可用于单元测试。
     */
    void parseSingleResponse(const QByteArray &response, quint16 transactionId);

    /**
     * 使用示例（测试/调试）:
     * @code
     * QByteArray resp = ...;
     * client->parseSingleResponse(resp, 1);
     * @endcode
     */

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void registerValueChanged(int address, quint16 value);
    void registerValueChangedNamed(const QString &name, quint16 value);
    void dataReceived(const QByteArray &data);

public slots:
    /**
     * @brief 开始轮询已注册的寄存器列表
     *
     * 启动内部定时器周期性读取通过 `addRegisterToPoll()` 注册的寄存器。
     *
     * 使用示例:
     * @code
     * client->addRegisterToPoll(100, "battery");
     * client->startPolling();
     * @endcode
     */
    void startPolling();

    /**
     * @brief 停止轮询操作
     *
     * 立即停止定时器并暂停对寄存器的自动读取。
     *
     * 使用示例:
     * @code
     * client->stopPolling();
     * @endcode
     */
    void stopPolling();

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void pollRegisters();
    void tryReconnect();

private:
    // Modbus TCP协议处理
    QByteArray createReadRequest(int startAddress, int count, quint8 functionCode = 0x04);  // 修改这里
    QByteArray createWriteSingleRequest(int address, quint16 value);
    QByteArray createWriteMultipleRequest(int startAddress, const QVector<quint16> &values);

    // 新增私有函数
    bool readRegisters(int startAddress, int count, quint8 functionCode);
    bool parseResponse(const QByteArray &data);
    
    struct TransactionInfo {
        int address;
        qint64 timestamp; // 发送时的时间戳 (ms)
    };

    QMap<quint16, TransactionInfo> m_transactionAddressMap;  // 事务ID -> 事务信息映射
    QByteArray m_responseBuffer;
    int m_maxTimeoutMs; // 事务超时时间 (ms)

    bool parseReadResponse(const QByteArray &data, int startAddress, QVector<quint16> &values);
    bool parseWriteResponse(const QByteArray &data, int expectedAddress);

    // 线程安全操作
    void updateRegisterValue(int address, quint16 value);

private:
    QTcpSocket *m_socket;
    QThread *m_networkThread;
    QMutex m_mutex;

    QString m_host;
    quint16 m_port;
    int m_slaveId;

    bool m_autoReconnect;
    int m_reconnectInterval;
    QTimer *m_reconnectTimer;

    bool m_polling;
    int m_pollInterval;
    QTimer *m_pollTimer;

    QMap<int, ModbusRegister> m_registers;
    QMap<int, QString> m_registerNames;
    QList<int> m_pollList;

    quint16 m_transactionId;
    bool m_connectedState = false; // 追踪连接状态
    QString m_lastSocketError;
    qint64 m_lastSocketErrorMs = 0;
    bool m_transactionMapMismatchLogged = false;
};

#endif // MODBUSTCPCLIENT_H
