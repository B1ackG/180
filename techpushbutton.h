#ifndef TECHPUSHBUTTON_H
/**
 * @file techpushbutton.h
 * @brief 自定义风格的推按钮控件声明，封装样式与交互行为。
 *
 * 详细说明: 定义 `TechPushButton` 或同类控件，可用于统一界面按钮风格与状态控制。
 *
 * 使用示例:
 * @code
 * #include "techpushbutton.h"
 * TechPushButton *b = new TechPushButton(parent);
 * b->setStyle(TechPushButton::StyleEnergy);
 * @endcode
 */
#define TECHPUSHBUTTON_H

#include "animationmanager.h"  //

#include <QtMath>
#include <QWidget>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QEvent>

class TechPushButton : public QPushButton
{
    Q_OBJECT
    // 定义可动画属性
    Q_PROPERTY(qreal glowOpacity READ glowOpacity WRITE setGlowOpacity)
    Q_PROPERTY(qreal pulseScale READ pulseScale WRITE setPulseScale)
    Q_PROPERTY(qreal scanPosition READ scanPosition WRITE setScanPosition)
    Q_PROPERTY(QColor glowColor READ glowColor WRITE setGlowColor)

public:
    /**
     * @brief 构造 TechPushButton
     *
     * 可配置样式、动画与多种特效的按钮控件，支持文本/图标组合，适合科技风 UI。
     * 内部使用 `QPropertyAnimation` 驱动发光/脉冲/扫描线等效果，绘制开销
     * 随效果复杂度增加而上升。
     *
     * @param parent 父控件。
     * @note 默认不启用所有特效，需通过对应 enableXXX 接口开启。
     * @warning 启用大量动画（尤其多个按钮同时动画）可能带来 CPU/GPU 开销，
     *          在性能敏感界面应限制并发动画数量或降低帧率。
     * @since 1.0.0
     */
    explicit TechPushButton(QWidget *parent = nullptr);
    /**
     * 使用示例:
     * @code
     * auto *b = new TechPushButton("Start", parent);
     * b->setPrimaryColor(QColor("#00AEEF"));
     * b->enablePulseEffect(true);
     * connect(b, &QPushButton::clicked, [](){ qDebug() << "clicked"; });
     * @endcode
     */
    // 按钮样式枚举
    enum ButtonStyle {
        StyleDefault,       // 默认科技蓝
        StyleHolographic,   // 全息风格
        StyleEnergy,        // 能量风格
        StyleCircuit,       // 电路板风格
        StyleCyber          // 赛博朋克风格
    };

    // 按钮状态
    enum ButtonState {
        StateNormal,
        StateHovered,
        StatePressed,
        StateDisabled
    };

    /**
     * @brief 使用文本构造
     *
     * @param text 按钮文字
     * @param parent 父控件
     */
    explicit TechPushButton(const QString &text, QWidget *parent = nullptr);

    /**
     * @brief 使用图标与文本构造
     *
     * @param icon 图标
     * @param text 按钮文本
     * @param parent 父控件
     */
    explicit TechPushButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     *
     * 停止所有动画并释放关联的 `QPropertyAnimation`/`QGraphicsDropShadowEffect` 等资源。
     */
    ~TechPushButton();

    // 设置按钮样式
    /**
     * @brief 设置按钮风格
     *
     * @param style 枚举值，选择预定义的视觉风格。
     * @since 1.0.0
     * @see ButtonStyle
     */
    void setButtonStyle(ButtonStyle style);

    /**
     * @brief 获取当前按钮风格
     * @return 当前风格枚举
     */
    ButtonStyle buttonStyle() const { return m_style; }

    /**
     * @brief 设置主色
     * @param color 主颜色
     */
    void setPrimaryColor(const QColor &color);

    /**
     * @brief 获取主色
     * @return 当前主颜色
     */
    QColor primaryColor() const { return m_primaryColor; }

    /**
     * @brief 设置次要色
     * @param color 次要颜色
     */
    void setSecondaryColor(const QColor &color);

    /**
     * @brief 获取次要色
     * @return 当前次要颜色
     */
    QColor secondaryColor() const { return m_secondaryColor; }

    /**
     * @brief 设置发光颜色
     * @param color 发光颜色
     */
    void setGlowColor(const QColor &color);

    /**
     * @brief 获取发光颜色
     * @return 当前发光颜色
     */
    QColor glowColor() const { return m_glowColor; }

    /**
     * @brief 设置边框宽度
     * @param width 边框像素宽度
     */
    void setBorderWidth(int width);

    /**
     * @brief 获取边框宽度
     * @return 当前边框宽度（像素）
     */
    int borderWidth() const { return m_borderWidth; }

    // 动画控制
    /**
     * @brief 启用/禁用悬停动画
     * @param enable true 启用，false 禁用
     * @note 悬停动画使用 `QPropertyAnimation`，可能导致频繁重绘
     */
    void enableHoverAnimation(bool enable = true);

    /**
     * @brief 启用/禁用点击动画
     * @param enable true 启用，false 禁用
     */
    void enableClickAnimation(bool enable = true);

    /**
     * @brief 启用/禁用脉冲效果
     * @param enable true 启用，false 禁用
     * @warning 启用后会创建 `m_pulseAnimation`，请注意资源释放
     */
    void enablePulseEffect(bool enable = true);

    /**
     * @brief 启用/禁用扫描线效果
     * @param enable true 启用，false 禁用
     */
    void enableScanLine(bool enable = true);

