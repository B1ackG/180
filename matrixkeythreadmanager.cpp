#include "matrixkeythreadmanager.h"
#include "matrixkeymonitor.h"
#include <QDebug>

MatrixKeyThreadManager::MatrixKeyThreadManager(QObject *parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_monitor(nullptr)
{
    qDebug() << "MatrixKeyThreadManager 创建";
}

MatrixKeyThreadManager::~MatrixKeyThreadManager()
{
    stop();
    qDebug() << "MatrixKeyThreadManager 销毁";
}

bool MatrixKeyThreadManager::start(const QString &device)
{
    if (m_workerThread) {
        qWarning() << "键盘管理器已在运行";
        return true;
    }

    try {
        // 创建工作线程
        m_workerThread = new QThread();

        // 创建监控器对象（在堆上分配，无 parent，才能迁入工作线程）
        m_monitor = new MatrixKeyMonitor(device);

        // 将监控器移动到工作线程
        m_monitor->moveToThread(m_workerThread);

        // 连接线程相关信号
        connect(m_workerThread, &QThread::started, m_monitor, &MatrixKeyMonitor::startMonitoring);

        // 连接键盘信号到主线程
        connect(m_monitor, &MatrixKeyMonitor::keyPressed,
                this, &MatrixKeyThreadManager::keyPressed, Qt::QueuedConnection);

        // 连接错误信号。保留 keyPressed(-1) 作为既有错误通知，不改上层语义。
        connect(m_monitor, &MatrixKeyMonitor::errorOccurred,
                this, [this](const QString &error) {
                    qWarning() << "键盘监控错误:" << error;
                    emit keyPressed(-1, false);
                }, Qt::QueuedConnection);

        // 启动线程
        m_workerThread->start();

        qDebug() << "矩阵按键线程启动成功，线程ID:" << m_workerThread;
        return true;

    } catch (const std::exception &e) {
        qCritical() << "启动键盘管理器失败:" << e.what();
        stop();
        return false;
    }
}

void MatrixKeyThreadManager::stop()
{
    qDebug() << "正在停止键盘管理器...";

    QThread *thread = m_workerThread;
    MatrixKeyMonitor *monitor = m_monitor;
    if (!thread && !monitor) {
        return;
    }

    if (thread && thread->isRunning() && monitor) {
        const bool invoked = QMetaObject::invokeMethod(monitor, [monitor]() {
            monitor->stopMonitoring();
            monitor->deleteLater();
        }, Qt::BlockingQueuedConnection);
        if (!invoked) {
            qWarning() << "矩阵键监控器无法在所属线程停止";
        }
        m_monitor = nullptr;
        thread->quit();
        if (!thread->wait(3000)) {
            qWarning() << "矩阵键线程未在 3 秒内退出，保留线程对象，禁止 terminate";
            return;
        }
    } else if (monitor && monitor->thread() == QThread::currentThread()) {
        monitor->stopMonitoring();
        delete monitor;
        m_monitor = nullptr;
    }

    if (thread && !thread->isRunning()) {
        delete thread;
        m_workerThread = nullptr;
        m_monitor = nullptr;
    }

    qDebug() << "键盘管理器已停止";
}


bool MatrixKeyThreadManager::isRunning() const
{
    return m_workerThread && m_workerThread->isRunning();
}




