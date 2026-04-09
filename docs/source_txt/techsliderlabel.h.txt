#ifndef TECHSLIDERLABEL_H
/**
 * @file techsliderlabel.h
 * @brief 滑块标签控件的声明，显示与滑块关联的文本信息。
 *
 * 详细说明: 包含用于显示当前滑块值、名称或单位的标签控件声明。
 *
 * 使用示例:
 * @code
 * #include "techsliderlabel.h"
 * TechSliderLabel *l = new TechSliderLabel(parent);
 * l->setText("速度");
 * @endcode
 */
#define TECHSLIDERLABEL_H

#include "animationmanager.h"
#include <QtMath>
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QEvent>
#include <QMouseEvent>
#include <QElapsedTimer>

class TechSliderLabel : public QWidget
{
    Q_OBJECT
    // 自定义属性
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int precision READ precision WRITE setPrecision)
    // 新增属性
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText)
    Q_PROPERTY(bool rangeLabelsVisible READ rangeLabelsVisible WRITE setRangeLabelsVisible)
public:
    /**
     * @brief 构造 TechSliderLabel 控件
     *
     * 用于只读显示滑块值与标签的控件，常用于仪表盘或仪表列表中。
     * 内部采用 `m_slider` 绘制轨迹、`m_valueLabel` 显示格式化数值。
     *
     * @param parent 父控件。
     * @since 1.0.0
     *
    * 使用示例:
    * @code
    * auto *lbl = new TechSliderLabel(parent);
    * lbl->setRange(0, 100);
    * lbl->setSuffix("kW");
    * lbl->setValue(42);
    * @endcode
     */
    explicit TechSliderLabel(QWidget *parent = nullptr);

    /**
     * @brief 获取当前显示值
     * @return 当前值（double）
     */
    double value() const;

    /**
     * @brief 获取最小值
     * @return 最小允许值
     */
    double minimum() const;

    /**
     * @brief 获取最大值
     * @return 最大允许值
     */
    double maximum() const;

    /**
     * @brief 获取小数精度
     * @return 小数位数
     */
    int precision() const;

    /**
     * @brief 设置显示范围
     *
     * 将可视化的范围设置为 [min, max]，并更新刻度与标签显示。
     *
     * @param min 最小值
     * @param max 最大值
     * @note 若 min >= max，则此调用将被忽略并在调试模式打印提示。
     */
    void setRange(double min, double max);

    /**
     * 使用示例:
     * @code
     * lbl->setRange(0, 100);
     * lbl->setValue(42);
     * @endcode
     */

    /**
     * @brief 设置单步值
     * @param step 单步增量，用于精确显示/调整场景
     */
    void setSingleStep(double step);

    /**
     * @brief 设置数值后缀（单位）
     * @param suffix 后缀字符串，传空表示不显示后缀
     */
    void setSuffix(const QString &suffix);

    // 样式设置
    /**
     * @brief 设置主颜色（滑块/刻度主色）
     * @param color 主颜色
     */
    void setPrimaryColor(const QColor &color);

    /**
     * @brief 设置次要颜色（标签/文字颜色）
     * @param color 次要颜色
     */
    void setSecondaryColor(const QColor &color);

    /**
     * @brief 设置发光颜色（用于高亮/边缘发光）
     * @param color 发光颜色
     */
    void setGlowColor(const QColor &color);

    /**
     * @brief 设置发光强度
     * @param intensity 强度数值
     */
    void setGlowIntensity(int intensity);

    /**
     * @brief 应用内置科技蓝主题样式
     */
    void setTechBlueStyle();

    // 启用/禁用效果
    void enableGlowEffect(bool enable);
    void enableScanLine(bool enable);
    void enablePulseEffect(bool enable);

    // 新增函数
    /**
     * @brief 获取标签文本
     * @return 文本字符串
     */
    QString labelText() const;

    /**
     * @brief 设置标签文本
     * @param text 文本字符串
     */
    void setLabelText(const QString &text);

    /**
     * 使用示例:
     * @code
     * lbl->setLabelText("速度");
     * @endcode
     */

    /**
     * @brief 设置标签宽度
     * @param width 像素宽度
     */
    void setLabelWidth(int width);

    /**
     * @brief 设置范围标签宽度
     * @param width 像素宽度
     */
    void setRangeLabelsWidth(int width);

    /**
     * @brief 返回范围标签是否可见
     * @return true 可见
     */
    bool rangeLabelsVisible() const { return m_minLabel && m_minLabel->isVisible(); }

    /**
     * @brief 设置范围标签可见性
     * @param visible true 可见
     */
    void setRangeLabelsVisible(bool visible);

    void setModbusAddress(int address);

    /**
     * @brief 设置力控模式开启状态
     * @param enabled true 开启，颜色随值变化
     */
    void setForceControlMode(bool enabled);
    bool forceControlMode() const { return m_forceControlEnabled; }

    /**
     * 使用示例:
     * @code
     * lbl->setModbusAddress(1001);
     * @endcode
     */
    int modbusAddress() const { return m_modbusAddress; }
    void updateFromModbus(double value);

public slots:
    /**
     * @brief 设置当前值并更新显示
     * @param value 要设置的数值
     *
     * 使用示例:
     * @code
     * lbl->setValue(75.5);
     * @endcode
     */
    void setValue(double value);
    void setMinimum(double min);
    void setMaximum(double max);
    void setPrecision(int precision);
    void updateAnimation();

signals:
    void valueChanged(double value);
    void valueChangedWithRecord(double oldValue, double newValue);
    void modbusAddressChanged(int address);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSliderValueChanged(int value);
    void updateScanLine();
    void updatePulseEffect();

private:
    void requestRepaint();

