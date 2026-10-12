#include "animationmanager.h"
#include <QDebug>

AnimationManager* AnimationManager::m_instance = nullptr;

AnimationManager* AnimationManager::instance()
{
    if (!m_instance) {
        m_instance = new AnimationManager();
        // 绑定程序退出信号，自动释放单例（核心修复：解决单例泄漏）
        qApp->connect(qApp, &QApplication::aboutToQuit, []() {
            delete AnimationManager::m_instance;
            AnimationManager::m_instance = nullptr;
        });
    }
    return m_instance;
}

// 补充析构函数（原代码无析构）
AnimationManager::~AnimationManager()
{
    m_mainTimer.stop();          // 停止定时器
    m_widgets.clear();           // 清空控件列表（仅清理容器，不删控件）
    qDebug() << "动画管理器单例已销毁，内存释放";
}

AnimationManager::AnimationManager(QObject* parent)
    : QObject(parent)
{
    setAnimationFPS(10);
    qDebug() << "动画管理器已创建，使用单一定时器 (10 FPS)";
}

void AnimationManager::registerWidget(QObject* widget)
{
    if (!widget) {
        return;
    }
    for (const QPointer<QObject> &existing : m_widgets) {
        if (existing == widget) {
            return;
        }
    }
    m_widgets.append(widget);
    connect(widget, &QObject::destroyed, this, [this]() {
        pruneWidgets();
    });
    qDebug() << "控件注册到动画管理器：" << widget->objectName();
}

void AnimationManager::unregisterWidget(QObject* widget)
{
    for (int i = m_widgets.size() - 1; i >= 0; --i) {
        if (m_widgets.at(i) == widget || m_widgets.at(i).isNull()) {
            m_widgets.removeAt(i);
        }
    }
}

void AnimationManager::pruneWidgets()
{
    unregisterWidget(nullptr);
}

void AnimationManager::setAnimationFPS(int fps)
{
    if (fps <= 0) {
        m_mainTimer.stop();
    } else {
        m_mainTimer.setInterval(1000 / fps);  // 1000毫秒 ÷ 帧率 = 间隔
        if (!m_mainTimer.isActive()) {
            m_mainTimer.start();
        }
    }
}

void AnimationManager::updateAllAnimations()
{
    pruneWidgets();
    for (const QPointer<QObject> &widget : m_widgets) {
        if (!widget) {
            continue;
        }
        QMetaObject::invokeMethod(widget.data(), "updateAnimation");
    }
}
