#include "techslideredit.h"
#include <QDebug>
#include <QLabel>
#include <QIntValidator>
#include <QRegularExpressionValidator>

TechSliderEdit::TechSliderEdit(QWidget *parent)
    : QWidget{parent}, m_value(50.0)
    , m_oldValue(50.0)
    , m_minimum(0.0)
    , m_maximum(100.0)
    , m_singleStep(1.0)
    , m_precision(0)
    , m_suffix("")
    , m_modbusAddress(-1)      // 默认无Modbus地址
    , m_sliderPressed(false)
    , m_primaryColor(QColor(0, 200, 255))      // 科技蓝
    , m_secondaryColor(QColor(0, 200, 255))   // 紫色
    , m_glowColor(QColor(0, 255, 255, 180))    // 青色辉光
    , m_glowEnabled(true)
    , m_scanLineEnabled(false)
    , m_pulseEnabled(false)
    , m_scanLinePos(0)
    , m_scanLineWidth(60)
    , m_pulseAlpha(0.3)
    , m_pulseDirection(true)
    , m_conversionFactor(1)
{
    setupUI();
    setupConnections();
    applyTechStyle();

    // 设置验证器
    updateLineEditValidator();

    // 设置默认值
    setValue(50.0);

    // 安装事件过滤器
    m_lineEdit->installEventFilter(this);

    // // 初始化扫描线定时器
    // m_scanTimer = new QTimer(this);
    // connect(m_scanTimer, &QTimer::timeout, this, &TechSliderEdit::updateScanLine);
    // if (m_scanLineEnabled) {
    //     m_scanTimer->start(30); // 30ms更新一次
    // }

    // // 初始化脉冲效果定时器
    // m_pulseTimer = new QTimer(this);
    // connect(m_pulseTimer, &QTimer::timeout, this, &TechSliderEdit::updatePulseEffect);
    // if (m_pulseEnabled) {
    //     m_pulseTimer->start(50); // 50ms更新一次
    // }
    AnimationManager::instance()->registerWidget(this);
    setTechBlueStyle();
}
void TechSliderEdit::setupUI()
{
    // 创建控件
    m_label = new QLabel(this);
    m_lineEdit = new QLineEdit(this);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_minLabel = new QLabel(this);
    m_maxLabel = new QLabel(this);

    m_lowBtn = new QPushButton("低速", this);
    m_midBtn = new QPushButton("中速", this);
    m_highBtn = new QPushButton("高速", this);

    m_lowBtn->setFixedWidth(60);
    m_midBtn->setFixedWidth(60);
    m_highBtn->setFixedWidth(60);

    // 设置滑块属性
    m_slider->setMinimum(0);
    m_slider->setMaximum(1000);
    m_slider->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 禁用鼠标事件，使其不可拖动

    // 创建主垂直布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(10);

    // 创建第一行布局（标签 + LineEdit + 按钮）
    m_firstRowLayout = new QHBoxLayout();
    m_firstRowLayout->setContentsMargins(0, 0, 0, 0);
    m_firstRowLayout->setSpacing(10);

    // 添加标签和LineEdit到第一行
    m_label->setText("参数:");
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setFixedWidth(120);

    m_lineEdit->setFixedWidth(50);
    m_lineEdit->setPlaceholderText("输入数值");

    m_firstRowLayout->addWidget(m_label);
    m_firstRowLayout->addWidget(m_lineEdit);
    m_firstRowLayout->addWidget(m_lowBtn);
    m_firstRowLayout->addWidget(m_midBtn);
    m_firstRowLayout->addWidget(m_highBtn);

    // 添加占位符使LineEdit与Slider对齐
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

    // 设置鼠标跟踪
    setMouseTracking(false);
}

