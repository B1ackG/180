#include "techsliderlabel.h"

#include <QDebug>
#include <QLabel>

TechSliderLabel::TechSliderLabel(QWidget *parent)
    : QWidget(parent)
    , m_value(50.0)
    , m_oldValue(50.0)
    , m_minimum(0.0)
    , m_maximum(100.0)
    , m_singleStep(1.0)
    , m_precision(0)
    , m_suffix("")
    , m_modbusAddress(-1)
    , m_primaryColor(QColor(0, 200, 255))
    , m_secondaryColor(QColor(0, 200, 255))
    , m_glowColor(QColor(0, 255, 255, 180))
    , m_glowEnabled(true)
    , m_scanLineEnabled(false)
    , m_pulseEnabled(false)
    , m_scanLinePos(0)
    , m_scanLineWidth(60)
    , m_pulseAlpha(0.3)
    , m_pulseDirection(true)
    , m_repaintPending(false)
    , m_minRepaintIntervalMs(100)
    , m_forceControlEnabled(false)
    , m_conversionFactor(1)
{
    setupUI();
    setupConnections();
    
    // 保存初始颜色，用于力控模式切换
    m_originalPrimaryColor = m_primaryColor;
    m_originalSecondaryColor = m_secondaryColor;
    m_originalGlowColor = m_glowColor;

    applyTechStyle();

    // 设置默认值
    setValue(50.0);

    AnimationManager::instance()->registerWidget(this);
    setTechBlueStyle();
}

void TechSliderLabel::setupUI()
{
    // 创建控件
    m_label = new QLabel(this);
    m_valueLabel = new QLabel(this);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_minLabel = new QLabel(this);
    m_maxLabel = new QLabel(this);

    // 设置滑块属性
    m_slider->setMinimum(0);
    m_slider->setMaximum(1000);
    m_slider->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 禁用鼠标事件，使其不可拖动

    // 创建主垂直布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(10);

    // 创建第一行布局（标签 + 值标签）
    m_firstRowLayout = new QHBoxLayout();
    m_firstRowLayout->setContentsMargins(0, 0, 0, 0);
    m_firstRowLayout->setSpacing(10);

    // 添加标签和值标签到第一行
    m_label->setText("参数:");
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setFixedWidth(120);

    m_valueLabel->setFixedWidth(120);
    m_valueLabel->setAlignment(Qt::AlignCenter);

    m_firstRowLayout->addWidget(m_label);
    m_firstRowLayout->addWidget(m_valueLabel);

    // 添加占位符使值标签与Slider对齐
    QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_firstRowLayout->addItem(spacer);

    m_mainLayout->addLayout(m_firstRowLayout);

    // 创建第二行布局（最小值标签 + Slider + 最大值标签）
    m_secondRowLayout = new QHBoxLayout();
    m_secondRowLayout->setContentsMargins(80, 0, 0, 0);
    m_secondRowLayout->setSpacing(10);

    // 创建滑块容器和布局
    m_sliderContainer = new QWidget(this);
    m_sliderLayout = new QHBoxLayout(m_sliderContainer);
    m_sliderLayout->setContentsMargins(0, 0, 0, 0);
    m_sliderLayout->setSpacing(8);

    // 设置标签宽度和文本对齐
    m_minLabel->setFixedWidth(80);
    m_maxLabel->setFixedWidth(80);
    m_minLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_maxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 添加最小值标签
    m_sliderLayout->addWidget(m_minLabel);

    // 添加滑块
    m_sliderLayout->addWidget(m_slider);

    // 添加最大值标签
    m_sliderLayout->addWidget(m_maxLabel);

    // 将滑块容器添加到第二行布局
    m_secondRowLayout->addWidget(m_sliderContainer);
    m_mainLayout->addLayout(m_secondRowLayout);

    // 更新最小最大值标签
    updateRangeLabels();
}

void TechSliderLabel::setupConnections()
{
    // 先设置初始值
    setValue(m_value);
    // Slider值变化
    connect(m_slider, &QSlider::valueChanged,
            this, &TechSliderLabel::onSliderValueChanged);
}