private:
    void setupUI();
    void setupConnections();
    void updateSliderFromValue();
    void updateValueLabelFromValue();
    void updateRangeLabels();
    double sliderToValue(int sliderVal) const;
    int valueToSlider(double value) const;

    void applyTechStyle()
    {
        // 计算转换因子（用于支持小数）
        m_conversionFactor = qPow(10, m_precision);

        // 设置标签样式
        QString labelStyle = QString(
                                 "QLabel {"
                                 "    background: transparent;"
                                 "    font-family: 'Segoe UI', 'Microsoft YaHei';"
                                 "    font-size: 13px;"
                                 "    font-weight: bold;"
                                 "    color: %1;"
                                 "    padding: 0px 5px;"
                                 "}"
                                 ).arg(m_secondaryColor.name());

        m_label->setStyleSheet(labelStyle);
        m_minLabel->setStyleSheet(labelStyle);
        m_maxLabel->setStyleSheet(labelStyle);

        // 设置值显示标签样式
        QString valueLabelStyle = QString(
                                      "QLabel {"
                                      "    background: transparent;"  // 透明背景
                                      "    border: none;"             // 去掉边框
                                      "    font-family: 'Segoe UI', 'Microsoft YaHei';"
                                      "    font-size: 14px;"
                                      "    font-weight: bold;"
                                      "    color: %1;"
                                      "    min-width: 80px;"
                                      "    text-align: center;"
                                      "    padding: 0px;"  // 去掉内边距
                                      "}"
                                      ).arg(m_secondaryColor.name());

        m_valueLabel->setStyleSheet(valueLabelStyle);

        // 设置Slider样式
        QString sliderStyle = QString(
                                  "QSlider {"
                                  "    background: transparent;"
                                  "}"
                                  "QSlider::groove:horizontal {"
                                  "    border: 1px solid #999999;"
                                  "    height: 8px;"
                                  "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                                  "        stop:0 rgba(40, 40, 60, 200), "
                                  "        stop:1 rgba(60, 60, 80, 200));"
                                  "    border-radius: 4px;"
                                  "}"
                                  "QSlider::sub-page:horizontal {"
                                  "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                                  "        stop:0 %1, "
                                  "        stop:1 %2);"
                                  "    border: 1px solid %3;"
                                  "    height: 8px;"
                                  "    border-radius: 4px;"
                                  "}"
                                  "QSlider::add-page:horizontal {"
                                  "    background: rgba(80, 80, 100, 100);"
                                  "    border: 1px solid #777777;"
                                  "    height: 8px;"
                                  "    border-radius: 4px;"
                                  "}"
                                  "QSlider::handle:horizontal {"
                                  "    background: qradialgradient("
                                  "        cx:0.5, cy:0.5, radius:0.5,"
                                  "        fx:0.5, fy:0.5,"
                                  "        stop:0 white,"
                                  "        stop:0.5 %1,"
                                  "        stop:1 %2);"
                                  "    width: 20px;"
                                  "    height: 20px;"
                                  "    margin: -6px 0;"
                                  "    border: 2px solid white;"
                                  "    border-radius: 10px;"
                                  "}"
                                  "QSlider::handle:horizontal:hover {"
                                  "    background: qradialgradient("
                                  "        cx:0.5, cy:0.5, radius:0.5,"
                                  "        fx:0.5, fy:0.5,"
                                  "        stop:0 white,"
                                  "        stop:0.3 %3,"
                                  "        stop:1 %1);"
                                  "    border: 2px solid %3;"
                                  "}"
                                  ).arg(m_primaryColor.name())
                                  .arg(m_secondaryColor.name())
                                  .arg(m_glowColor.name());

        m_slider->setStyleSheet(sliderStyle);

        // 设置滑块容器样式
        if (m_sliderContainer) {
            m_sliderContainer->setStyleSheet("QWidget { background: transparent; }");
        }

        // 添加阴影效果
        if (m_glowEnabled) {
            m_shadowEffect = new QGraphicsDropShadowEffect(this);
            m_shadowEffect->setBlurRadius(15);
            m_shadowEffect->setColor(m_glowColor);
            m_shadowEffect->setOffset(0, 0);
            this->setGraphicsEffect(m_shadowEffect);
        }

        // 更新最小最大值标签
        updateRangeLabels();
    }

private:
    QLabel *m_label;          // 描述标签
    QLabel *m_valueLabel;     // 值显示标签（替代LineEdit）
    QSlider *m_slider;
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_firstRowLayout;
    QHBoxLayout *m_secondRowLayout;

    // 属性
    double m_value;
    double m_oldValue;
    double m_minimum;
    double m_maximum;
    double m_singleStep;
    int m_precision;
    QString m_suffix;
    int m_modbusAddress;

    // 样式
    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_glowColor;
    QGraphicsDropShadowEffect *m_shadowEffect;

    // 动画效果
    bool m_glowEnabled;
    bool m_scanLineEnabled;
    bool m_pulseEnabled;
    QTimer *m_scanTimer;
    QTimer *m_pulseTimer;

    QLabel *m_minLabel;
    QLabel *m_maxLabel;
    QHBoxLayout *m_sliderLayout;
    QWidget *m_sliderContainer;
    int m_scanLinePos;
    int m_scanLineWidth;
    qreal m_pulseAlpha;
    bool m_pulseDirection;
    bool m_repaintPending;
    int m_minRepaintIntervalMs;
    QElapsedTimer m_repaintElapsed;

    // 力控开关控制变色
    bool m_forceControlEnabled;
    QColor m_originalPrimaryColor;
    QColor m_originalSecondaryColor;
    QColor m_originalGlowColor;

    // 转换因子（用于处理小数）
    int m_conversionFactor;
};

#endif // TECHSLIDERLABEL_H