void TechSliderEdit::setupConnections()
{
    // LineEdit文本变化
    connect(m_lineEdit, &QLineEdit::textChanged,
            this, &TechSliderEdit::onLineEditTextChanged);

    // LineEdit编辑完成
    connect(m_lineEdit, &QLineEdit::editingFinished,
            this, &TechSliderEdit::onLineEditEditingFinished);

    // Slider值变化
    connect(m_slider, &QSlider::valueChanged,
            this, &TechSliderEdit::onSliderValueChanged);

    // 预设按钮点击
    connect(m_lowBtn, &QPushButton::clicked, this, &TechSliderEdit::onPresetButtonClicked);
    connect(m_midBtn, &QPushButton::clicked, this, &TechSliderEdit::onPresetButtonClicked);
    connect(m_highBtn, &QPushButton::clicked, this, &TechSliderEdit::onPresetButtonClicked);
}



void TechSliderEdit::updateRangeLabels()
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

void TechSliderEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // // 绘制扫描线效果
    // if (m_scanLineEnabled && m_scanLinePos > 0) {
    //     // 绘制在LineEdit上
    //     QRect lineEditRect = m_lineEdit->geometry();
    //     painter.save();

    //     // 创建扫描线渐变
    //     QLinearGradient gradient(lineEditRect.left() + m_scanLinePos,
    //                              lineEditRect.top(),
    //                              lineEditRect.left() + m_scanLinePos + m_scanLineWidth,
    //                              lineEditRect.top());
    //     gradient.setColorAt(0, QColor(255, 255, 255, 0));
    //     gradient.setColorAt(0.5, QColor(255, 255, 255, 100));
    //     gradient.setColorAt(1, QColor(255, 255, 255, 0));

    //     painter.setBrush(gradient);
    //     painter.setPen(Qt::NoPen);
    //     painter.drawRect(lineEditRect.left() + m_scanLinePos,
    //                      lineEditRect.top(),
    //                      m_scanLineWidth,
    //                      lineEditRect.height());

    //     painter.restore();
    // }

    // // 绘制脉冲效果
    // if (m_pulseEnabled && m_pulseAlpha > 0.1) {
    //     QRect sliderRect = m_slider->geometry();
    //     painter.save();

    //     QColor pulseColor = m_glowColor;
    //     pulseColor.setAlphaF(m_pulseAlpha * 0.5);

    //     painter.setBrush(Qt::NoBrush);
    //     painter.setPen(QPen(pulseColor, 2));

    //     // 获取滑块的位置和大小（使用QStyle的方法）
    //     QStyleOptionSlider option;
    //     option.initFrom(m_slider);
    //     option.minimum = m_slider->minimum();
    //     option.maximum = m_slider->maximum();
    //     option.sliderPosition = m_slider->value();
    //     option.sliderValue = m_slider->value();
    //     option.orientation = m_slider->orientation();
    //     option.state |= QStyle::State_Enabled;

    //     // 获取滑块手柄的矩形
    //     QRect handleRect = m_slider->style()->subControlRect(
    //         QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, m_slider);

    //     // 将滑块手柄的坐标转换到TechSliderEdit的坐标空间
    //     handleRect.translate(sliderRect.topLeft());

    //     // 绘制脉冲光环（比滑块稍大）
    //     QRect pulseRect = handleRect.adjusted(-8, -8, 8, 8);
    //     painter.drawEllipse(pulseRect);

    //     painter.restore();
    // }

    QWidget::paintEvent(event);
}

void TechSliderEdit::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_scanLineEnabled) {
        m_scanLineWidth = width() / 4;
    }
}

