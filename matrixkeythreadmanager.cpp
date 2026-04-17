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
        m_workerThread = new QThread(this);

        // 创建监控器对象（在堆上分配）
        m_monitor = new MatrixKeyMonitor(device);

        // 将监控器移动到工作线程
        m_monitor->moveToThread(m_workerThread);

        // 连接线程相关信号
        connect(m_workerThread, &QThread::started, m_monitor, &MatrixKeyMonitor::startMonitoring);

        // 连接键盘信号到主线程
        connect(m_monitor, &MatrixKeyMonitor::keyPressed,
                this, &MatrixKeyThreadManager::keyPressed, Qt::QueuedConnection);

        // 连接错误信号
        connect(m_monitor, &MatrixKeyMonitor::errorOccurred,
                this, [this](const QString &error) {
                    qWarning() << "键盘监控错误:" << error;
                    emit keyPressed(-1, false); // 发送错误信号
                });

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

    if (m_workerThread && m_workerThread->isRunning()) {
        // 请求监控器停止
        if (m_monitor) {
            QMetaObject::invokeMethod(m_monitor, "stopMonitoring", Qt::BlockingQueuedConnection);
        }

        // 退出线程循环
        m_workerThread->quit();

        // 等待线程结束（最多2秒）
        if (!m_workerThread->wait(2000)) {
            qWarning() << "线程未在2秒内退出，发起中断并再次等待";
            m_workerThread->requestInterruption();
            m_workerThread->quit();
            if (!m_workerThread->wait(1000)) {
                qWarning() << "线程仍未退出，执行最后手段 terminate";
                m_workerThread->terminate();
                m_workerThread->wait(1000);
            }
        }
    }

    // 清理资源
    if (m_monitor) {
        delete m_monitor;
        m_monitor = nullptr;
    }

    if (m_workerThread) {
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    qDebug() << "键盘管理器已停止";
}


bool MatrixKeyThreadManager::isRunning() const
{
    return m_workerThread && m_workerThread->isRunning();
}