void TechSliderLabel::updateRangeLabels()
{
    // 格式化数值显示
    QString minText = QString::number(m_minimum, 'f', m_precision);
    QString maxText = QString::number(m_maximum, 'f', m_precision);

    // 如果有后缀，添加后缀
    if (!m_suffix.isEmpty()) {
        minText += " " + m_suffix;
        maxText += " " + m_suffix;
    }

    m_minLabel->setText(minText);
    m_maxLabel->setText(maxText);
}

void TechSliderLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QWidget::paintEvent(event);
}

void TechSliderLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_scanLineEnabled) {
        m_scanLineWidth = width() / 4;
    }
}

double TechSliderLabel::sliderToValue(int sliderVal) const
{
    return m_minimum + (sliderVal * (m_maximum - m_minimum)) / (m_slider->maximum() - m_slider->minimum());
}

int TechSliderLabel::valueToSlider(double value) const
{
    return ((value - m_minimum) * (m_slider->maximum() - m_slider->minimum())) / (m_maximum - m_minimum);
}

void TechSliderLabel::onSliderValueChanged(int sliderValue)
{
    double newValue = sliderToValue(sliderValue);
    setValue(newValue);

    // 四舍五入到指定精度
    if (m_precision > 0) {
        newValue = qRound(newValue * m_conversionFactor) / (double)m_conversionFactor;
    }

    if (qAbs(newValue - m_value) > 0.0001) {
        m_oldValue = m_value;
        m_value = newValue;
        updateValueLabelFromValue();
        emit valueChanged(m_value);
        emit valueChangedWithRecord(m_oldValue, m_value);
    }

    // 触发重绘
    requestRepaint();
}

void TechSliderLabel::updateSliderFromValue()
{
    int sliderPos = valueToSlider(m_value);
    m_slider->blockSignals(true);
    m_slider->setValue(sliderPos);
    m_slider->blockSignals(false);
}

void TechSliderLabel::updateValueLabelFromValue()
{
    QString text = QString::number(m_value, 'f', m_precision);
    if (!m_suffix.isEmpty()) {
        text += " " + m_suffix;
    }

    m_valueLabel->setText(text);
}

void TechSliderLabel::updateScanLine()
{
    if (m_scanLineEnabled) {
        m_scanLinePos += 3;
        if (m_scanLinePos > width()) {
            m_scanLinePos = -m_scanLineWidth;
        }
        requestRepaint();
    }
}

void TechSliderLabel::updatePulseEffect()
{
    if (m_pulseEnabled) {
        if (m_pulseDirection) {
            m_pulseAlpha += 0.05;
            if (m_pulseAlpha >= 1.0) {
                m_pulseAlpha = 1.0;
                m_pulseDirection = false;
            }
        } else {
            m_pulseAlpha -= 0.05;
            if (m_pulseAlpha <= 0.3) {
                m_pulseAlpha = 0.3;
                m_pulseDirection = true;
            }
        }
        requestRepaint();
    }
}

double TechSliderLabel::value() const
{
    return m_value;
}

double TechSliderLabel::minimum() const
{
    return m_minimum;
}

double TechSliderLabel::maximum() const
{
    return m_maximum;
}

int TechSliderLabel::precision() const
{
    return m_precision;
}

