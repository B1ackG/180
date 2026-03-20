#ifndef SPEEDMODESELECTOR_H
/**
 * @file speedmodeselector.h
 * @brief 速度模式选择器控件的声明，封装了低/中/高三档速度模式的选择逻辑与显示。
 *
 * 详细说明: 该文件定义了 `SpeedModeSelector` 控件及相关枚举、信号和槽，供界面中选择速度模式使用。
 *
 * 使用示例:
 * @code
 * #include "speedmodeselector.h"
 * auto *s = new SpeedModeSelector(parent);
 * s->setButtonStyle(TechPushButton::StyleEnergy);
 * connect(s, &SpeedModeSelector::modeChanged, [](SpeedMode m){ qDebug() << "mode" << m; });
 * @endcode
 */
#define SPEEDMODESELECTOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "techpushbutton.h"

// 速度模式枚举
enum SpeedMode {
    MODE_LOW = 0,
    MODE_MEDIUM = 1,
    MODE_HIGH = 2
};

class SpeedModeSelector : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(SpeedMode currentMode READ currentMode WRITE setCurrentMode NOTIFY modeChanged)

public:
    /**
     * 功能: 构造 SpeedModeSelector 并初始化 UI 元素。
     * 如何使用: 在界面上直接创建并添加到布局中以供用户选择速度模式。
     * 如何修改: 若需增加模式或不同布局，可在 initUI 中调整并添加对应枚举值。
     */
    explicit SpeedModeSelector(QWidget *parent = nullptr);

    /**
     * 使用示例:
     * @code
     * auto *s = new SpeedModeSelector(parent);
     * s->setButtonStyle(TechPushButton::StyleEnergy);
     * connect(s, &SpeedModeSelector::modeChanged, [](SpeedMode m){ qDebug() << "mode" << m; });
     * @endcode
     */

    /**
     * 功能: 析构，清理动画和 UI 资源。
     * 如何使用: 由 Qt 管理，不需要手动调用。
     * 如何修改: 若增加复杂资源，确保在析构中按顺序释放。
     */
    ~SpeedModeSelector();

    // 获取当前模式
    /**
     * 功能: 返回当前选中的速度模式。
     * 如何使用: 用于逻辑判断或显示当前模式状态。
     * 如何修改: 若增加更多状态信息，可改为返回结构体包含名称/描述等。
     */
    SpeedMode currentMode() const { return m_currentMode; }

    // 获取模式文本
    /**
     * 功能: 根据模式枚举返回简短文本（例如 "低速"）。
     * 如何使用: 用于 UI 文本显示或日志记录。
     * 如何修改: 若需多语言支持，可在此处集成翻译接口。
     */
    QString modeText(SpeedMode mode) const;

    // 获取模式描述
    /**
     * 功能: 返回更详细的模式描述（用于工具提示或说明）。
     * 如何使用: 在 UI 上显示描述信息帮助用户理解各模式差异。
     * 如何修改: 可将描述从外部配置读取以便维护。
     */
    QString modeDescription(SpeedMode mode) const;

    // 设置按钮风格
    /**
     * 功能: 设置内部按钮的样式类型。
     * 如何使用: 在创建后或运行时调用以改变视觉风格。
     * 如何修改: 若新增样式，扩展 `TechPushButton::ButtonStyle` 并在 updateButtonStyles 中处理。
     */
    void setButtonStyle(TechPushButton::ButtonStyle style);

public slots:
    /**
     * @brief 设置当前速度模式
     *
     * 将选择器切换到指定的速度模式并更新界面显示与动画。
     * 会在模式实际改变时发出 `modeChanged(SpeedMode)` 信号。
     *
     * 使用示例:
     * @code
     * selector->setCurrentMode(MODE_HIGH);
     * @endcode
     */
    void setCurrentMode(SpeedMode mode);

    /**
     * @brief 设置激活（选中）状态的颜色
     * @param color 选中时使用的颜色
     *
     * 使用示例:
     * @code
     * selector->setActiveColor(QColor("#00FF00"));
     * @endcode
     */
    void setActiveColor(const QColor &color);

    /**
     * @brief 设置未激活（未选中）状态的颜色
     * @param color 未选中时使用的颜色
     *
     * 使用示例:
     * @code
     * selector->setInactiveColor(QColor("#333333"));
     * @endcode
     */
    void setInactiveColor(const QColor &color);

    /**
     * @brief 设置文字颜色
     * @param color 文本颜色
     *
     * 使用示例:
     * @code
     * selector->setTextColor(Qt::white);
     * @endcode
     */
    void setTextColor(const QColor &color);

signals:
    // 模式改变信号
    void modeChanged(SpeedMode mode);

private slots:
    // 按钮点击槽函数
    void onLowSpeedClicked();
    void onMediumSpeedClicked();
    void onHighSpeedClicked();

private:
    void initUI();              // 初始化UI
    void updateButtonStyles();  // 更新按钮样式
    void createAnimation();     // 创建动画效果

    SpeedMode m_currentMode;    // 当前模式

    // 按钮
    TechPushButton *m_btnLow;
    TechPushButton *m_btnMedium;
    TechPushButton *m_btnHigh;

    // 布局
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_buttonLayout;
    QVBoxLayout *m_labelLayout;

    // 标签
    QLabel *m_titleLabel;
    QLabel *m_modeLabel;
    QLabel *m_descLabel;

    // 样式相关
    TechPushButton::ButtonStyle m_buttonStyle;
    QColor m_activeColor;
    QColor m_inactiveColor;
    QColor m_textColor;

    // 动画
    QPropertyAnimation *m_glowAnimation;
};


#endif // SPEEDMODESELECTOR_H
