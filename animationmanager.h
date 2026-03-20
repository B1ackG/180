#ifndef ANIMATIONMANAGER_H
/**
 * @file animationmanager.h
 * @brief 动画管理器声明，负责创建与管理界面动画效果的工具类。
 *
 * 详细说明: 提供统一的动画创建、复用与缓存策略，简化界面动画的使用。
 *
 * 使用示例:
 * @code
 * #include "animationmanager.h"
 * AnimationManager::instance().playFadeIn(widget);
 * @endcode
 */
#define ANIMATIONMANAGER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QApplication>

class AnimationManager : public QObject
{
    Q_OBJECT
public:
    // 单例接口
    // 单例接口
    /**
     * 功能: 获取 AnimationManager 单例，用于统一管理 UI 动画定时更新。
     * 如何使用: 调用 AnimationManager::instance() 并通过 registerWidget 注册需要动画的控件。
     * 如何修改: 若需多实例或不同定时器策略，可改造为可配置工厂模式。
     */
    static AnimationManager* instance();

    /**
     * 使用示例:
     * @code
     * AnimationManager::instance()->registerWidget(widget);
     * AnimationManager::instance()->setAnimationFPS(30);
     * @endcode
     */

    /**
     * 功能: 析构函数，释放单例资源并停止定时器。
     * 如何使用: 单例析构由程序结束时触发，通常不需手动调用。
     * 如何修改: 若需要更早释放资源，可提供显式 shutdown() 接口。
     */
    ~AnimationManager() override;

    // 注册/注销控件
    /**
     * 功能: 将一个 QObject（通常为 QWidget 或其子类）注册到动画管理器，以在定时器触发时更新动画。
     * 如何使用: 在控件创建或需要动画时调用 registerWidget(widget)。
     * 如何修改: 若需优先级或按组更新，扩展注册接口以接受元数据（优先级/分组）。
     */
    void registerWidget(QObject* widget);

    /**
     * 功能: 注销已注册的控件，停止对其的动画更新。
     * 如何使用: 在控件销毁或不再需要动画时调用以避免悬挂指针。
     * 如何修改: 若使用弱引用，则可在内部自动回收，此处可简化为可选操作。
     */
    void unregisterWidget(QObject* widget);

    // 设置动画帧率
    /**
     * 功能: 设置动画刷新帧率（FPS），影响定时器间隔。
     * 如何使用: 在初始化或运行时调整以优化平滑度与性能。
     * 如何修改: 可改为支持每控件单独帧率或按场景动态调整。
     */
    void setAnimationFPS(int fps);

public slots:
    void updateAllAnimations();

private:
    // 私有构造函数（单例）
    explicit AnimationManager(QObject* parent = nullptr);
    // 禁止拷贝/赋值（避免单例被复制）
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;

    static AnimationManager* m_instance;
    QTimer m_mainTimer;               // 主定时器
    QList<QObject*> m_widgets;        // 注册的控件列表
};

#endif // ANIMATIONMANAGER_H
