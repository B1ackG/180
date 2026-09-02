#include "speedmodeselector.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QDebug>

#include "modebuttonstyler.h"

SpeedModeSelector::SpeedModeSelector(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(MODE_MEDIUM)
    , m_buttonStyle(TechPushButton::StyleDefault)
    , m_activeColor(0, 150, 255)      // 激活状态颜色（蓝色）
    , m_inactiveColor(100, 100, 100)  // 非激活状态颜色（灰色）
    , m_textColor(Qt::white)
    , m_glowAnimation(nullptr)
{
    // 设置默认样式
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("background-color: transparent;");

    initUI();
    createAnimation();
    updateButtonStyles();
}

SpeedModeSelector::~SpeedModeSelector()
{
    delete m_glowAnimation;
}

void SpeedModeSelector::initUI()
{
    // 主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(15);

    // 标题
    m_titleLabel = new QLabel("速度模式选择", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    m_mainLayout->addWidget(m_titleLabel);

    // 按钮布局
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(10);

    // 创建按钮
    m_btnLow = new TechPushButton("低速模式", this);
    m_btnMedium = new TechPushButton("中速模式", this);
    m_btnHigh = new TechPushButton("高速模式", this);

    ModeButtonStyler::configureInteractiveButton(m_btnLow, QSize(100, 44), "btnLowSpeed");
    ModeButtonStyler::configureInteractiveButton(m_btnMedium, QSize(100, 44), "btnMediumSpeed");
    ModeButtonStyler::configureInteractiveButton(m_btnHigh, QSize(100, 44), "btnHighSpeed");

    // 添加到布局
    m_buttonLayout->addWidget(m_btnLow);
    m_buttonLayout->addWidget(m_btnMedium);
    m_buttonLayout->addWidget(m_btnHigh);
    m_buttonLayout->addStretch();

    m_mainLayout->addLayout(m_buttonLayout);

    // 标签布局（显示当前模式和描述）
    m_labelLayout = new QVBoxLayout();
    m_labelLayout->setSpacing(5);

    m_modeLabel = new QLabel(this);
    m_modeLabel->setAlignment(Qt::AlignCenter);
    m_modeLabel->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");

    m_descLabel = new QLabel(this);
    m_descLabel->setAlignment(Qt::AlignCenter);
    m_descLabel->setStyleSheet("color: #AAAAAA; font-size: 12px;");
    m_descLabel->setWordWrap(true);

    m_labelLayout->addWidget(m_modeLabel);
    m_labelLayout->addWidget(m_descLabel);
    m_mainLayout->addLayout(m_labelLayout);

    m_mainLayout->addStretch();

    // 连接信号槽
    connect(m_btnLow, &TechPushButton::clicked, this, &SpeedModeSelector::onLowSpeedClicked);
    connect(m_btnMedium, &TechPushButton::clicked, this, &SpeedModeSelector::onMediumSpeedClicked);
    connect(m_btnHigh, &TechPushButton::clicked, this, &SpeedModeSelector::onHighSpeedClicked);

    // 初始显示
    updateButtonStyles();
    m_modeLabel->setText(modeText(m_currentMode));
    m_descLabel->setText(modeDescription(m_currentMode));
}

QString SpeedModeSelector::modeText(SpeedMode mode) const
{
    switch(mode) {
    case MODE_LOW: return "低速模式";
    case MODE_MEDIUM: return "中速模式";
    case MODE_HIGH: return "高速模式";
    default: return "未知模式";
    }
}

QString SpeedModeSelector::modeDescription(SpeedMode mode) const
{
    switch(mode) {
    case MODE_LOW: return "低功耗运行，适用于长时间工作";
    case MODE_MEDIUM: return "平衡性能与功耗，推荐使用";
    case MODE_HIGH: return "高性能运行，适用于快速任务";
    default: return "";
    }
}

void SpeedModeSelector::setButtonStyle(TechPushButton::ButtonStyle style)
{
    m_buttonStyle = style;

    updateButtonStyles();
}

void SpeedModeSelector::setCurrentMode(SpeedMode mode)
{
    if (m_currentMode == mode)
        return;

    m_currentMode = mode;

    // 更新按钮样式
    updateButtonStyles();

    // 更新显示
    m_modeLabel->setText(modeText(mode));
    m_descLabel->setText(modeDescription(mode));

    // 触发动画
    if (m_glowAnimation) {
        m_glowAnimation->stop();
        m_glowAnimation->setStartValue(0.0);
        m_glowAnimation->setEndValue(1.0);
        m_glowAnimation->start();
    }

    // 发出信号
    emit modeChanged(mode);
}

void SpeedModeSelector::setActiveColor(const QColor &color)
{
    m_activeColor = color;
    updateButtonStyles();
}

void SpeedModeSelector::setInactiveColor(const QColor &color)
{
    m_inactiveColor = color;
    updateButtonStyles();
}

void SpeedModeSelector::setTextColor(const QColor &color)
{
    m_textColor = color;
    ModeButtonStyler::applyTextColor({m_btnLow, m_btnMedium, m_btnHigh}, color);

    m_titleLabel->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;")
                                    .arg(color.name()));
}

void SpeedModeSelector::updateButtonStyles()
{
    ModeButtonStyler::applyGroupStyle(
        {m_btnLow, m_btnMedium, m_btnHigh},
        static_cast<int>(m_currentMode),
        m_activeColor,
        m_inactiveColor,
        m_textColor,
        m_buttonStyle,
        false);
}

void SpeedModeSelector::createAnimation()
{
    m_glowAnimation = new QPropertyAnimation(this, "windowOpacity");
    m_glowAnimation->setDuration(500);
    m_glowAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void SpeedModeSelector::onLowSpeedClicked()
{
    setCurrentMode(MODE_LOW);
}

void SpeedModeSelector::onMediumSpeedClicked()
{
    setCurrentMode(MODE_MEDIUM);
}

void SpeedModeSelector::onHighSpeedClicked()
{
    setCurrentMode(MODE_HIGH);
}
