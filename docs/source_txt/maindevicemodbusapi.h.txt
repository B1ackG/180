#ifndef MAINDEVICEMODBUSAPI_H
#define MAINDEVICEMODBUSAPI_H

#include <QString>
#include <QtGlobal>

class ModbusThreadManager;

/**
 * @brief 主设备 Modbus API 适配层。
 *
 * 该类对上层暴露一组“先检查连接状态，再执行读写”的安全接口，
 * 用于减少主窗口和轮询代码中的重复判断与错误信息拼装。
 */
class MainDeviceModbusApi
{
public:
    /**
     * @brief 判断管理器是否可用于主设备 Modbus 操作。
     * @param manager 线程管理器。
     * @return 管理器存在且已连接时返回 true。
     */
    static bool isReady(const ModbusThreadManager *manager);

    /**
     * @brief 写入单个寄存器。
     * @param manager 线程管理器。
     * @param address 寄存器地址。
     * @param value 要写入的数值。
     * @param errorMessage 可选输出参数，用于返回失败原因。
     * @return 写入请求是否成功发起。
     *
     * 使用示例:
     * @code
     * QString error;
     * MainDeviceModbusApi::writeRegister(manager, 200, 1, &error);
     * @endcode
     */
    static bool writeRegister(ModbusThreadManager *manager,
                              int address,
                              int value,
                              QString *errorMessage = nullptr);

    /**
     * @brief 读取保持寄存器。
     * @param manager 线程管理器。
     * @param startAddress 起始地址。
     * @param count 读取数量。
     * @param errorMessage 可选输出参数，用于返回失败原因。
     * @return 读取请求是否成功发起。
     */
    static bool readHoldingRegisters(ModbusThreadManager *manager,
                                     int startAddress,
                                     int count,
                                     QString *errorMessage = nullptr);

    /**
     * @brief 读取并调试输出指定地址。
     * @param manager 线程管理器。
     * @param address 目标寄存器地址。
     * @param errorMessage 可选输出参数，用于返回失败原因。
     * @return 读取请求是否成功发起。
     */
    static bool readAndDebugAddress(ModbusThreadManager *manager,
                                    int address,
                                    QString *errorMessage = nullptr);
};

#endif // MAINDEVICEMODBUSAPI_H