void TechSliderLabel::setValue(double value)
{
    if (value < m_minimum) value = m_minimum;
    if (value > m_maximum) value = m_maximum;

    if (qAbs(m_value - value) > 0.0001) {
        m_oldValue = m_value;
        m_value = value;
        updateSliderFromValue();
        
        // 如果开启了力控模式，根据数值更新颜色
        if (m_forceControlEnabled) {
            double range = m_maximum - m_minimum;
            if (range > 0) {
                // 计算离极值的程度 (0-1), 0表示在中间, 1表示在最大或最小值
                double center = (m_maximum + m_minimum) / 2.0;
                double deviation = qAbs(m_value - center) / (range / 2.0);
                
                // 限制在0-1范围
                deviation = qBound(0.0, deviation, 1.0);
                
                // 插值颜色: 原色 -> 红色 (#FF3333)
                QColor targetRed(255, 51, 51);
                
                auto interpolate = [](const QColor& start, const QColor& end, double t) {
                    return QColor(
                        start.red() + (end.red() - start.red()) * t,
                        start.green() + (end.green() - start.green()) * t,
                        start.blue() + (end.blue() - start.blue()) * t,
                        start.alpha() + (end.alpha() - start.alpha()) * t
                    );
                };
                
                m_primaryColor = interpolate(m_originalPrimaryColor, targetRed, deviation);
                m_secondaryColor = interpolate(m_originalSecondaryColor, targetRed, deviation);
                m_glowColor = interpolate(m_originalGlowColor, QColor(255, 51, 51, 180), deviation);
                
                applyTechStyle();
            }
        }
        
        updateValueLabelFromValue();
        emit valueChanged(m_value);
    }
}

void TechSliderLabel::setMinimum(double min)
{
    m_minimum = min;
    if (m_value < min) setValue(min);
    updateRangeLabels();
}

void TechSliderLabel::setMaximum(double max)
{
    m_maximum = max;
    if (m_value > max) setValue(max);
    updateRangeLabels();
}

void TechSliderLabel::setPrecision(int precision)
{
    m_precision = qMax(0, precision);
    m_conversionFactor = qPow(10, m_precision);
    updateValueLabelFromValue();
    updateRangeLabels();
}

void TechSliderLabel::setRange(double min, double max)
{
    m_minimum = min;
    m_maximum = max;
    if (m_value < min) setValue(min);
    if (m_value > max) setValue(max);
    updateRangeLabels();
}

void TechSliderLabel::setSingleStep(double step)
{
    m_singleStep = step;
    int sliderStep = step * (m_slider->maximum() - m_slider->minimum()) / (m_maximum - m_minimum);
    m_slider->setSingleStep(qMax(1, sliderStep));
    m_slider->setPageStep(sliderStep * 5);
}

void TechSliderLabel::setSuffix(const QString &suffix)
{
    m_suffix = suffix;
    updateValueLabelFromValue();
    updateRangeLabels();
}

QString TechSliderLabel::labelText() const
{
    return m_label ? m_label->text() : QString();
}

void TechSliderLabel::setLabelText(const QString &text)
{
    if (m_label) {
        m_label->setText(text);
    }
}

void TechSliderLabel::setLabelWidth(int width)
{
    if (m_label) {
        m_label->setFixedWidth(width);
    }
}

void TechSliderLabel::setRangeLabelsWidth(int width)
{
    if (m_minLabel && m_maxLabel) {
        m_minLabel->setFixedWidth(width);
        m_maxLabel->setFixedWidth(width);
    }
}

void TechSliderLabel::setRangeLabelsVisible(bool visible)
{
    if (m_minLabel && m_maxLabel) {
        m_minLabel->setVisible(visible);
        m_maxLabel->setVisible(visible);
    }
}

void TechSliderLabel::setPrimaryColor(const QColor &color)
{
    m_primaryColor = color;
    applyTechStyle();
}

void TechSliderLabel::setSecondaryColor(const QColor &color)
{
    m_secondaryColor = color;
    applyTechStyle();
}

void TechSliderLabel::setGlowColor(const QColor &color)
{
    m_glowColor = color;
    applyTechStyle();
}

void TechSliderLabel::setGlowIntensity(int intensity)
{
    if (m_shadowEffect) {
        m_shadowEffect->setBlurRadius(qBound(5, intensity, 50));
    }
}

