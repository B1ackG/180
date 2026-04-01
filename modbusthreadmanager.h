// file name: modbusthreadmanager.h
#ifndef MODBUSTHREADMANAGER_H
/**
 * @file modbusthreadmanager.h
 * @brief Modbus 线程管理器的声明，封装 Modbus 通信线程的启动与调度逻辑。
 *
 * 详细说明: 该文件声明用于管理 Modbus 客户端/服务器通信线程的类，以保证线程安全与消息调度。
 *
 * 使用示例:
 * @code
 * #include "modbusthreadmanager.h"
 * ModbusThreadManager mgr;
 * mgr.start();
 * @endcode
 */
#define MODBUSTHREADMANAGER_H

#include <QObject>
#include <QThread>
#include <QMap>
#include "modbustcpclient.h"

    class TechSliderEdit;  // 前向声明
class TechSliderLabel; // 前向声明

class ModbusThreadManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取 ModbusThreadManager 单例实例
     *
     * 线程本地初始化的单例访问入口，确保多个模块能共享同一管理器实例。
     *
     * @return 指向单例实例的指针
     * @since 1.0.0
     */
    static ModbusThreadManager* instance();

    /**
     * 使用示例:
     * @code
     * auto *mgr = ModbusThreadManager::instance();
     * mgr->connectToDevice("192.168.1.10");
     * @endcode
     */

    // ... 已有的其他方法 ...

    /**
     * @brief 同步读取单个寄存器
     *
     * @param address 寄存器地址
     * @param value 输出参数，用于返回读取到的值
     * @return 成功返回 true
     * @note 该方法可能会阻塞或依赖内部同步实现，生产代码中优先使用异步回调。
     */
    bool readSingleRegister(int address, quint16 &value);

    /**
     * 使用示例:
     * @code
     * quint16 v;
     * if(ModbusThreadManager::instance()->readSingleRegister(1001, v)) {
     *     qDebug() << "value" << v;
     * }
     * @endcode
     */

    /**
     * @brief 读取并在调试输出打印指定地址的值
     * @param address 要读取的寄存器地址
     */
    void readAndDebugAddress(int address);  // 新增：读取并调试输出指定地址的值

    // 连接管理
    /**
     * @brief 连接到 Modbus 设备
     * @param host 设备主机
     * @param port 端口（默认 502）
     * @param slaveId 从站 ID
     * @return 是否成功发起连接
     */
    bool connectToDevice(const QString &host, quint16 port = 502, int slaveId = 1);

    /**
     * 使用示例:
     * @code
    * ModbusThreadManager::instance()->connectToDevice("192.168.1.13", 502);
     * @endcode
     */

    /**
     * @brief 断开与设备的连接并停止轮询
     */
    void disconnectFromDevice();

    /**
     * @brief 查询连接状态
     * @return true 表示已连接
     */
    bool isConnected() const;

    // 寄存器管理 - TechSliderEdit
    /**
     * @brief 将 `TechSliderEdit` 与寄存器地址关联
     * @param slider 指向滑块控件
     * @param address 寄存器地址
     * @note 关联后控件与地址将自动同步读写
     */
    void registerSlider(TechSliderEdit *slider, int address);

    /**
     * 使用示例:
     * @code
     * mgr->registerSlider(mySlider, 1001);
     * @endcode
     */

    /**
     * @brief 注销滑块与地址的关联
     */
    void unregisterSlider(TechSliderEdit *slider);

    /**
     * @brief 根据地址注销滑块关联
     * @param address 要注销的寄存器地址
     */
    void unregisterSlider(int address);

    // 寄存器管理 - TechSliderLabel
    /**
     * @brief 将 `TechSliderLabel` 与寄存器地址关联（只读显示）
     */
    void registerSliderLabel(TechSliderLabel *sliderLabel, int address);

    /**
     * @brief 注销标签与地址的关联
     */
    void unregisterSliderLabel(TechSliderLabel *sliderLabel);

    /**
     * @brief 根据地址注销标签关联
     */
    void unregisterSliderLabel(int address);

    // 配置
    /**
     * @brief 设置轮询间隔（毫秒）
     * @param ms 间隔毫秒数
     */
    void setPollInterval(int ms);

    /**
     * @brief 启用/禁用自动重连
     * @param enable true 启用
     * @param interval 重连间隔（毫秒），默认为 5000ms
     */
    void setAutoReconnect(bool enable, int interval = 5000);

    // 寄存器读写操作
    /**
     * @brief 读取单个寄存器的值（同步）
     * @param address 寄存器地址
     * @return 读取到的值（失败时行为取决于实现，可能返回 0）
     */
    quint16 readSingleRegister(int address);

    /**
     * @brief 读取一段寄存器
     * @param startAddress 起始地址
     * @param count 读取数量
     */
    void readMultipleRegisters(int startAddress, int count);

    // 新增：读取保持寄存器函数（功能码0x03）
    /**
     * @brief 读取保持寄存器（功能码 0x03）
     * @param startAddress 起始地址
     * @param count 读取数量
     * @return 是否成功发出读取请求
     */
    bool readHoldingRegisters(int startAddress, int count);  // 修改返回类型为bool
    /**
     * @brief 读取输入寄存器（功能码 0x04）
     */
    bool readInputRegisters(int startAddress, int count);    // 修改返回类型为bool

    /**
     * @brief 批量读取保持寄存器（内部可能分片）
     */
    void readMultipleHoldingRegisters(int startAddress, int count);

    /**
     * @brief 写单个寄存器
     * @param address 地址
     * @param value 值
     * @return 是否成功发出写请求
     */
    bool writeSingleRegister(int address, quint16 value);  // 新增：写入单个寄存器

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void registerValueChanged(int address, quint16 value);
    void registerWritten(int address, quint16 value);  // 新增：寄存器写入信号
    void writeOperationComplete(bool success, const QString &message);  // 新增：写入操作完成信号

private:
    explicit ModbusThreadManager(QObject *parent = nullptr);
    ~ModbusThreadManager();

    // 单例模式
    // 线程安全单例通过 `instance()` 局部静态变量实现（无需额外成员）

private slots:
    void onRegisterValueChanged(int address, quint16 value);
    void onSliderDestroyed(QObject *obj);
    void onSliderLabelDestroyed(QObject *obj);

private:
    ModbusTCPClient *m_modbusClient;

    // TechSliderEdit 映射表
    QMap<int, TechSliderEdit*> m_addressToSlider;
    QMap<TechSliderEdit*, int> m_sliderToAddress;

    // TechSliderLabel 映射表
    QMap<int, TechSliderLabel*> m_addressToSliderLabel;
    QMap<TechSliderLabel*, int> m_sliderLabelToAddress;
};

#endif // MODBUSTHREADMANAGER_H