bool TechSliderEdit::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_lineEdit) {
        if (event->type() == QEvent::FocusIn) {
            // 获得焦点时增强辉光效果
            if (m_glowEnabled && m_shadowEffect) {
                m_shadowEffect->setBlurRadius(25);
                m_shadowEffect->setColor(m_glowColor.lighter(120));
            }
        } else if (event->type() == QEvent::FocusOut) {
            // 失去焦点时恢复辉光效果
            if (m_glowEnabled && m_shadowEffect) {
                m_shadowEffect->setBlurRadius(15);
                m_shadowEffect->setColor(m_glowColor);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// 值转换函数
double TechSliderEdit::sliderToValue(int sliderVal) const
{
    return m_minimum + (sliderVal * (m_maximum - m_minimum)) / (m_slider->maximum() - m_slider->minimum());
}

int TechSliderEdit::valueToSlider(double value) const
{
    return ((value - m_minimum) * (m_slider->maximum() - m_slider->minimum())) / (m_maximum - m_minimum);
}

// 私有槽函数
void TechSliderEdit::onLineEditTextChanged()
{
    // 实时验证，但不立即更新滑块（等待编辑完成）
}

void TechSliderEdit::onLineEditEditingFinished()
{
    QString text = m_lineEdit->text();
    if (text.isEmpty()) {
        m_lineEdit->setText(QString::number(m_value, 'f', m_precision));
        return;
    }

    bool ok;
    double newValue = text.toDouble(&ok);

    if (ok) {
        // 限制在范围内
        if (newValue < m_minimum) newValue = m_minimum;
        if (newValue > m_maximum) newValue = m_maximum;
        if (qAbs(newValue - m_value) > 0.0001) {
            m_oldValue = m_value;  // 保存旧值

        setValue(newValue);
            emit valueChangedWithRecord(m_oldValue, m_value);  // 发出带记录的信号
        }
        emit editingFinished();
    } else {
        // 恢复原值
        m_lineEdit->setText(QString::number(m_value, 'f', m_precision));
    }
}

void TechSliderEdit::onPresetButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;

    double ratio = 0.0;
    if (btn == m_lowBtn) ratio = 0.1;
    else if (btn == m_midBtn) ratio = 0.4;
    else if (btn == m_highBtn) ratio = 0.8;

    double newValue = m_maximum * ratio;
    
    // 如果有精度要求，进行四舍五入
    if (m_precision > 0) {
        newValue = qRound(newValue * m_conversionFactor) / (double)m_conversionFactor;
    } else {
        newValue = qRound(newValue);
    }

    if (qAbs(newValue - m_value) > 0.0001) {
        m_oldValue = m_value;
        setValue(newValue);
        emit valueChangedWithRecord(m_oldValue, m_value);
    }
    emit editingFinished();
}

void TechSliderEdit::onSliderValueChanged(int sliderValue)
{
    double newValue = sliderToValue(sliderValue);
    setValue(newValue);  // 程序调用时正常更新

    // 四舍五入到指定精度
    if (m_precision > 0) {
        newValue = qRound(newValue * m_conversionFactor) / (double)m_conversionFactor;
    }

    if (qAbs(newValue - m_value) > 0.0001) {
         m_oldValue = m_value;  // 保存旧值
        m_value = newValue;
        updateLineEditFromValue();
        emit valueChanged(m_value);
        emit valueChangedWithRecord(m_oldValue, m_value);  // 发出带记录的信号
    }

    // 触发重绘（用于脉冲效果）
    update();
}

void TechSliderEdit::updateLineEditValidator()
{
    // 根据精度设置验证器
    if (m_precision == 0) {
        // 整数
        m_lineEdit->setValidator(new QIntValidator(m_minimum, m_maximum, this));
    } else {
        // 小数
        QString pattern = QString("^-?\\d*\\.?\\d{0,%1}$").arg(m_precision);
        m_lineEdit->setValidator(new QRegularExpressionValidator(
            QRegularExpression(pattern), this));
    }
}

void TechSliderEdit::updateSliderFromValue()
{
    int sliderPos = valueToSlider(m_value);
    m_slider->blockSignals(true);
    m_slider->setValue(sliderPos);
    m_slider->blockSignals(false);
}

void TechSliderEdit::updateLineEditFromValue()
{
    QString text = QString::number(m_value, 'f', m_precision);
    // if (!m_suffix.isEmpty()) {
    //     text += " " + m_suffix;
    // }

    m_lineEdit->blockSignals(true);
    m_lineEdit->setText(text);
    m_lineEdit->blockSignals(false);
}

void TechSliderEdit::updateScanLine()
{
    if (m_scanLineEnabled) {
        m_scanLinePos += 3;
        if (m_scanLinePos > width()) {
            m_scanLinePos = -m_scanLineWidth;
        }
        update();
    }
}

void TechSliderEdit::updatePulseEffect()
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
        update();
    }
}

