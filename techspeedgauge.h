#ifndef TECHSPEEDGAUGE_H
/**
 * @file techspeedgauge.h
 * @brief 速度表盘/仪表控件的声明，用于显示当前速度或目标速度。
 *
 * 详细说明: 定义 `TechSpeedGauge` 等控件，以可视化方式展示速度数据并支持指针/刻度等效果。
 *
 * 使用示例:
 * @code
 * #include "techspeedgauge.h"
 * TechSpeedGauge *g = new TechSpeedGauge(parent);
 * g->setValue(30);
 * @endcode
 */
#define TECHSPEEDGAUGE_H



#include "animationmanager.h"  //
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QPropertyAnimation>
#include <QTimer>
#include <QDebug>

class TechSpeedGauge : public QWidget
{
    Q_OBJECT
    // 定义可动画属性，用于平滑过渡值
    Q_PROPERTY(qreal currentValue READ currentValue WRITE setCurrentValue NOTIFY valueChanged)
    Q_PROPERTY(qreal scanAngle READ scanAngle WRITE setScanAngle)

public:
    // 仪表风格枚举
    enum GaugeStyle {
        StyleCyberFuturistic,   // 赛博未来风格（冷色调，锐利）
        StyleEnergyArc,         // 能量弧风格（暖色调，流动感）
        StyleHolographic,       // 全息投影风格（透明，多层）
        StyleClassicDial        // 经典表盘风格（传统但现代化）
    };


    /**
     * @brief 构造 TechSpeedGauge
     *
     * 可配置样式与动画的速度仪表控件，支持平滑动画过渡与多种视觉特效。
     * 内部通过 `m_valueAnimation` 平滑过渡目标值，通过缓存 `m_backgroundCache`
     * 减少重复绘制开销。
     *
     * @param parent 父控件
     * @note 若改动几何尺寸，请在适当时机清理或重建 `m_backgroundCache`。
     * @warning 长时间启用高帧率动画会增加 CPU 与 GPU 负载，必要时降低精度或使用较慢的动画节奏。
     * @since 1.0.0
     */
    explicit TechSpeedGauge(QWidget *parent = nullptr);

    /**
     * 使用示例:
     * @code
     * auto *g = new TechSpeedGauge(parent);
     * g->setRange(0, 240);
     * g->setPrecision(0);
     * g->setValueSmooth(80); // 平滑过渡到 80
     * @endcode
     */

    // 基本设置接口
    /**
     * @brief 设置仪表数值范围
     *
     * @param min 最小值
     * @param max 最大值
     * @note 调用后会更新刻度并修正当前值到新范围内。
     */
    void setRange(qreal min, qreal max);

    /**
     * 使用示例:
     * @code
     * gauge->setRange(0, 240);
     * gauge->setPrecision(0);
     * gauge->setValueSmooth(80);
     * @endcode
     */

    /**
     * @brief 直接设置当前值（无动画）
     *
     * 立即把显示值设为 `value` 并触发重绘。
     *
     * @param value 目标值，通常在 [m_minValue, m_maxValue] 范围内。
     * @note 若需要平滑过渡请使用 `setValueSmooth()`。
     */
    void setValue(qreal value);                // 直接设置值（跳变）

    /**
     * @brief 平滑动画方式设置目标值
     *
     * 使用 `m_valueAnimation` 插值从当前值过渡到 `value`。
     *
     * @param value 目标值
     * @note 动画完成后将发出 `valueAnimationFinished()` 信号。
     * @warning 大量并发动画可能导致帧率下降。
     */
    void setValueSmooth(qreal value);          // 平滑动画过渡到目标值

    /**
     * 使用示例:
     * @code
     * gauge->setValueSmooth(120.0);
     * @endcode
     */

    /**
     * @brief 设置仪表风格
     * @param style 枚举风格
     */
    void setGaugeStyle(GaugeStyle style);

    /**
     * 使用示例:
     * @code
     * gauge->setGaugeStyle(TechSpeedGauge::StyleEnergyArc);
     * @endcode
     */

    /**
     * @brief 设置主/次要颜色与单位
     */
    void setPrimaryColor(const QColor &color);
    void setSecondaryColor(const QColor &color);
    void setUnit(const QString &unit);
    void setGlowColor(const QColor &color);

