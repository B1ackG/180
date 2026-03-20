#ifndef TECHSPEEDDIALSIMPLE_H
/**
 * @file techspeeddialsimple.h
 * @brief 简化版速度拨盘控件的声明，提供基础的速度显示和调整功能。
 *
 * 详细说明: 该控件适用于需要简单速度显示与交互的场景，接口轻量。
 *
 * 使用示例:
 * @code
 * #include "techspeeddialsimple.h"
 * TechSpeedDialSimple *d = new TechSpeedDialSimple(parent);
 * d->setValue(50);
 * @endcode
 */
#define TECHSPEEDDIALSIMPLE_H

#include <QWidget>

#include <QDial>
#include <QPainter>
#include <QtMath>

class TechSpeedDialSimple : public QDial
{
    Q_OBJECT
    Q_PROPERTY(QColor themeColor READ themeColor WRITE setThemeColor)
    Q_PROPERTY(bool useWhiteTheme READ useWhiteTheme WRITE setUseWhiteTheme)

public:
    enum SpeedLevel {
        LowSpeed = 0,
        MediumSpeed = 33,
        HighSpeed = 66
    };

    /**
     * @brief 构造 TechSpeedDialSimple
     *
     * 简化版速度表盘，继承自 `QDial`，提供快速设置速度级别的便捷接口。
     *
     * @param parent 父控件
     * @since 1.0.0
     */
    explicit TechSpeedDialSimple(QWidget *parent = nullptr);

    /**
     * 使用示例:
     * @code
     * auto *d = new TechSpeedDialSimple(parent);
     * d->setThemeColor(QColor("#00CC99"));
     * d->setSpeedLevel(TechSpeedDialSimple::MediumSpeed);
     * @endcode
     */

    // 主题设置
    /**
     * @brief 获取主题颜色
     * @return 当前主题颜色
     */
    QColor themeColor() const { return m_themeColor; }

    /**
     * @brief 设置主题颜色
     * @param color 主题颜色
     */
    void setThemeColor(const QColor &color);

    /**
     * 使用示例:
     * @code
     * d->setThemeColor(QColor("#00CC99"));
     * @endcode
     */

    /**
     * @brief 是否使用白色主题
     * @return true 表示使用白色主题
     */
    bool useWhiteTheme() const { return m_useWhiteTheme; }

    /**
     * @brief 设置是否使用白色主题
     * @param useWhite true 使用白色主题
     */
    void setUseWhiteTheme(bool useWhite);

    // 设置速度级别（简化接口）
    /**
     * @brief 设置速度级别（Low/Medium/High）
     *
     * 将表盘设置为预定义的速度等级，会映射到对应的角度/值区间。
     *
     * @param level 0/1/2 或 `SpeedLevel` 枚举之一
     * @note 传入非法值会被忽略。
     */
    void setSpeedLevel(int level); // 0,1,2

    /**
     * 使用示例:
     * @code
     * d->setSpeedLevel(TechSpeedDialSimple::MediumSpeed);
     * @endcode
     */

    /**
     * @brief 获取当前速度级别（低/中/高）
     * @return 0/33/66 分别对应 Low/Medium/High
     */
    int currentSpeedLevel() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void drawTechDial(QPainter &painter);
    void drawSpeedMarks(QPainter &painter);
    void drawIndicator(QPainter &painter);
    void drawTechLabels(QPainter &painter);

    QColor m_themeColor;
    bool m_useWhiteTheme;
    QColor m_bgColor;
    QColor m_textColor;

    QRect m_dialRect;
};
#endif // TECHSPEEDDIALSIMPLE_H
