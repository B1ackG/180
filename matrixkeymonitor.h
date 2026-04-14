#ifndef MATRIXKEYMONITOR_H
/**
 * @file matrixkeymonitor.h
 * @brief 矩阵按键监听器的声明，用于采集并分发矩阵式按键事件。
 *
 * 详细说明: 提供对行列扫描按键矩阵的封装，包含防抖、长按/短按识别等功能。
 *
 * 使用示例:
 * @code
 * #include "matrixkeymonitor.h"
 * MatrixKeyMonitor monitor;
 * monitor.startScan();
 * @endcode
 */
#define MATRIXKEYMONITOR_H

#include <QObject>
#include <QSocketNotifier>
#include <QMutex>
#include <linux/input.h>
#include <QTimer>
#include <QEventLoop>
#include <QHash>

class MatrixKeyMonitor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造 MatrixKeyMonitor
     *
     * @param device 输入设备路径，默认 "/dev/input/event0"。
     * @param parent 父对象
     * @since 1.0.0
     */
    explicit MatrixKeyMonitor(const QString &device = QStringLiteral("/dev/input/event0"), QObject *parent = nullptr);

    /**
     * @brief 备用构造（仅 parent）
     */
    explicit MatrixKeyMonitor(QObject *parent = nullptr);

    /**
     * @brief 析构
     */
    ~MatrixKeyMonitor();

    /**
     * @brief 开始监控输入事件
     *
     * 打开设备文件并通过 `QSocketNotifier` 监听事件，可在独立线程中运行以避免阻塞主线程。
     *
     * @return true 表示监控已成功启动
     * @warning 若设备路径无效或权限不足则会返回 false 并发出 `errorOccurred()`。
     */
    bool startMonitoring();  // 开始监控

    /**
     * 使用示例:
     * @code
     * MatrixKeyMonitor monitor("/dev/input/event0");
     * if(monitor.startMonitoring()) {
     *     connect(&monitor, &MatrixKeyMonitor::keyPressed, [](int k, bool p){ qDebug() << k << p; });
     * }
     * @endcode
     */

    /**
     * @brief 停止监控并关闭资源
     */
    Q_INVOKABLE void stopMonitoring();   // 停止监控

    /**
     * 使用示例:
     * @code
     * monitor.stopMonitoring();
     * @endcode
     */

    /**
     * @brief 是否正在监控
     * @return true 表示正在监控
     */
    bool isMonitoring() const { return m_isRunning; }

    /**
     * @brief 运行事件循环（阻塞式），通常在独立线程中使用
     */
    void runEventLoop();  // 运行事件循环

    /**
     * 使用示例:
     * @code
     * // 在独立线程中运行
     * monitor.moveToThread(&thread);
     * connect(&thread, &QThread::started, &monitor, &MatrixKeyMonitor::runEventLoop);
     * thread.start();
     * @endcode
     */

signals:
    /**
     * @brief 按键事件（更高层抽象）
     */
    void keyPressed(int keyNumber, bool isPressed);

    /**
     * @brief 原始按键事件
     * @param keyCode 按键代码
     * @param keyValue 值（按下/释放）
     */
    void rawKeyEvent(int keyCode, int keyValue);

    /**
     * 说明: 该信号用于传递底层原始按键事件，通常用于调试或底层处理。
     */

    /**
     * @brief 错误发生时发出
     */
    void errorOccurred(const QString &error);

    /**
     * @brief 状态变化（运行/停止）
     */
    void statusChanged(bool running);

private slots:
    void onSocketActivated(int socket);

private:
    int mapKeyCodeToButtonNumber(int keyCode);

private:
    int m_fd;
    QSocketNotifier *m_notifier;
    QString m_devicePath;
    QMutex m_mutex;
    bool m_isRunning;
    QHash<int, bool> m_lastKeyState;
    // QTimer *m_keepAliveTimer;  // 保持线程活动的定时器
};

#endif // MATRIXKEYMONITOR_H
