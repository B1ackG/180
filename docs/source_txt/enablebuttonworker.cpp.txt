#include "enablebuttonworker.h"
#include <unistd.h>
#include <cstring>
#include <QDebug>
#include <QTimer>

EnableButtonWorker::EnableButtonWorker(int fd, QObject *parent)
    : QObject(parent), m_fd(fd), m_running(false), m_timer(nullptr)
{
    qDebug() << "EnableButtonWorker 创建，文件描述符:" << fd;
}

EnableButtonWorker::~EnableButtonWorker()
{
    stopPolling();
    qDebug() << "EnableButtonWorker 销毁";
}

void EnableButtonWorker::startPolling()
{
    if (m_running) {
        return;
    }

    m_running = true;
    qDebug() << "使能按钮开始监控 (定时轮询模式)";

    if (m_fd >= 0) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &EnableButtonWorker::poll);
        m_timer->start(100); // 100ms 轮询一次
    } else {
        qCritical() << "无效的文件描述符!";
        emit errorOccurred("无效的文件描述符");
    }
}

void EnableButtonWorker::stopPolling()
{
    m_running = false;

    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
}

void EnableButtonWorker::poll()
{
    if (!m_running) return;
    int socket = m_fd;

    // 读取设备数据
    // 假设每次读取8字节（根据之前的代码逻辑）
    char data[8];
    // 清零数据
    std::memset(data, 0, sizeof(data));
    
    // 移动文件指针到开头
    lseek(socket, 0, SEEK_SET);
    
    ssize_t bytesRead = read(socket, data, sizeof(data));

    if (bytesRead > 0) {
        // 转换为字符串
        QString dataStr = QString::fromLocal8Bit(data, bytesRead);

        // 解析状态：第一个字符是'1'表示按下，'0'表示松开
        // 注意：根据之前的代码，'1'是enabled
        bool enabled = (data[0] == '1');

        // 发送信号到主线程
        emit buttonStateChanged(enabled);

    } else if (bytesRead < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            QString error = QString("读取使能按钮失败: %1 (errno: %2)")
                                .arg(strerror(errno)).arg(errno);
            qWarning() << "[EnableButton]" << error;
            emit errorOccurred(error);
        }
    }
}
