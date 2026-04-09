#ifndef MAINMODBUSPOLLER_H
#define MAINMODBUSPOLLER_H

class QTimer;
class ModbusThreadManager;
class ModbusVariables;

/**
 * @brief 主设备 Modbus 轮询调度辅助类。
 *
 * 该类把“是否启动轮询”“是否跳过当前轮询”和“轮询下一条变量”这类
 * 控制逻辑集中在一起，避免这些流程散落在主窗口里。
 */
class MainModbusPoller
{
public:
    /**
     * @brief 轮询启动前的保护检查。
     * @param pollTimer 当前轮询定时器。
     * @return 始终返回 true；若定时器已激活则先停止它。
     *
     * 该函数用于把“重新启动轮询前先停掉旧定时器”的逻辑集中封装。
     */
    static bool shouldSkipStart(QTimer *pollTimer);

    /**
     * @brief 当前轮询周期是否应跳过。
     * @return 始终返回 true。
     *
     * 该函数作为轮询门控入口保留，便于未来按功能开关或运行状态
     * 引入更复杂的跳过条件。
     */
    static bool shouldSkipPoll();

    /**
     * @brief 轮询下一条 Modbus 变量。
     * @param manager Modbus 线程管理器。
     * @param variables 变量表。
     * @param currentIndex 当前轮询索引，成功后会自动递增并循环。
     * @return 成功发起轮询返回 true。
     *
     * 使用示例:
     * @code
     * int index = 0;
     * MainModbusPoller::pollNextVariable(ModbusThreadManager::instance(),
     *                                   vars,
     *                                   index);
     * @endcode
     */
    static bool pollNextVariable(ModbusThreadManager *manager,
                                 ModbusVariables *variables,
                                 int &currentIndex);
};

#endif // MAINMODBUSPOLLER_H
