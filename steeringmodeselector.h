#ifndef STEERINGMODESELECTOR_H
/**
 * @file steeringmodeselector.h
 * @brief 转向模式选择器的声明，封装了多种转向模式的 UI 与交互。
 *
 * 详细说明: 提供用于选择转向相关模式的控件或工具类声明。
 *
 * 使用示例:
 * @code
 * #include "steeringmodeselector.h"
 * auto *s = new SteeringModeSelector(parent);
 * s->show();
 * @endcode
 */
#define STEERINGMODESELECTOR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include "techpushbutton.h"

// 转向模式枚举
enum SteeringMode {
    STEER_FRONT_BACK = 0,  // 前后轮转向
    STEER_FRONT_ONLY = 1,  // 前轮转向
    STEER_PARALLEL = 2,    // 平移
    STEER_LATERAL = 3,     // 横向移动
    STEER_ROTATE = 4       // 原地旋转
};

class SteeringModeSelector : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(SteeringMode currentMode READ currentMode WRITE setCurrentMode NOTIFY modeChanged)

public:
    /**
     * 功能: 构造函数，初始化转向模式选择器 UI。
     * 如何使用: 在需要显示转向模式选择的界面创建并添加到布局。
     * 如何修改: 若需支持更多转向模式或不同布局，在 initUI 中扩展并增加枚举项。
     */
    explicit SteeringModeSelector(QWidget *parent = nullptr);

    /**
     * 使用示例:
     * @code
     * auto *st = new SteeringModeSelector(parent);
     * st->setButtonStyle(TechPushButton::StyleHolographic);
     * st->setCurrentMode(STEER_FRONT_ONLY);
     * @endcode
     */

    /**
     * 功能: 析构函数，释放动画与 UI 资源。
     * 如何使用: 由 Qt 自动管理对象树时调用。
     * 如何修改: 如增加外部资源，确保在析构中正确释放。
     */
    ~SteeringModeSelector();

    // 获取当前模式
    /**
     * 功能: 返回当前选中的转向模式。
     * 如何使用: 用于控制逻辑或显示当前状态。
     * 如何修改: 若需返回附加信息，可改为返回结构体或提供额外查询方法。
     */
    SteeringMode currentMode() const { return m_currentMode; }

    // 获取模式文本
    /**
     * 功能: 根据模式枚举返回可显示的文本描述。
     * 如何使用: 在 UI 上显示模式名称或提示文案时调用。
     * 如何修改: 若需多语言支持，可集成翻译系统。
     */
    QString modeText(SteeringMode mode) const;

    // 获取Modbus写入值
    /**
     * 功能: 返回对应模式在 Modbus 中写入的数值（便于下发到 PLC）。
     * 如何使用: 当模式切换需要写入 PLC 时，调用此函数获取写入值。
     * 如何修改: 若映射关系可配置，请将映射数据外置并在此读取。
     */
    int modeModbusValue(SteeringMode mode) const;

    // 设置按钮风格
    /**
     * 功能: 设置内部按钮样式。
     * 如何使用: 在创建后或运行时调用以改变视觉样式。
     * 如何修改: 若增加新样式，扩展 `TechPushButton::ButtonStyle` 并在样式更新中处理。
     */
    void setButtonStyle(TechPushButton::ButtonStyle style);

public slots:
    /**
     * @brief 设置当前转向模式
     *
     * 切换到指定的转向模式，并在模式变更时发出 `modeChanged()` 信号。
     *
     * 使用示例:
     * @code
     * selector->setCurrentMode(STEER_PARALLEL);
     * @endcode
     */
    void setCurrentMode(SteeringMode mode);

    /**
     * @brief 设置激活（选中）颜色
     * @param color 激活状态使用的颜色
     *
     * 使用示例:
     * @code
     * selector->setActiveColor(QColor("#00FF00"));
     * @endcode
     */
    void setActiveColor(const QColor &color);

    /**
     * @brief 设置未激活颜色
     * @param color 未选中时使用的颜色
     *
     * 使用示例:
     * @code
     * selector->setInactiveColor(QColor("#333333"));
     * @endcode
     */
    void setInactiveColor(const QColor &color);

    /**
     * @brief 设置文本颜色
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
    void modeChanged(SteeringMode mode, int modbusValue);

private slots:
    // 按钮点击槽函数
    void onFrontBackClicked();
    void onFrontOnlyClicked();
    void onParallelClicked();
    void onLateralClicked();
    void onRotateClicked();

private:
    void initUI();              // 初始化UI
    void updateButtonStyles();  // 更新按钮样式

    SteeringMode m_currentMode;    // 当前模式

    // 按钮
    TechPushButton *m_btnFrontBack;
    TechPushButton *m_btnFrontOnly;
    TechPushButton *m_btnParallel;
    TechPushButton *m_btnLateral;
    TechPushButton *m_btnRotate;

    // 布局
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_buttonLayout;

    // 标题
    QLabel *m_titleLabel;

    // 样式相关
    TechPushButton::ButtonStyle m_buttonStyle;
    QColor m_activeColor;
    QColor m_inactiveColor;
    QColor m_textColor;
};

#endif // STEERINGMODESELECTOR_H
