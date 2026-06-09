// file name: agvmodbusmanager.h
#ifndef AGVMODBUSMANAGER_H
/**
 * @file agvmodbusmanager.h
 * @brief AGV 专用的 Modbus 管理器声明，负责与底层设备的通讯协调。
 *
 * 详细说明: 封装对 AGV 设备特定寄存器和命令的读写逻辑，提供高层调用接口。
 *
 * 使用示例:
 * @code
 * #include "agvmodbusmanager.h"
 * AGVModbusManager mgr;
 * mgr.connectToDevice("192.168.1.10");
 * @endcode
 */
#define AGVMODBUSMANAGER_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QMap>
#include <QListWidget>
#include <QDateTime>
#include <QSet>
#include <QLibrary>

/*位变量（BOOL）：

    前触边 -> 对象名: label_front_touch

    后触边 -> 对象名: label_back_touch

    左触边 -> 对象名: label_left_touch

    右触边 -> 对象名: label_right_touch

    前避障减速 -> 对象名: label_front_slow

    前避障停止 -> 对象名: label_front_stop

    后避障减速 -> 对象名: label_back_slow

    后避障停止 -> 对象名: label_back_stop

    点动运行中 -> 对象名: label_jog_running

    转向轮回正 -> 不显示

    横移模式转向轮到位 -> 不显示

    原地旋转模式转向轮到位 -> 不显示

    低电报警 -> 不单独显示，合并到故障显示

    通讯故障 -> 不单独显示，合并到故障显示

    驱动故障 -> 不单独显示，合并到故障显示

字变量（INT）：

    电池1电量 -> 进度条: progressBar_battery1, 标签: label_battery1_text

    电池2电量 -> 标签: label_battery2_text

    行驶速度 -> 标签: label_speed

    点动位移 -> 标签: label_jog_displacement

    转向1故障代码 -> 通过故障代码列表显示

    转向2故障代码 -> 通过故障代码列表显示

    转向3故障代码 -> 通过故障代码列表显示

    转向4故障代码 -> 通过故障代码列表显示

    行走1故障代码 -> 通过故障代码列表显示

    行走2故障代码 -> 通过故障代码列表显示

    行走3故障代码 -> 通过故障代码列表显示

    行走4故障代码 -> 通过故障代码列表显示*/

class AGVModbusManager : public QObject
{
    Q_OBJECT

public:
    /**
     * 功能: 构造 AGVModbusManager，用于管理 AGV 专用的 Modbus 通信与数据处理。
     * 如何使用: 在应用初始化阶段创建该管理对象并连接相应信号以接收更新。
     * 如何修改: 若需要支持多个 AGV，可将其改造成工厂/管理容器而非单例式全局对象。
     */
    explicit AGVModbusManager(QObject *parent = nullptr);

    // 启停专用线程（对象自身会迁移到该线程中运行）
    bool startWorkerThread();
    void stopWorkerThread();

    /**
     * 功能: 析构函数，清理 socket、线程和计时器等资源。
     * 如何使用: 由 Qt 对象树自动销毁或手动 delete。
     * 如何修改: 增加资源时在析构中同步释放并确保线程安全。
     */
    ~AGVModbusManager();

    /**
     * 使用示例:
     * @code
     * auto *mgr = new AGVModbusManager(qApp);
     * mgr->setAutoReconnect(true, 3000);
     * mgr->connectToDevice("192.168.1.88", 502);
     * connect(mgr, &AGVModbusManager::registerValueChanged, [](int addr, quint16 v){ qDebug() << addr << v; });
     * @endcode
     */

    // 连接管理
    /**
     * 功能: 连接到 AGV 的 Modbus 设备（默认 IP/端口可被覆盖）。
     * 如何使用: 调用并监听 connected()/errorOccurred() 信号以获取连接结果。
     * 如何修改: 若需通过 DHCP 或多地址尝试连接，可在实现中增加尝试逻辑。
     */
    bool connectToDevice(const QString &host = "192.168.1.88", quint16 port = 502);

    /**
     * 使用示例:
     * @code
     * AGVModbusManager mgr;
    * mgr.connectToDevice("192.168.1.88", 502);
     * @endcode
     */

    /**
     * 功能: 主动断开与设备的连接并停止轮询。
     * 如何使用: 在应用退出或停止通信时调用。
     * 如何修改: 若需在断开前进行缓冲发送或状态同步，可在实现中添加流程。
     */
    void disconnectFromDevice();

    /**
     * 功能: 返回当前网络连接状态。
     * 如何使用: 用于 UI 指示或决定是否发起读写操作。
     * 如何修改: 若支持多设备，应提供设备级别的状态查询接口。
     */
    bool isConnected() const;

    // 配置
    /**
     * 功能: 设置寄存器轮询间隔（毫秒）。
     * 如何使用: 在连接或初始化时调用以控制数据刷新频率。
     * 如何修改: 可支持按类型或优先级的细粒度间隔配置。
     */
    void setPollInterval(int ms);    // 设置轮询间隔
    int pollInterval() const { return m_pollInterval; } // 获取轮询间隔

    /**
     * 功能: 启用或禁用自动重连并设置重连间隔。
     * 如何使用: 在不稳定网络环境下启用以保持连接可用性。
     * 如何修改: 可实现指数退避等更复杂的重连策略。
     */
    void setAutoReconnect(bool enable, int interval = 5000);

    // 批量读取寄存器
    /**
     * 功能: 读取一段寄存器并触发相应的处理与信号。
     * 如何使用: 提供起始地址与数量以批量获取数据。
     * 如何修改: 对大数量读取请分片或增加并发控制以兼容设备限制。
     */
    void readMultipleRegisters(int startAddress, int count);