void TechSliderLabel::enableGlowEffect(bool enable)
{
    m_glowEnabled = enable;
    if (m_shadowEffect) {
        m_shadowEffect->setEnabled(enable);
    }
    if (!enable && m_shadowEffect) {
        this->setGraphicsEffect(nullptr);
        delete m_shadowEffect;
        m_shadowEffect = nullptr;
    } else if (enable && !m_shadowEffect) {
        m_shadowEffect = new QGraphicsDropShadowEffect(this);
        m_shadowEffect->setBlurRadius(15);
        m_shadowEffect->setColor(m_glowColor);
        m_shadowEffect->setOffset(0, 0);
        this->setGraphicsEffect(m_shadowEffect);
    }
}

void TechSliderLabel::enableScanLine(bool enable)
{
    m_scanLineEnabled = enable;
    requestRepaint();
}

void TechSliderLabel::enablePulseEffect(bool enable)
{
    m_pulseEnabled = enable;
    requestRepaint();
}

void TechSliderLabel::setTechBlueStyle()
{
    // 设置科技蓝样式
    setPrimaryColor(QColor(0, 200, 255));
    setSecondaryColor(QColor(0, 200, 255));
    setGlowColor(QColor(0, 255, 255, 180));
}

void TechSliderLabel::updateAnimation()
{
    bool needUpdate = false;

    // 更新扫描线效果
    if (m_scanLineEnabled) {
        m_scanLinePos += 3;
        if (m_scanLinePos > width()) {
            m_scanLinePos = -m_scanLineWidth;
        }
        needUpdate = true;
    }

    // 更新脉冲效果
    if (m_pulseEnabled) {
        if (m_pulseDirection) {
            m_pulseAlpha += 0.05;
            if (m_pulseAlpha >= 1.0) {
                m_pulseAlpha = 1.0;
                m_pulseDirection = false;
            }
        } else {
            m_pulseAlpha -= 0.05;
            if (m_pulseAlpha <= 0.3) {
                m_pulseAlpha = 0.3;
                m_pulseDirection = true;
            }
        }
        needUpdate = true;
    }

    if (needUpdate) {
        requestRepaint();
    }
}

void TechSliderLabel::setModbusAddress(int address)
{
    if (m_modbusAddress != address) {
        m_modbusAddress = address;
        emit modbusAddressChanged(address);
    }
}

void TechSliderLabel::setForceControlMode(bool enabled)
{
    if (m_forceControlEnabled != enabled) {
        m_forceControlEnabled = enabled;
        if (!enabled) {
            // 恢复原色
            m_primaryColor = m_originalPrimaryColor;
            m_secondaryColor = m_originalSecondaryColor;
            m_glowColor = m_originalGlowColor;
            applyTechStyle();
        } else {
            // 开启后立即根据当前值重算颜色
            // 我们需要让setValue逻辑生效，但setValue有值差异检查
            // 这里我们手动触发一次颜色更新，或者临时修改m_value触发setValue
            double currentVal = m_value;
            m_value = -999999.0; // 强制不同
            setValue(currentVal);
        }
    }
}

void TechSliderLabel::updateFromModbus(double value)
{
    // 从Modbus更新值，但不触发用户操作的信号
    if (qAbs(m_value - value) > 0.0001) {
        m_oldValue = m_value;
        m_value = value;

        // 阻塞信号更新UI
        m_slider->blockSignals(true);
        updateSliderFromValue();
        m_slider->blockSignals(false);

        updateValueLabelFromValue();

        // 只发出值改变信号，不发出带记录的信号
        emit valueChanged(m_value);

        requestRepaint();
    }
}

void TechSliderLabel::requestRepaint()
{
    if (!m_repaintElapsed.isValid()) {
        m_repaintElapsed.start();
        update();
        return;
    }

    const qint64 elapsed = m_repaintElapsed.elapsed();
    if (elapsed >= m_minRepaintIntervalMs) {
        m_repaintElapsed.restart();
        update();
        return;
    }

    if (m_repaintPending) {
        return;
    }

    m_repaintPending = true;
    const int remainMs = m_minRepaintIntervalMs - static_cast<int>(elapsed);
    QTimer::singleShot(remainMs, this, [this]() {
        m_repaintPending = false;
        m_repaintElapsed.restart();
        update();
    });
}
