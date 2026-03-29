#ifndef EIGHTY_AGV_APP_H
#define EIGHTY_AGV_APP_H

#include <QObject>
#include <QString>
#include <QVariant>
#include "iinputdevice.h"
#include "imodbusclient.h"

// Forward declarations
class OperationRecorder;
class MappingConfig;

/**
 * @brief 80 系列 AGV 业务聚合入口。
 *
 * 该类把 Modbus 通信、输入设备、操作记录与映射配置封装到一个业务层对象中，
 * 便于主窗口只关注“调用哪个业务动作”，而不直接操纵底层协议细节。
 *
 * 使用示例:
 * @code
 * EightyAgvApp app(modbusClient, inputDevice, recorder, mappingConfig);
 * app.connectToAgv("192.168.1.100", 502);
 * app.requestJog(0, true);
 * @endcode
 */
class EightyAgvApp : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造 AGV 业务对象。
     * @param modbus Modbus 客户端接口实现。
     * @param keyboard 输入设备接口实现。
     * @param recorder 操作记录器。
     * @param mapping 控件/操作映射配置。
     * @param parent 父对象。
     */
    explicit EightyAgvApp(IModbusClient* modbus, IInputDevice* keyboard, OperationRecorder* recorder, MappingConfig* mapping, QObject *parent = nullptr);

    /**
     * @brief 析构函数。
     */
    virtual ~EightyAgvApp();

    /**
     * @brief 获取输入设备接口。
     */
    IInputDevice* keyboard() const { return m_keyboard; }

    /**
     * @brief 获取 Modbus 接口。
     */
    IModbusClient* modbus() const { return m_modbus; }

    /**
     * @brief 获取操作记录器。
     */
    OperationRecorder* recorder() const { return m_recorder; }

    /**
     * @brief 获取映射配置。
     */
    MappingConfig* mapping() const { return m_mapping; }

    /**
     * @brief 发送点动请求。
     * @param direction 方向编号（实现中按业务约定映射）。
     * @param start true 表示启动点动，false 表示停止。
     *
     * 使用示例:
     * @code
     * app.requestJog(0, true);   // 前进开始
     * app.requestJog(0, false);  // 前进停止
     * @endcode
     */
    void requestJog(int direction, bool start);

    /**
     * @brief 连接到 AGV 设备。
     * @param ip 目标 IP。
     * @param port 目标端口。
     */
    void connectToAgv(const QString& ip, int port);

    /**
     * @brief 断开 AGV 连接。
     */
    void disconnectAgv();

    /**
     * @brief 查询 AGV 是否已连接。
     * @return 已连接返回 true。
     */
    bool isAgvConnected() const;

signals:
    /**
     * @brief 电池电量更新信号。
     * @param battery1 电池 1 电量。
     * @param battery2 电池 2 电量。
     */
    void batteryUpdated(int battery1, int battery2);

    /**
     * @brief 速度更新信号。
     * @param speed 当前速度。
     */
    void speedUpdated(int speed);

    /**
     * @brief 告警触发信号。
     * @param message 告警内容。
     */
    void alarmTriggered(const QString &message);

    /**
     * @brief AGV 连接状态变化信号。
     * @param connected 当前连接状态。
     */
    void agvStateChanged(bool connected);

private slots:
    /**
     * @brief AGV 连接成功后的内部回调。
     */
    void onAgvConnected();

    /**
     * @brief AGV 断开连接后的内部回调。
     */
    void onAgvDisconnected();

    /**
     * @brief AGV 通信错误后的内部回调。
     * @param error 错误文本。
     */
    void onAgvError(const QString& error);

    /**
     * @brief 处理寄存器值变化。
     * @param address 寄存器地址。
     * @param value 新值。
     */
    void onWordVariableChanged(int address, quint16 value);

    /**
     * @brief 处理位变量变化。
     * @param address 寄存器地址。
     * @param bitPosition 位位置。
     * @param value 位状态。
     */
    void onBitVariableChanged(int address, int bitPosition, bool value);

private:
    IModbusClient* m_modbus;
    IInputDevice* m_keyboard;
    OperationRecorder* m_recorder;
    MappingConfig* m_mapping;
};

#endif // EIGHTY_AGV_APP_H