    /**
     * @brief 同步读取一段保持寄存器
     */
    bool readHoldingRegistersSync(int startAddress, int count, QVector<quint16> &values);

    /**
     * 使用示例:
     * @code
     * mgr.readMultipleRegisters(100, 10);
     * @endcode
     */

    /**
     * 功能: 测试用：发送进度条更新信号（用于调试/演示）。
     * 如何使用: 在调试或 UI 演示时调用。
     * 如何修改: 仅用于测试，生产代码可移除或受条件编译控制。
     */
    void testUpdateProgressBar(const QString &name, int value) {
        emit updateProgressBar(name, value);
    }
    void testUpdateStatusLabel(const QString &name, const QString &text) {
        emit updateStatusLabel(name, text);
    }

    // 添加写入功能
    /**
     * 功能: 写单个寄存器到 AGV 设备。
     * 如何使用: 提供地址和值，调用后观察 writeCompleted 或 errorOccurred 信号以获取结果。
     * 如何修改: 若需事务进行多寄存器写入，请使用批量写接口并加入确认机制。
     */
    bool writeSingleRegister(int address, quint16 value);

    /**
     * 功能: 从起始地址连续写入多个保持寄存器（FC16）。
     * 如何使用: 传入起始地址与 16 位字序列；若动态库支持则一次写入，否则退化为多次单寄存器写。
     */
    bool writeMultipleRegisters(int startAddress, const QVector<quint16> &values);

signals:
    // 状态信号
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

    // 数据更新信号
    void registerValueChanged(int address, quint16 value);

    // AGV专用信号
    void bitVariableChanged(int address, int bitPosition, bool value);
    void wordVariableChanged(int address, quint16 value);

    // UI更新信号
    void updateFaultsLabel(const QString &faultText);
    void updateProgressBar(const QString &name, int value);
    void updateStatusLabel(const QString &name, const QString &text);
    void addFaultCodeToList(const QString &faultCode);
    void clearFaultCodes();

    // 心跳信号（用于调试）
    void heartbeatReceived();
    // 添加写入完成信号
    void writeCompleted(int address, quint16 value, bool success);

    /** 因示教端写门禁导致 AGV 写被拒绝（用于 UI 弹窗提示） */
    void teachingWriteGateDenied();


private slots:
    void pollRegisters();
    void tryReconnect();

private:
    void handleCommunicationFailure(const QString &reason);

    // 动态库后端（可选）：启用后替代手写 Modbus TCP 帧处理
    bool ensureDynamicBackendLoaded();
    void unloadDynamicBackend();

    using MbCreateHandleFn = void* (*)();
    using MbDestroyHandleFn = void (*)(void*);
    using MbConnectFn = int (*)(void*, const char*, int, int);
    using MbDisconnectFn = void (*)(void*);
    using MbIsConnectedFn = int (*)(void*);
    using MbReadRegistersFn = int (*)(void*, int, int, quint16*, int);
    using MbWriteSingleFn = int (*)(void*, int, quint16);
    using MbWriteMultipleFn = int (*)(void*, int, const quint16*, int);

    // 数据处理
    void processBitVariables(int address, quint16 value);
    void processWordVariables(int address, quint16 value);

    // 状态管理
    void updateConnectionStatus(bool connected);
    QString getBitVariableName(int address, int bitPos) const;
    QString getWordVariableName(int address) const;

    // 故障处理
    void updateFaultsDisplay();
    void updateFaultCodesDisplay();
private:
    QThread *m_networkThread;
    QMutex m_mutex;

    QString m_host;
    quint16 m_port;

    bool m_autoReconnect;
    int m_reconnectInterval;
    QTimer *m_reconnectTimer;

    QTimer *m_pollTimer;
    int m_pollInterval;

    // 数据存储
    QMap<int, quint16> m_registerValues;  // 地址 -> 值

    // 位变量状态
    QMap<int, QMap<int, bool>> m_bitStates;  // 地址 -> {位位置 -> 状态}

    // 故障状态
    struct FaultStatus {
        bool lowBattery = false;     // 低电报警
        bool commFault = false;      // 通讯故障
        bool driveFault = false;     // 驱动故障
    } m_faultStatus;

    // 故障代码
    QMap<int, quint16> m_faultCodes;  // 地址 -> 故障代码

    // 心跳检测
    bool m_lastHeartbeatState = false;
    QDateTime m_lastHeartbeatTime;

    bool m_connectedState = false;
    QSet<int> m_disconnectedWriteWarnedAddresses;
    QString m_lastSocketError;
    bool m_writesEnabled = true; // 控制是否允许向AGV写入；默认开启，避免正常控制被静默拦截

    QLibrary m_dynamicBackendLibrary;
    QString m_dynamicBackendPath;
    void *m_dynamicBackendHandle = nullptr;
    bool m_useDynamicBackend = false;
    bool m_dynamicBackendLoadAttempted = false;
    QString m_lastDynamicBackendError;
    MbCreateHandleFn m_backendCreate = nullptr;
    MbDestroyHandleFn m_backendDestroy = nullptr;
    MbConnectFn m_backendConnect = nullptr;
    MbDisconnectFn m_backendDisconnect = nullptr;
    MbIsConnectedFn m_backendIsConnected = nullptr;
    MbReadRegistersFn m_backendReadHolding = nullptr;
    MbReadRegistersFn m_backendReadInput = nullptr;
    MbWriteSingleFn m_backendWriteSingle = nullptr;
    MbWriteMultipleFn m_backendWriteMultiple = nullptr;

public:
    // 运行时控制写入开关
    void setWritesEnabled(bool enabled);
    bool writesEnabled() const;
};

#endif // AGVMODBUSMANAGER_H
