#ifndef TECHSLIDEREDIT_H
/**
 * @file techslideredit.h
 * @brief 滑块编辑器控件声明，提供可调整数值的滑动控件与编辑交互。
 *
 * 详细说明: 该文件声明可用于显示与编辑数值的滑块控件，支持信号/槽与格式化显示。
 *
 * 使用示例:
 * @code
 * #include "techslideredit.h"
 * TechSliderEdit *s = new TechSliderEdit(parent);
 * s->setRange(0,100);
 * @endcode
 */
#define TECHSLIDEREDIT_H

#include "animationmanager.h"  //


#include <QtMath>
#include <QWidget>
#include <QLineEdit>
#include <QSlider>
#include <QVBoxLayout>  // 添加垂直布局
#include <QHBoxLayout>
#include <QDoubleValidator>
#include <QPropertyAnimation>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>  // 添加鼠标事件头文件

class TechSliderEdit : public QWidget
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
    Q_PROPERTY(bool presetButtonsVisible READ presetButtonsVisible WRITE setPresetButtonsVisible)
public:
    /**
     * @brief 构造 TechSliderEdit 控件
     *
     * 提供带标签的滑块 + 编辑框组合控件，适用于需要同时显示数值与滑块控制的场景。
     * 内部通过 `m_conversionFactor` 支持小数精度，通过 `m_slider` 与 `m_lineEdit` 双向同步，
     * 并支持将值映射到 Modbus 地址以便与外部设备联动。
     *
     * @param parent 父控件，通常传入 `this`。
     *
     * @note 构造不会触发初始值改变信号，建议创建后调用 `setRange()` 与 `setPrecision()` 进行初始化。
     * @warning 在高频率更新（>50Hz）场景下，频繁触发 UI 重绘会影响性能，应合并更新或使用后台数据采样。
     * @since 1.0.0
     *
     * 使用示例:
     * @code
     * auto *s = new TechSliderEdit(parent);
     * s->setRange(0, 100);
     * s->setPrecision(1);
     * s->setValue(50.0);
     * @endcode
     */
    explicit TechSliderEdit(QWidget *parent = nullptr);

    /**
     * @brief 获取当前数值
     *
     * 返回当前控件表示的数值（已按 `m_precision` 处理）。
     *
     * @return 当前数值（double）。
     * @note 该返回值为内部缓存值，频繁调用开销低。
     */
    double value() const;

    /**
     * @brief 获取最小允许值
     *
     * @return 最小值（double）。
     * @see setRange()
     */
    double minimum() const;

    /**
     * @brief 获取最大允许值
     *
     * @return 最大值（double）。
     * @see setRange()
     */
    double maximum() const;

    /**
     * @brief 获取数值显示精度（小数位）
     *
     * @return 精度（整数，小数位数）。
     * @see setPrecision()
     */
    int precision() const;


    /**
     * @brief 设置可选范围
     *
     * 将滑块与文本编辑的允许范围设置为 [min, max]。
     * 内部会自动调整 `m_slider` 的步长并校准当前值到新范围内。
     *
     * @param min 最小值，必须小于 `max`。
     * @param max 最大值，必须大于 `min`。
     * @note 当 `min >= max` 时函数会忽略请求并在调试模式下打印警告。
     * @warning 修改范围可能触发 `valueChanged()`，调用方应准备接收该信号。
     * @since 1.0.0
     */
    void setRange(double min, double max);

    /**
     * 使用示例:
     * @code
     * slider->setRange(0, 100);
     * slider->setPrecision(1);
     * @endcode
     */

    /**
     * @brief 设置单步增量
     *
     * 控制当使用键盘或微调时每次改变的数值（与 `QSlider` 的单步类似）。
     *
     * @param step 单步值，需为正数。
     * @warning 过小的 step 可能导致显示精度与实际变化感受不一致。
     */
    void setSingleStep(double step);

    /**
     * @brief 设置数值后缀（单位文本）
     *
     * 该后缀显示于编辑框右侧，用于标注单位（例如 "%"、"kW" 等）。
     *
     * @param suffix 非空字符串表示单位文本，传空字符串代表不显示后缀。
     * @note 传入空字符串不会触发信号，只会影响显示格式。
     */
    void setSuffix(const QString &suffix);

    // 样式设置
    /**
     * @brief 设置主颜色（用于滑块、进度条等主视觉元素）
     * @param color 主颜色
     */
    void setPrimaryColor(const QColor &color);

    /**
     * @brief 设置次要颜色（用于标签、文字等次要元素）
     * @param color 次要颜色
     */
    void setSecondaryColor(const QColor &color);

    /**
     * @brief 设置发光颜色（用于发光/高亮效果）
     * @param color 发光颜色
     */
    void setGlowColor(const QColor &color);

    /**
     * @brief 设置发光强度（影响阴影模糊半径或明亮度）
     * @param intensity 强度（整数，越大越明显）
     */
    void setGlowIntensity(int intensity);

    /**
     * @brief 应用内置“科技蓝”主题样式
     *
     * 便捷方法，将主/次/发光颜色设为预定义的科技蓝样式。
     */
    void setTechBlueStyle();

    // 启用/禁用效果
    /**
     * @brief 启用或禁用发光效果
     * @param enable true 启用，false 禁用
     * @note 启用时会增加绘制开销
     */
    void enableGlowEffect(bool enable);

    /**
     * @brief 启用或禁用扫描线效果
     * @param enable true 启用，false 禁用
     */
    void enableScanLine(bool enable);

    /**
     * @brief 启用或禁用脉冲效果
     * @param enable true 启用，false 禁用
     */
    void enablePulseEffect(bool enable);

    // 新增函数
    /**
     * @brief 获取标签文本
     * @return 标签字符串
     */
    QString labelText() const;

    /**
     * @brief 设置标签文本
     * @param text 文本字符串
     */
    void setLabelText(const QString &text);

    /**
     * @brief 设置标签宽度（像素）
     * @param width 宽度
     */
    void setLabelWidth(int width);

    /**
     * @brief 设置范围标签宽度（像素）
     * @param width 宽度
     */
    void setRangeLabelsWidth(int width);

    /**
     * @brief 返回范围标签是否可见
     * @return true 表示可见
     */
    bool rangeLabelsVisible() const { return m_minLabel && m_minLabel->isVisible(); }

    /**
     * @brief 设置范围标签可见性
     * @param visible true 可见
     */
    void setRangeLabelsVisible(bool visible);

    /**
     * @brief 返回预设按钮是否可见
     * @return true 表示可见
     */
    bool presetButtonsVisible() const { return m_lowBtn && m_lowBtn->isVisible(); }

    /**
     * @brief 设置预设按钮可见性
     * @param visible true 可见
     */
    void setPresetButtonsVisible(bool visible);

    // Modbus相关函数
    /**
     * @brief 设置 Modbus 地址，用于外部映射
     * @param address Modbus 地址（寄存器编号）
     */
    void setModbusAddress(int address);  // 设置Modbus地址

    /**
     * 使用示例:
     * @code
     * slider->setModbusAddress(1001);
     * @endcode
     */

    /**
     * @brief 获取当前 Modbus 地址
     * @return Modbus 地址
     */
    int modbusAddress() const { return m_modbusAddress; }

    /**
     * @brief 由 Modbus 数据更新控件显示值
     * @param value 从 Modbus 读取到的数值
     * @note 该函数仅更新显示，不负责写回 Modbus
     */
    void updateFromModbus(double value);  // 从Modbus更新值

    /**
     * 使用示例:
     * @code
     * slider->updateFromModbus(42.0);
     * @endcode
     */

