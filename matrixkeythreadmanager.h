#ifndef MATRIXKEYTHREADMANAGER_H
/**
 * @file matrixkeythreadmanager.h
 * @brief 矩阵按键线程管理器声明，负责按键扫描线程的生命周期与事件分发。
 *
 * 详细说明: 管理按键扫描相关线程，提供安全的事件投递接口和启动/停止控制。
 *
 * 使用示例:
 * @code
 * #include "matrixkeythreadmanager.h"
 * MatrixKeyThreadManager mgr;
 * mgr.start();
 * @endcode
 */
#define MATRIXKEYTHREADMANAGER_H

#include <QObject>
#include <QThread>

    class MatrixKeyMonitor;

class MatrixKeyThreadManager : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造 MatrixKeyThreadManager
     *
     * 管理 `MatrixKeyMonitor` 的线程启动与停止，封装线程生命周期。
     *
     * @param parent 父对象。
     * @since 1.0.0
     */
    explicit MatrixKeyThreadManager(QObject *parent = nullptr);

    /**
     * @brief 析构
     *
     * 停止并清理后台线程与监控器资源。
     */
    ~MatrixKeyThreadManager();

    /**
     * @brief 启动监控线程
     *
     * @param device 输入设备路径，默认为 "/dev/input/event0"。
     * @return true 表示成功启动并开始监控，false 表示失败（例如无法打开设备）。
     * @note 内部会在新线程中创建 `MatrixKeyMonitor` 并将其移入该线程。
     * @warning 若设备被其它进程占用，可能无法打开导致启动失败。
     */
    bool start(const QString &device = "/dev/input/event0");

    /**
     * 使用示例:
     * @code
     * MatrixKeyThreadManager mgr;
     * if(mgr.start("/dev/input/event0")) {
     *     connect(&mgr, &MatrixKeyThreadManager::keyPressed, [](int k, bool p){ qDebug() << k << p; });
     * }
     * @endcode
     */

    /**
     * @brief 停止监控并终止线程
     */
    void stop();

    /**
     * 使用示例:
     * @code
     * mgr.stop();
     * @endcode
     */

    /**
     * @brief 是否正在运行
     * @return true 表示正在运行
     */
    bool isRunning() const;

    /**
     * @brief 批量读取寄存器（占位/辅助方法）
     *
     * 此方法为工具接口，便于批量读取或触发相关处理。
     */
    void readMultipleRegisters(int startAddress, int count);

    /**
     * 使用示例:
     * @code
     * mgr.readMultipleRegisters(100, 10);
     * @endcode
     */

    /**
     * @brief 获取实际用于工作的线程
     * @return 指向正在运行的 `QThread`，无线程时返回 nullptr。
     */
    QThread* workerThread() const { return m_workerThread; }

signals:
    /**
     * @brief 键按下事件
     * @param keyNumber 键编号
     * @param isPressed 是否按下（true 表示按下）
     */
    void keyPressed(int keyNumber, bool isPressed);

private:
    QThread *m_workerThread;
    MatrixKeyMonitor *m_monitor;
};

#endif // MATRIXKEYTHREADMANAGER_H
