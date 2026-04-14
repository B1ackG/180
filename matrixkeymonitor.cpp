#include "matrixkeymonitor.h"
#include <fcntl.h>
#include <unistd.h>
#include <QDebug>
#include <QCoreApplication>

MatrixKeyMonitor::MatrixKeyMonitor(const QString &device, QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_notifier(nullptr)
    , m_devicePath(device)
    , m_isRunning(false)
    // , m_keepAliveTimer(nullptr)
{
    qDebug() << "MatrixKeyMonitor 创建，设备:" << device;
}

MatrixKeyMonitor::MatrixKeyMonitor(QObject *parent)
    : MatrixKeyMonitor("/dev/input/event0", parent)
{
}

MatrixKeyMonitor::~MatrixKeyMonitor()
{
    stopMonitoring();
    qDebug() << "MatrixKeyMonitor 销毁";
}

bool MatrixKeyMonitor::startMonitoring()
{
    if (m_isRunning) {
        qWarning() << "监控器已在运行";
        return true;
    }

    // 打开输入设备（非阻塞模式）
    m_fd = open(m_devicePath.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) {
        QString error = QString("无法打开设备 %1: %2").arg(m_devicePath).arg(strerror(errno));
        qCritical() << error;
        emit errorOccurred(error);
        return false;
    }

    // 创建socket通知器
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &MatrixKeyMonitor::onSocketActivated);

    // 创建保活定时器（防止线程意外退出）
    // m_keepAliveTimer = new QTimer(this);
    // m_keepAliveTimer->setInterval(1000); // 1秒
    // connect(m_keepAliveTimer, &QTimer::timeout, this, []() {
    //     // 空操作，仅保持事件循环活跃
    // });
    // m_keepAliveTimer->start();

    m_isRunning = true;
    m_lastKeyState.clear();
    qDebug() << "键盘监控已启动，设备:" << m_devicePath;
    emit statusChanged(true);

    return true;
}

void MatrixKeyMonitor::stopMonitoring()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;

    // 停止定时器
    // if (m_keepAliveTimer) {
    //     m_keepAliveTimer->stop();
    //     delete m_keepAliveTimer;
    //     m_keepAliveTimer = nullptr;
    // }

    // 停止socket通知器
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

    // 关闭文件描述符
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }

    m_lastKeyState.clear();

    qDebug() << "键盘监控已停止";
    emit statusChanged(false);
}

void MatrixKeyMonitor::onSocketActivated(int socket)
{
    // 这个槽函数会在工作线程中执行
    struct input_event ev;

    // 读取所有可用的事件
    while (true) {
        ssize_t bytesRead = read(socket, &ev, sizeof(ev));

        if (bytesRead != sizeof(ev)) {
            if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据了
                    break;
                }
                qWarning() << "读取键盘事件失败:" << strerror(errno);
                break;
            }
            break;
        }

        // 只处理按键事件
        if (ev.type == EV_KEY) {
            // Linux input: 0=release, 1=press, 2=auto-repeat。
            // auto-repeat 不能当作释放，否则会错误触发 514 写 0。
            if (ev.value != 0 && ev.value != 1) {
                continue;
            }

            int keyNumber = mapKeyCodeToButtonNumber(ev.code);
            bool pressed = (ev.value == 1);

            if (keyNumber >= 0) {
                if (m_lastKeyState.contains(keyNumber) && m_lastKeyState.value(keyNumber) == pressed) {
                    continue;
                }
                m_lastKeyState[keyNumber] = pressed;

                qDebug() << "矩阵按键事件 - 键码:" << ev.code
                         << "-> 按钮" << keyNumber
                         << "状态:" << (pressed ? "按下" : "释放");

                // 发出信号
                emit keyPressed(keyNumber, pressed);
                emit rawKeyEvent(ev.code, ev.value);
            }
        }
    }
}

int MatrixKeyMonitor::mapKeyCodeToButtonNumber(int keyCode)
{
    switch (keyCode) {
    case 62: return 1;   // ○1
    case 47: return 2;   // ○2
    case 5:  return 3;   // ○3
    case 60: return 4;   // ○4
    case 4:  return 5;   // ○5
    case 3:  return 6;   // ○6
    case 18: return 7;   // ○7
    case 17: return 8;   // ○8
    case 32: return 9;   // ○9
    case 31: return 10;  // ○10
    case 46: return 11;  // ○11
    case 45: return 12;  // ○12
    case 30: return 13;  // ○13
    case 16: return 14;  // ○14
    default:
        qDebug() << "未映射的键码:" << keyCode;
        return -1;
    }
}

void MatrixKeyMonitor::runEventLoop()
{
    // 如果需要手动运行事件循环
    QEventLoop loop;
    connect(this, &MatrixKeyMonitor::statusChanged, &loop, &QEventLoop::quit);
    loop.exec();
}
