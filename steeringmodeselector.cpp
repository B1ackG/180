#include "steeringmodeselector.h"
#include <QDebug>

SteeringModeSelector::SteeringModeSelector(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(STEER_FRONT_BACK)  // 默认前后轮转向
    , m_buttonStyle(TechPushButton::StyleHolographic)
    , m_activeColor(0, 200, 255)       // 激活状态颜色（科技蓝）
    , m_inactiveColor(80, 80, 100)     // 非激活状态颜色（灰色）
    , m_textColor(Qt::white)
{
    // 设置默认样式
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("background-color: transparent;");

    initUI();
    updateButtonStyles();
}

SteeringModeSelector::~SteeringModeSelector()
{
}

void SteeringModeSelector::initUI()
{
    // 主布局 - 垂直布局（标题+水平按钮）
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(10);

    // 标题
    m_titleLabel = new QLabel("转向模式选择", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    m_mainLayout->addWidget(m_titleLabel);

    // 按钮布局 - 水平布局
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->setSpacing(10);

    // 创建按钮（恢复完整文本）
    m_btnFrontBack = new TechPushButton("前后轮转向", this);
    m_btnFrontOnly = new TechPushButton("前轮转向", this);
    m_btnParallel = new TechPushButton("平移模式", this);
    m_btnLateral = new TechPushButton("横向移动", this);
    m_btnRotate = new TechPushButton("原地旋转", this);

    // 设置按钮属性
    m_btnFrontBack->setObjectName("btnFrontBack");
    m_btnFrontOnly->setObjectName("btnFrontOnly");
    m_btnParallel->setObjectName("btnParallel");
    m_btnLateral->setObjectName("btnLateral");
    m_btnRotate->setObjectName("btnRotate");

    // 设置推荐尺寸（标题+5个按钮的水平布局）
    // 推荐总宽度：5个按钮 × 120px + 4个间距 × 10px + 左右边距 = 600px + 40px = 640px
    // 推荐高度：标题30px + 按钮40px + 间距 = 约80px
    // 因此推荐控件尺寸：宽度640px，高度80px
    int btnWidth = 120;  // 按钮宽度（适应较长的中文文本）
    int btnHeight = 40;  // 按钮高度（适合手指触摸）

    m_btnFrontBack->setFixedSize(btnWidth, btnHeight);
    m_btnFrontOnly->setFixedSize(btnWidth, btnHeight);
    m_btnParallel->setFixedSize(btnWidth, btnHeight);
    m_btnLateral->setFixedSize(btnWidth, btnHeight);
    m_btnRotate->setFixedSize(btnWidth, btnHeight);

    // 启用点击动画
    m_btnFrontBack->enableClickAnimation(true);
    m_btnFrontOnly->enableClickAnimation(true);
    m_btnParallel->enableClickAnimation(true);
    m_btnLateral->enableClickAnimation(true);
    m_btnRotate->enableClickAnimation(true);

    // 启用悬停动画
    m_btnFrontBack->enableHoverAnimation(true);
    m_btnFrontOnly->enableHoverAnimation(true);
    m_btnParallel->enableHoverAnimation(true);
    m_btnLateral->enableHoverAnimation(true);
    m_btnRotate->enableHoverAnimation(true);

    // 启用文字发光
    m_btnFrontBack->setTextGlow(true);
    m_btnFrontOnly->setTextGlow(true);
    m_btnParallel->setTextGlow(true);
    m_btnLateral->setTextGlow(true);
    m_btnRotate->setTextGlow(true);

    // 添加到水平布局
    m_buttonLayout->addWidget(m_btnFrontBack);
    m_buttonLayout->addWidget(m_btnFrontOnly);
    m_buttonLayout->addWidget(m_btnParallel);
    m_buttonLayout->addWidget(m_btnLateral);
    m_buttonLayout->addWidget(m_btnRotate);

    // 添加到主布局
    m_mainLayout->addLayout(m_buttonLayout);

    // 连接信号槽
    connect(m_btnFrontBack, &TechPushButton::clicked, this, &SteeringModeSelector::onFrontBackClicked);
    connect(m_btnFrontOnly, &TechPushButton::clicked, this, &SteeringModeSelector::onFrontOnlyClicked);
    connect(m_btnParallel, &TechPushButton::clicked, this, &SteeringModeSelector::onParallelClicked);
    connect(m_btnLateral, &TechPushButton::clicked, this, &SteeringModeSelector::onLateralClicked);
    connect(m_btnRotate, &TechPushButton::clicked, this, &SteeringModeSelector::onRotateClicked);
}

QString SteeringModeSelector::modeText(SteeringMode mode) const
{
    switch(mode) {
    case STEER_FRONT_BACK: return "前后轮转向";
    case STEER_FRONT_ONLY: return "前轮转向";
    case STEER_PARALLEL:   return "平移模式";
    case STEER_LATERAL:    return "横向移动";
    case STEER_ROTATE:     return "原地旋转";
    default: return "未知模式";
    }
}

int SteeringModeSelector::modeModbusValue(SteeringMode mode) const
{
    switch(mode) {
    case STEER_FRONT_BACK: return 0;
    case STEER_FRONT_ONLY: return 1;
    case STEER_PARALLEL:   return 2;
    case STEER_LATERAL:    return 3;
    case STEER_ROTATE:     return 4;
    default: return 0;
    }
}

void SteeringModeSelector::setButtonStyle(TechPushButton::ButtonStyle style)
{
    m_buttonStyle = style;

    m_btnFrontBack->setButtonStyle(style);
    m_btnFrontOnly->setButtonStyle(style);
    m_btnParallel->setButtonStyle(style);
    m_btnLateral->setButtonStyle(style);
    m_btnRotate->setButtonStyle(style);

    updateButtonStyles();
}

void SteeringModeSelector::setCurrentMode(SteeringMode mode)
{
    if (m_currentMode == mode)
        return;

    m_currentMode = mode;

    // 更新按钮样式
    updateButtonStyles();

    // 发出信号，附带Modbus值
    emit modeChanged(mode, modeModbusValue(mode));
}

void SteeringModeSelector::setActiveColor(const QColor &color)
{
    m_activeColor = color;
    updateButtonStyles();
}

void SteeringModeSelector::setInactiveColor(const QColor &color)
{
    m_inactiveColor = color;
    updateButtonStyles();
}

void SteeringModeSelector::setTextColor(const QColor &color)
{
    m_textColor = color;

    m_btnFrontBack->setTextColor(color);
    m_btnFrontOnly->setTextColor(color);
    m_btnParallel->setTextColor(color);
    m_btnLateral->setTextColor(color);
    m_btnRotate->setTextColor(color);

    // 更新标题颜色
    m_titleLabel->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;")
                                    .arg(color.name()));
}

void SteeringModeSelector::updateButtonStyles()
{
    // 重置所有按钮为非激活状态
    m_btnFrontBack->setPrimaryColor(m_inactiveColor);
    m_btnFrontBack->setGlowColor(m_inactiveColor);
    m_btnFrontBack->setTextColor(m_textColor);
    m_btnFrontBack->enablePulseEffect(false);

    m_btnFrontOnly->setPrimaryColor(m_inactiveColor);
    m_btnFrontOnly->setGlowColor(m_inactiveColor);
    m_btnFrontOnly->setTextColor(m_textColor);
    m_btnFrontOnly->enablePulseEffect(false);

    m_btnParallel->setPrimaryColor(m_inactiveColor);
    m_btnParallel->setGlowColor(m_inactiveColor);
    m_btnParallel->setTextColor(m_textColor);
    m_btnParallel->enablePulseEffect(false);

    m_btnLateral->setPrimaryColor(m_inactiveColor);
    m_btnLateral->setGlowColor(m_inactiveColor);
    m_btnLateral->setTextColor(m_textColor);
    m_btnLateral->enablePulseEffect(false);

    m_btnRotate->setPrimaryColor(m_inactiveColor);
    m_btnRotate->setGlowColor(m_inactiveColor);
    m_btnRotate->setTextColor(m_textColor);
    m_btnRotate->enablePulseEffect(false);

    // 激活当前模式的按钮
    switch(m_currentMode) {
    case STEER_FRONT_BACK:
        m_btnFrontBack->setPrimaryColor(m_activeColor);
        m_btnFrontBack->setGlowColor(m_activeColor.lighter(150));
        m_btnFrontBack->setTextColor(Qt::white);
        // m_btnFrontBack->enablePulseEffect(true);
        break;

    case STEER_FRONT_ONLY:
        m_btnFrontOnly->setPrimaryColor(m_activeColor);
        m_btnFrontOnly->setGlowColor(m_activeColor.lighter(150));
        m_btnFrontOnly->setTextColor(Qt::white);
        // m_btnFrontOnly->enablePulseEffect(true);
        break;

    case STEER_PARALLEL:
        m_btnParallel->setPrimaryColor(m_activeColor);
        m_btnParallel->setGlowColor(m_activeColor.lighter(150));
        m_btnParallel->setTextColor(Qt::white);
        // m_btnParallel->enablePulseEffect(true);
        break;

    case STEER_LATERAL:
        m_btnLateral->setPrimaryColor(m_activeColor);
        m_btnLateral->setGlowColor(m_activeColor.lighter(150));
        m_btnLateral->setTextColor(Qt::white);
        // m_btnLateral->enablePulseEffect(true);
        break;

    case STEER_ROTATE:
        m_btnRotate->setPrimaryColor(m_activeColor);
        m_btnRotate->setGlowColor(m_activeColor.lighter(150));
        m_btnRotate->setTextColor(Qt::white);
        // m_btnRotate->enablePulseEffect(true);
        break;
    }

    // 设置按钮样式
    m_btnFrontBack->setButtonStyle(m_buttonStyle);
    m_btnFrontOnly->setButtonStyle(m_buttonStyle);
    m_btnParallel->setButtonStyle(m_buttonStyle);
    m_btnLateral->setButtonStyle(m_buttonStyle);
    m_btnRotate->setButtonStyle(m_buttonStyle);

    // 更新按钮
    m_btnFrontBack->update();
    m_btnFrontOnly->update();
    m_btnParallel->update();
    m_btnLateral->update();
    m_btnRotate->update();
}

void SteeringModeSelector::onFrontBackClicked()
{
    setCurrentMode(STEER_FRONT_BACK);
}

void SteeringModeSelector::onFrontOnlyClicked()
{
    setCurrentMode(STEER_FRONT_ONLY);
}

void SteeringModeSelector::onParallelClicked()
{
    setCurrentMode(STEER_PARALLEL);
}

void SteeringModeSelector::onLateralClicked()
{
    setCurrentMode(STEER_LATERAL);
}

void SteeringModeSelector::onRotateClicked()
{
    setCurrentMode(STEER_ROTATE);
}