public slots:
    /**
     * 使用示例:
     * @code
     * slider->setValue(55.5);
     * @endcode
     */
    void setValue(double value);
    void setMinimum(double min);
    void setMaximum(double max);
    void setPrecision(int precision);

    void updateAnimation();


signals:
    void valueChanged(double value);
    void valueChangedWithRecord(double oldValue, double newValue);  // 新增信号
    void editingFinished();
    void modbusAddressChanged(int address);  // Modbus地址改变信号

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;


private slots:
    void onLineEditTextChanged();
    void onSliderValueChanged(int value);
    void onLineEditEditingFinished();
    void onPresetButtonClicked();
    void updateScanLine();
    void updatePulseEffect();

private:
    void setupUI();
    void setupConnections();
    void updateLineEditValidator();
    void updateSliderFromValue();
    void updateLineEditFromValue();
    void updateRangeLabels();  // 新增：更新范围标签
    void setupPresetButtons(); // 新增：设置预设按钮样式
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

        // 设置LineEdit样式
        QString lineEditStyle = QString(
                                    "QLineEdit {"
                                    "    background-color: rgba(20, 20, 40, 220);"
                                    "    border: 2px solid %1;"
                                    "    border-radius: 8px;"
                                    "    padding: 8px 12px;"
                                    "    font-family: 'Segoe UI', 'Microsoft YaHei';"
                                    "    font-size: 14px;"
                                    "    font-weight: bold;"
                                    "    color: %2;"
                                    "    selection-background-color: %3;"
                                    "    selection-color: white;"
                                    "}"
                                    "QLineEdit:focus {"
                                    "    border: 2px solid %3;"
                                    "    background-color: rgba(30, 30, 50, 240);"
                                    "}"
                                    ).arg(m_primaryColor.name())
                                    .arg(m_secondaryColor.name())
                                    .arg(m_glowColor.name());

        m_lineEdit->setStyleSheet(lineEditStyle);

        // 设置按钮样式
        setupPresetButtons();

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
    QLineEdit *m_lineEdit;
    QSlider *m_slider;
    QVBoxLayout *m_mainLayout;  // 改为垂直布局
    QHBoxLayout *m_firstRowLayout;  // 第一行布局
    QHBoxLayout *m_secondRowLayout;  // 第二行布局

    QPushButton *m_lowBtn;
    QPushButton *m_midBtn;
    QPushButton *m_highBtn;

    // 属性
    double m_value;
    double m_oldValue;  // 保存旧值
    double m_minimum;
    double m_maximum;
    double m_singleStep;
    int m_precision;
    QString m_suffix;
    int m_modbusAddress;      // Modbus地址

    // 鼠标拖动相关
    bool m_sliderPressed;
    QPoint m_lastMousePos;

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
    // 新增成员变量
    QLabel *m_label;          // 描述标签
    QLabel *m_minLabel;       // 最小值标签
    QLabel *m_maxLabel;       // 最大值标签
    QHBoxLayout *m_sliderLayout; // 滑块布局
    QWidget *m_sliderContainer;  // 滑块容器
    int m_scanLinePos;
    int m_scanLineWidth;
    qreal m_pulseAlpha;
    bool m_pulseDirection;

    // 转换因子（用于处理小数）
    int m_conversionFactor;

signals:
};


#endif // TECHSLIDEREDIT_H