    /**
     * @brief 启用/禁用数据流效果
     * @param enable true 启用，false 禁用
     */
    void enableDataFlow(bool enable = true);

    /**
     * @brief 启用/禁用 3D 效果
     * @param enable true 启用，false 禁用
     */
    void enable3DEffect(bool enable = true);

    /**
     * @brief 设置圆角半径
     * @param radius 圆角半径（像素）
     */
    void setCornerRadius(int radius);

    /**
     * @brief 设置图标尺寸
     * @param size 图标尺寸
     */
    void setTechIconSize(const QSize &size);

    /**
     * @brief 设置按钮图标
     * @param icon 要显示的图标
     */
    void setTechIcon(const QIcon &icon);

    // 设置按钮文字效果
    /**
     * @brief 启用/禁用文字发光
     * @param enable true 启用，false 禁用
     */
    void setTextGlow(bool enable = true);

    /**
     * @brief 设置文字颜色
     * @param color 文本颜色
     */
    void setTextColor(const QColor &color);

    /**
     * @brief 仅激活项显示发光；默认态保持平坦
     */
    void applySelectionVisual(bool active);

    /** @brief 方向垫矢量符号；配合底部短标题使用 */
    enum PadGlyph {
        PadGlyphNone = 0,
        PadGlyphUp,
        PadGlyphUpRight,
        PadGlyphRight,
        PadGlyphDownRight,
        PadGlyphDown,
        PadGlyphDownLeft,
        PadGlyphLeft,
        PadGlyphUpLeft,
        PadGlyphRotateCW,
        PadGlyphRotateCCW
    };

    void setPadGlyph(PadGlyph glyph);
    PadGlyph padGlyph() const { return m_padGlyph; }


public slots:
    /**
     * @brief 触发点击动画
     *
     * 手动触发点击效果（常用于程序性反馈），会在内部执行按下/释放的视觉过渡动画。
     *
     * 使用示例:
     * @code
     * button->triggerClickEffect();
     * @endcode
     */
    void triggerClickEffect();

    /**
     * @brief 开始脉冲动画
     *
     * 启动按钮的脉冲效果（放大/透明度变化），通常在强调状态时使用。
     *
     * 使用示例:
     * @code
     * button->startPulse();
     * @endcode
     */
    void startPulse();

    /**
     * @brief 停止脉冲动画
     *
     * 立即停止脉冲效果并恢复到普通显示状态。
     *
     * 使用示例:
     * @code
     * button->stopPulse();
     * @endcode
     */
    void stopPulse();
    

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    // 动画属性getter/setter
    qreal glowOpacity() const { return m_glowOpacity; }
    void setGlowOpacity(qreal opacity);

    qreal pulseScale() const { return m_pulseScale; }
    void setPulseScale(qreal scale);

    qreal scanPosition() const { return m_scanPosition; }
    void setScanPosition(qreal position);

private:
    void init();
    void setupAnimations();
    void updateHoverEffect();
    void updatePressedEffect();
    void updateNormalEffect();

    // 各种绘制函数
    void drawBaseButton(QPainter &painter, const QRect &rect);
    void drawHolographicButton(QPainter &painter, const QRect &rect);
    void drawEnergyButton(QPainter &painter, const QRect &rect);
    void drawCircuitButton(QPainter &painter, const QRect &rect);
    void drawCyberButton(QPainter &painter, const QRect &rect);

    void drawIcon(QPainter &painter, const QRect &rect);
    void drawPadGlyph(QPainter &painter, const QRect &rect);
    void drawText(QPainter &painter, const QRect &rect);
    void drawGlowEffect(QPainter &painter, const QRect &rect);
    void drawScanLine(QPainter &painter, const QRect &rect);
    void drawDataFlow(QPainter &painter, const QRect &rect);
    void drawCircuitLines(QPainter &painter, const QRect &rect);
    void drawCyberGrid(QPainter &painter, const QRect &rect);

    // 样式相关
    ButtonStyle m_style;
    ButtonState m_state;
    PadGlyph m_padGlyph;

    // 颜色相关
    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_glowColor;
    QColor m_textColor;

    // 尺寸相关
    int m_borderWidth;
    int m_cornerRadius;
    QSize m_iconSize;

    // 效果开关
    bool m_hoverAnimationEnabled;
    bool m_clickAnimationEnabled;
    bool m_pulseEffectEnabled;
    bool m_scanLineEnabled;
    bool m_dataFlowEnabled;
    bool m_3dEffectEnabled;
    bool m_textGlowEnabled;

    // 动画相关
    QPropertyAnimation *m_glowAnimation;
    QPropertyAnimation *m_pulseAnimation;
    QPropertyAnimation *m_scanAnimation;
    qreal m_glowOpacity;
    qreal m_pulseScale;
    qreal m_scanPosition;

    // 图标相关
    QIcon m_techIcon;

    // 定时器（用于数据流效果）
    // QTimer *m_dataFlowTimer;
    qreal m_dataFlowProgress;

    // 随机数生成器
    QRandomGenerator *m_random;

    // 阴影效果
    QGraphicsDropShadowEffect *m_shadowEffect;

    // 缓存
    QPixmap m_buttonCache;
    bool m_cacheValid;
private slots:
    /**
     * @brief 更新动画状态（由外部定时器或内部动画驱动器调用）
     * @note 该槽函数负责推进动画帧并更新缓存
     */
    void updateAnimation();

signals:

};

#endif // TECHPUSHBUTTON_H