    // 避障指示逻辑
    void setObstacleStatus(bool front, bool back, bool left, bool right);
    void drawObstacleIndicators(QPainter &painter);
    bool obstacleFront() const { return m_obstacleFront; }
    bool obstacleBack() const { return m_obstacleBack; }
    bool obstacleLeft() const { return m_obstacleLeft; }
    bool obstacleRight() const { return m_obstacleRight; }

    /**
     * 使用示例:
     * @code
     * gauge->setPrimaryColor(QColor("#00AEEF"));
     * gauge->setUnit("km/h");
     * @endcode
     */

    // 效果开关
    /**
     * @brief 启用/禁用发光效果
     * @param enable true 启用
     */
    void enableGlowEffect(bool enable);

    /**
     * @brief 启用/禁用扫描线效果
     * @param enable true 启用
     */
    void enableScanLine(bool enable);

    /**
     * @brief 启用/禁用脉冲效果
     * @param enable true 启用
     */
    void enablePulseEffect(bool enable);

    /**
     * 使用示例:
     * @code
     * gauge->enableGlowEffect(true);
     * gauge->enableScanLine(false);
     * @endcode
     */

    // 外观设置

    /**
     * @brief 设置数值显示精度
     * @param precision 小数位数
     */
    void setPrecision(int precision);          // 数值显示精度

    /**
     * @brief 设置标题文本
     * @param title 标题字符串
     */
    void setTitle(const QString &title);       // 仪表标题

    /**
     * 使用示例:
     * @code
     * gauge->setTitle("速度");
     * gauge->setPrecision(1);
     * @endcode
     */

    // 获取当前值
    qreal currentValue() const { return m_currentValue; }
    qreal scanAngle() const { return m_scanAngle; }
    //设置当前值
    void setCurrentValue(qreal value);
    //设置模拟值
    bool scanLineEnabled() const;

    /**
     * 使用示例:
     * @code
     * qreal v = gauge->value();
     * bool scan = gauge->scanLineEnabled();
     * @endcode
     */

    /**
     * @brief 获取当前值（用于非动画情况下直接查询）
     * @return 当前显示值
     */
    qreal value() const { return m_currentValue; }


signals:
    void valueChanged(qreal value);
    void valueAnimationFinished();             // 值动画完成信号

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateAnimation();                    // 动画更新槽

private:
    // 私有设置函数
    void setScanAngle(qreal angle);

    // 绘制函数（按图层顺序）
    void drawBackground(QPainter &painter);
    void drawTicksAndNumbers(QPainter &painter);
    void drawValueArc(QPainter &painter);      // 绘制速度弧
    void drawNeedle(QPainter &painter);        // 绘制指针
    void drawValueDisplay(QPainter &painter);  // 中央数值显示
    void drawTitle(QPainter &painter);         // 标题
    void drawForegroundEffects(QPainter &painter); // 前景特效

    // 辅助计算函数
    qreal angleFromValue(qreal value) const;
    QPointF pointOnCircle(qreal radius, qreal angle) const;
    QColor blendColors(const QColor &c1, const QColor &c2, qreal factor) const;

    // 成员变量
    GaugeStyle m_style;
    qreal m_minValue;
    qreal m_maxValue;
    qreal m_currentValue;
    qreal m_targetValue;        // 动画目标值
    QString m_unit;
    QString m_title;
    int m_precision;

    // 颜色
    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_glowColor;

    // 效果开关
    bool m_glowEnabled;
    bool m_scanLineEnabled;
    bool m_pulseEffectEnabled;

    // 避障防碰状态
    bool m_obstacleFront = false;
    bool m_obstacleBack = false;
    bool m_obstacleLeft = false;
    bool m_obstacleRight = false;

    // 动画相关
    qreal m_scanAngle;
    qreal m_pulseIntensity;
    // QTimer *m_animationTimer;
    QPropertyAnimation *m_valueAnimation;

    // 缓存（性能优化）
    QPixmap m_backgroundCache;
    bool m_cacheValid;

    // 几何计算
    QRectF m_gaugeRect;          // 仪表盘矩形
    QPointF m_center;            // 中心点
    qreal m_radius;              // 半径
};

#endif // TECHSPEEDGAUGE_H