// 公共成员函数
double TechSliderEdit::value() const
{
    return m_value;
}

double TechSliderEdit::minimum() const
{
    return m_minimum;
}

double TechSliderEdit::maximum() const
{
    return m_maximum;
}

int TechSliderEdit::precision() const
{
    return m_precision;
}

void TechSliderEdit::setValue(double value)
{
    if (value < m_minimum) value = m_minimum;
    if (value > m_maximum) value = m_maximum;

    if (qAbs(m_value - value) > 0.0001) {
         m_oldValue = m_value;  // 保存旧值
        m_value = value;
        updateSliderFromValue();
        updateLineEditFromValue();
        emit valueChanged(m_value);
    }
}

void TechSliderEdit::setMinimum(double min)
{
    m_minimum = min;
    if (m_value < min) setValue(min);
    updateLineEditValidator();
    updateRangeLabels();  // 新增：更新标签显示
}

void TechSliderEdit::setMaximum(double max)
{
    m_maximum = max;
    if (m_value > max) setValue(max);
    updateLineEditValidator();
    updateRangeLabels();  // 新增：更新标签显示
}

void TechSliderEdit::setPrecision(int precision)
{
    m_precision = qMax(0, precision);
    m_conversionFactor = qPow(10, m_precision);
    updateLineEditValidator();
    updateLineEditFromValue();
    updateRangeLabels();  // 新增：更新标签显示
}

void TechSliderEdit::setRange(double min, double max)
{
    m_minimum = min;
    m_maximum = max;
    if (m_value < min) setValue(min);
    if (m_value > max) setValue(max);
    updateLineEditValidator();
    updateRangeLabels();  // 新增：更新标签显示
}

void TechSliderEdit::setSingleStep(double step)
{
    m_singleStep = step;
    // 对于Slider，我们需要设置tick间隔
    int sliderStep = step * (m_slider->maximum() - m_slider->minimum()) / (m_maximum - m_minimum);
    m_slider->setSingleStep(qMax(1, sliderStep));
    m_slider->setPageStep(sliderStep * 5);
}

void TechSliderEdit::setSuffix(const QString &suffix)
{
    m_suffix = suffix;
    updateLineEditFromValue();
    updateRangeLabels();  // 新增：更新标签显示
}


// 新增：设置标签文本的函数
void TechSliderEdit::setLabelText(const QString &text)
{
    if (m_label) {
        m_label->setText(text);
    }
}

// 新增：获取标签文本的函数
QString TechSliderEdit::labelText() const
{
    return m_label ? m_label->text() : QString();
}

// 新增：设置标签宽度的函数
void TechSliderEdit::setLabelWidth(int width)
{
    if (m_label) {
        m_label->setFixedWidth(width);
    }
}

// 新增：设置范围标签宽度的函数
void TechSliderEdit::setRangeLabelsWidth(int width)
{
    if (m_minLabel && m_maxLabel) {
        m_minLabel->setFixedWidth(width);
        m_maxLabel->setFixedWidth(width);
    }
}

// 新增：设置是否显示范围标签的函数
void TechSliderEdit::setRangeLabelsVisible(bool visible)
{
    if (m_minLabel && m_maxLabel) {
        m_minLabel->setVisible(visible);
        m_maxLabel->setVisible(visible);
    }
}

