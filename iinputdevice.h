#ifndef IINPUTDEVICE_H
/**
 * @file iinputdevice.h
 * @brief 输入设备抽象接口，统一键盘、按键板等输入源的生命周期控制。
 *
 * 该接口用于把输入设备从具体实现中抽离出来，上层业务只关心“启动、停止、
 * 是否运行”，从而便于替换真实硬件、模拟器或测试桩。
 *
 * 使用示例:
 * @code
 * class MockInputDevice : public IInputDevice {
 * public:
 *     bool start() override { return true; }
 *     void stop() override {}
 *     bool isRunning() const override { return true; }
 * };
 * @endcode
 */
#define IINPUTDEVICE_H

/**
 * @brief 输入设备统一接口。
 */
class IInputDevice {
public:
    /**
     * @brief 析构函数。
     */
    virtual ~IInputDevice() = default;

    /**
     * @brief 启动输入设备。
     * @return 启动成功返回 true。
     *
     * 使用示例:
     * @code
     * if (device->start()) {
     *     // 输入设备已就绪
     * }
     * @endcode
     */
    virtual bool start() = 0;

    /**
     * @brief 停止输入设备。
     *
     * 使用示例:
     * @code
     * device->stop();
     * @endcode
     */
    virtual void stop() = 0;

    /**
     * @brief 查询输入设备是否正在运行。
     * @return 正在运行返回 true。
     */
    virtual bool isRunning() const = 0;
};

#endif // IINPUTDEVICE_H
