#ifndef IMODBUSCLIENT_H
/**
 * @file imodbusclient.h
 * @brief Modbus 客户端抽象接口，统一主设备和 AGV 设备的通信能力。
 *
 * 该接口用于解耦上层业务逻辑与具体通信实现。上层代码只依赖该接口，
 * 便于在真实 Modbus 客户端、仿真客户端或测试 Mock 之间切换。
 *
 * 使用示例:
 * @code
 * class MockModbusClient : public IModbusClient {
 * public:
 *     bool connectTo(const QString&, int) override { return true; }
 *     void disconnect() override {}
 *     bool isConnected() const override { return true; }
 *     bool readHoldingRegisters(int, int) override { return true; }
 *     bool writeSingleRegister(int, quint16) override { return true; }
 * };
 *
 * MockModbusClient client;
 * client.connectTo("127.0.0.1", 5020);
 * @endcode
 */
#define IMODBUSCLIENT_H

#include <QString>

/**
 * @brief Modbus 通信客户端接口。
 *
 * 实现类需要提供连接、断开、读取保持寄存器和写单寄存器能力。
 * 推荐在 UI、业务层、自动化测试中统一依赖该接口，而不是直接依赖
 * 某个具体的 TCP 客户端实现。
 */
class IModbusClient {
public:
    /**
     * @brief 析构函数。
     *
     * 通过基类指针销毁派生类对象时必须为虚析构。
     */
    virtual ~IModbusClient() = default;

    /**
     * @brief 连接到指定 Modbus 主机。
     * @param host 目标主机或 IP 地址。
     * @param port 目标端口，Modbus TCP 默认通常为 502。
     * @return 连接请求是否成功发起或已建立。
     *
     * 使用示例:
     * @code
     * if (client->connectTo("192.168.1.88", 502)) {
     *     // 继续读写寄存器
     * }
     * @endcode
     */
    virtual bool connectTo(const QString& host, int port) = 0;

    /**
     * @brief 断开当前连接。
     *
     * 使用示例:
     * @code
     * client->disconnect();
     * @endcode
     */
    virtual void disconnect() = 0;

    /**
     * @brief 查询连接状态。
     * @return 连接中返回 true，否则返回 false。
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief 读取保持寄存器。
     * @param startAddress 起始地址。
     * @param count 读取数量。
     * @return 读取请求是否成功发出。
     *
     * 使用示例:
     * @code
     * client->readHoldingRegisters(100, 8);
     * @endcode
     */
    virtual bool readHoldingRegisters(int startAddress, int count) = 0;

    /**
     * @brief 写入单个保持寄存器。
     * @param address 寄存器地址。
     * @param value 写入值。
     * @return 写入请求是否成功发出。
     *
     * 使用示例:
     * @code
     * client->writeSingleRegister(200, 1);
     * @endcode
     */
    virtual bool writeSingleRegister(int address, quint16 value) = 0;
};

#endif // IMODBUSCLIENT_H