void TechSliderEdit::setPresetButtonsVisible(bool visible)
{
    if (m_lowBtn) m_lowBtn->setVisible(visible);
    if (m_midBtn) m_midBtn->setVisible(visible);
    if (m_highBtn) m_highBtn->setVisible(visible);
}

void TechSliderEdit::setPrimaryColor(const QColor &color)
{
    m_primaryColor = color;
    applyTechStyle();
}

void TechSliderEdit::setSecondaryColor(const QColor &color)
{
    m_secondaryColor = color;
    applyTechStyle();
}

void TechSliderEdit::setGlowColor(const QColor &color)
{
    m_glowColor = color;
    applyTechStyle();
}

void TechSliderEdit::setGlowIntensity(int intensity)
{
    if (m_shadowEffect) {
        m_shadowEffect->setBlurRadius(qBound(5, intensity, 50));
    }
}

void TechSliderEdit::enableGlowEffect(bool enable)
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

void TechSliderEdit::enableScanLine(bool enable)
{
    m_scanLineEnabled = enable;
    // if (enable && !m_scanTimer->isActive()) {
    //     m_scanTimer->start(30);
    // } else if (!enable && m_scanTimer->isActive()) {
    //     m_scanTimer->stop();
    // }
    update();
}

void TechSliderEdit::enablePulseEffect(bool enable)
{
    m_pulseEnabled = enable;
    // if (enable && !m_pulseTimer->isActive()) {
    //     m_pulseTimer->start(50);
    // } else if (!enable && m_pulseTimer->isActive()) {
    //     m_pulseTimer->stop();
    // }
    update();
}
void TechSliderEdit::setTechBlueStyle()
{

}
void TechSliderEdit::updateAnimation()
{
    bool needUpdate = false;

    // 1. 更新扫描线效果
    if (m_scanLineEnabled) {
        m_scanLinePos += 3;
        if (m_scanLinePos > width()) {
            m_scanLinePos = -m_scanLineWidth;
        }
        needUpdate = true;
    }

    // 2. 更新脉冲效果
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

    // 3. 如果需要更新，就刷新界面
    if (needUpdate) {
        update();
    }
}




void TechSliderEdit::setModbusAddress(int address)
{
    if (m_modbusAddress != address) {
        m_modbusAddress = address;
        emit modbusAddressChanged(address);
    }
}

void TechSliderEdit::updateFromModbus(double value)
{
    // 从Modbus更新值，但不触发用户操作的信号
    if (qAbs(m_value - value) > 0.0001) {
        m_oldValue = m_value;
        m_value = value;

        // 阻塞信号更新UI
        m_slider->blockSignals(true);
        updateSliderFromValue();
        m_slider->blockSignals(false);

        m_lineEdit->blockSignals(true);
        updateLineEditFromValue();
        m_lineEdit->blockSignals(false);

        // 触发重绘
        update();
    }
}

void TechSliderEdit::setupPresetButtons()
{
    QString btnStyle = QString(
        "QPushButton {"
        "    background-color: rgba(30, 30, 60, 200);"
        "    border: 1px solid %1;"
        "    border-radius: 4px;"
        "    color: %2;"
        "    font-family: 'Segoe UI', 'Microsoft YaHei';"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    min-height: 25px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(40, 40, 80, 220);"
        "    border: 1px solid %3;"
        "    color: white;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(20, 20, 40, 240);"
        "    border: 2px solid %3;"
        "    padding-top: 2px;"
        "    padding-left: 2px;"
        "}"
    ).arg(m_primaryColor.name())
     .arg(m_secondaryColor.name())
     .arg(m_glowColor.name());

    m_lowBtn->setStyleSheet(btnStyle);
    m_midBtn->setStyleSheet(btnStyle);
    m_highBtn->setStyleSheet(btnStyle);
}
#if 0
        // 只发出值改变信号，不发出带记录的信号
        emit valueChanged(m_value);

        update();
    }
}
#endif








