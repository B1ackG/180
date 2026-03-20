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
    if (!m_widgets.contains(widget)) {
        m_widgets.append(widget);
        qDebug() << "控件注册到动画管理器：" << widget->objectName();
    }
}

void AnimationManager::unregisterWidget(QObject* widget)
{
    m_widgets.removeOne(widget);
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
    // 这里是关键！每次定时器触发，更新所有控件
    for (QObject* widget : m_widgets) {
        // 发送一个信号，告诉控件该更新动画了
        QMetaObject::invokeMethod(widget, "updateAnimation");
    }
}
