#ifndef ENABLEBUTTONWORKER_H
/**
 * @file enablebuttonworker.h
 * @brief 启用按钮后台任务处理的声明，用于异步处理按键使能/禁用逻辑。
 *
 * 详细说明: 包含用于在后台线程处理按钮状态或长操作的 worker 类声明。
 *
 * 使用示例:
 * @code
 * #include "enablebuttonworker.h"
 * EnableButtonWorker *w = new EnableButtonWorker;
 * w->start();
 * @endcode
 */
#define ENABLEBUTTONWORKER_H

#include <QObject>
#include <QThread>
#include <QTimer>

class EnableButtonWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * 功能: 构造 EnableButtonWorker，用于在独立线程中轮询外部使能按键（通过文件描述符）。
     * 如何使用: 在创建线程并将该 worker 移动到线程后，调用 startPolling() 开始轮询。
     * 如何修改: 若更换按键读取方式（例如 GPIO 库），在实现中替换 poll() 的读取逻辑。
     */
    explicit EnableButtonWorker(int fd, QObject *parent = nullptr);

    /**
     * 功能: 析构函数，停止轮询并释放资源。
     * 如何使用: 对象被销毁时自动调用，不需手动调用。
     * 如何修改: 若增加外部资源，确保在析构中正确关闭。
     */
    ~EnableButtonWorker();

public slots:
    /**
     * 功能: 启动轮询定时器或循环以检测按键状态变化。
     * 如何使用: 在 worker 所在线程就绪后调用，一般在移入线程并启动线程后执行。
     * 如何修改: 可调整轮询间隔或改用事件/中断机制以减少 CPU 占用。
     */
    void startPolling();

    /**
     * 使用示例:
     * @code
     * EnableButtonWorker *w = new EnableButtonWorker(fd);
     * w->moveToThread(workerThread);
     * connect(workerThread, &QThread::started, w, &EnableButtonWorker::startPolling);
     * @endcode
     */

    /**
     * 功能: 停止轮询并清理计时器。
     * 如何使用: 在线程退出或不再需要轮询时调用。
     * 如何修改: 若存在未处理的回调或状态，应先处理再停止以避免竞争。
     */
    void stopPolling();

    /**
     * 使用示例:
     * @code
     * w->stopPolling();
     * @endcode
     */

signals:
    void buttonStateChanged(bool enabled);
    void errorOccurred(const QString &error);

private slots:
    void poll();

private:
    int m_fd;
    bool m_running;
    QTimer *m_timer = nullptr;
};

#endif // ENABLEBUTTONWORKER_H
