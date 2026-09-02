#include "techpushbutton.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QMouseEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QEnterEvent>
#endif

TechPushButton::TechPushButton(QWidget *parent) : QPushButton(parent),
    m_style(StyleDefault),
    m_state(StateNormal),
    m_primaryColor(52, 152, 219),     // 科技蓝
    m_secondaryColor(155, 89, 182),   // 紫色
    m_glowColor(0, 255, 255, 100),    // 青色
    m_textColor(Qt::white),
    m_borderWidth(2),
    m_cornerRadius(8),
    m_iconSize(24, 24),
    m_hoverAnimationEnabled(false),
    m_clickAnimationEnabled(true),
    m_pulseEffectEnabled(false),
    m_scanLineEnabled(false),
    m_dataFlowEnabled(false),
    m_3dEffectEnabled(false),
    m_textGlowEnabled(false),
    m_glowOpacity(0.0),
    m_pulseScale(1.0),
    m_scanPosition(0.0),
    m_dataFlowProgress(0.0),
    m_random(new QRandomGenerator(QDateTime::currentMSecsSinceEpoch())),
    m_shadowEffect(nullptr),
    m_cacheValid(false)
{
    init();


}
TechPushButton::TechPushButton(const QString &text, QWidget *parent) :
    TechPushButton(parent)
{
    setText(text);
}

TechPushButton::TechPushButton(const QIcon &icon, const QString &text, QWidget *parent) :
    TechPushButton(parent)
{
    setIcon(icon);
    setText(text);
}

TechPushButton::~TechPushButton()
{
    delete m_random;
}

void TechPushButton::init()
{
    // 设置按钮基本属性
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);

    // 创建阴影效果（默认关闭，仅激活态发光时打开）
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(0);
    m_shadowEffect->setColor(m_glowColor);
    m_shadowEffect->setOffset(0, 0);
    setGraphicsEffect(m_shadowEffect);

    // 设置动画
    setupAnimations();

    // 初始化数据流定时器
    // m_dataFlowTimer = new QTimer(this);
    // connect(m_dataFlowTimer, &QTimer::timeout, this, [this]() {
    //     m_dataFlowProgress += 0.02;
    //     if (m_dataFlowProgress > 1.0) m_dataFlowProgress = 0.0;
    //     update();
    // });
     // ======= 改为注册到全局动画管理器 =======
    AnimationManager::instance()->registerWidget(this);
    // 添加默认字体设置
    QFont defaultFont = this->font();
    if (defaultFont.pointSize() <= 0) {
        defaultFont.setPointSize(14);
        this->setFont(defaultFont);
    }

    connect(this, &QAbstractButton::toggled, this, &TechPushButton::applySelectionVisual);

    // 初始状态
    updateNormalEffect();
}

void TechPushButton::applySelectionVisual(bool active)
{
    setTextGlow(active);
    if (active) {
        setGlowOpacity(0.55);
        return;
    }
    enablePulseEffect(false);
    enableScanLine(false);
    setGlowOpacity(0.0);
}

void TechPushButton::setupAnimations()
{
    // 发光动画
    m_glowAnimation = new QPropertyAnimation(this, "glowOpacity");
    m_glowAnimation->setDuration(300);
    m_glowAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // 脉冲动画
    m_pulseAnimation = new QPropertyAnimation(this, "pulseScale");
    m_pulseAnimation->setDuration(1500);
    m_pulseAnimation->setLoopCount(-1); // 无限循环
    m_pulseAnimation->setEasingCurve(QEasingCurve::InOutSine);
    m_pulseAnimation->setStartValue(0.95);
    m_pulseAnimation->setEndValue(1.05);

    // 扫描线动画
    m_scanAnimation = new QPropertyAnimation(this, "scanPosition");
    m_scanAnimation->setDuration(2000);
    m_scanAnimation->setLoopCount(-1);
    m_scanAnimation->setStartValue(0.0);
    m_scanAnimation->setEndValue(1.0);
}

void TechPushButton::updateAnimation()
{
    // 1. 更新扫描线位置
    if (m_scanLineEnabled) {
        m_scanPosition += 0.008; // 调整速度
        if (m_scanPosition > 1.0) {
            m_scanPosition = 0.0;
        }
    }

    // 2. 更新数据流进度（原来由m_dataFlowTimer处理）
    if (m_dataFlowEnabled) {
        m_dataFlowProgress += 0.02;
        if (m_dataFlowProgress > 1.0) {
            m_dataFlowProgress = 0.0;
        }
    }

    // 3. 如果有任何一个效果启用，就更新界面
    if (m_scanLineEnabled || m_dataFlowEnabled) {
        update();
    }
}

// 设置按钮样式
void TechPushButton::setButtonStyle(ButtonStyle style)
{
    if (m_style != style) {
        m_style = style;

        // 根据样式设置默认颜色
        switch (style) {
        case StyleDefault:
            m_primaryColor = QColor(52, 152, 219);  // 科技蓝
            m_secondaryColor = QColor(155, 89, 182);
            m_glowColor = QColor(0, 255, 255, 100);
            break;
        case StyleHolographic:
            m_primaryColor = QColor(0, 204, 255, 150);  // 青色半透明
            m_secondaryColor = QColor(255, 0, 255, 150);
            m_glowColor = QColor(255, 255, 255, 100);
            break;
        case StyleEnergy:
            m_primaryColor = QColor(255, 100, 0);      // 橙色
            m_secondaryColor = QColor(255, 220, 0);
            m_glowColor = QColor(255, 200, 0, 150);
            break;
        case StyleCircuit:
            m_primaryColor = QColor(46, 204, 113);     // 电路绿
            m_secondaryColor = QColor(41, 128, 185);
            m_glowColor = QColor(0, 255, 127, 100);
            break;
        case StyleCyber:
            m_primaryColor = QColor(138, 43, 226);     // 蓝紫色
            m_secondaryColor = QColor(255, 20, 147);
            m_glowColor = QColor(255, 0, 255, 150);
            break;
        }

        m_cacheValid = false;
        update();
    }
}

// 设置主颜色
void TechPushButton::setPrimaryColor(const QColor &color)
{
    if (m_primaryColor != color) {
        m_primaryColor = color;
        m_cacheValid = false;
        update();
    }
}

// 设置次要颜色
void TechPushButton::setSecondaryColor(const QColor &color)
{
    if (m_secondaryColor != color) {
        m_secondaryColor = color;
        m_cacheValid = false;
        update();
    }
}

// 设置发光颜色
void TechPushButton::setGlowColor(const QColor &color)
{
    if (m_glowColor != color) {
        m_glowColor = color;
        if (m_shadowEffect) {
            m_shadowEffect->setColor(m_glowColor);
        }
        update();
    }
}

// 设置边框宽度
void TechPushButton::setBorderWidth(int width)
{
    if (m_borderWidth != width && width >= 0) {
        m_borderWidth = width;
        m_cacheValid = false;
        update();
    }
}

// 启用悬停动画
void TechPushButton::enableHoverAnimation(bool enable)
{
    m_hoverAnimationEnabled = enable;
}

// 启点击动画
void TechPushButton::enableClickAnimation(bool enable)
{
    m_clickAnimationEnabled = enable;
}

// 启用脉冲效果
void TechPushButton::enablePulseEffect(bool enable)
{
    m_pulseEffectEnabled = enable;
    if (enable) {
        startPulse();
    } else {
        stopPulse();
    }
}

// 启用扫描线
void TechPushButton::enableScanLine(bool enable)
{
    if (m_scanLineEnabled != enable) {
        m_scanLineEnabled = enable;
        update(); // 状态改变，立即重绘
    }
}

// 启用数据流
void TechPushButton::enableDataFlow(bool enable)
{
    m_dataFlowEnabled = enable;
    // if (enable) {
    //     m_dataFlowTimer->start(30);
    // } else {
    //     m_dataFlowTimer->stop();
    // }
    update();
}

// 启用3D效果
void TechPushButton::enable3DEffect(bool enable)
{
    m_3dEffectEnabled = enable;
    update();
}

// 设置圆角半径
void TechPushButton::setCornerRadius(int radius)
{
    if (m_cornerRadius != radius && radius >= 0) {
        m_cornerRadius = radius;
        m_cacheValid = false;
        update();
    }
}

// 设置图标大小
void TechPushButton::setTechIconSize(const QSize &size)
{
    if (m_iconSize != size) {
        m_iconSize = size;
        update();
    }
}

// 设置图标
void TechPushButton::setTechIcon(const QIcon &icon)
{
    m_techIcon = icon;
    update();
}

// 设置文字发光
void TechPushButton::setTextGlow(bool enable)
{
    if (m_textGlowEnabled != enable) {
        m_textGlowEnabled = enable;
        update();
    }
}

// 设置文字颜色
void TechPushButton::setTextColor(const QColor &color)
{
    if (m_textColor != color) {
        m_textColor = color;
        update();
    }
}

// 触发点击动画
void TechPushButton::triggerClickEffect()
{
    if (!m_clickAnimationEnabled) return;

    // 短暂的高亮效果
    QPropertyAnimation *clickAnim = new QPropertyAnimation(this, "glowOpacity");
    clickAnim->setDuration(200);
    clickAnim->setStartValue(m_glowOpacity);
    clickAnim->setEndValue(1.0);
    clickAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // 缩放效果
    QPropertyAnimation *scaleAnim = new QPropertyAnimation(this, "geometry");
    scaleAnim->setDuration(100);
    scaleAnim->setEasingCurve(QEasingCurve::OutBack);

    QRect startRect = geometry();
    QRect endRect = startRect.adjusted(-2, -2, 2, 2);

    scaleAnim->setStartValue(startRect);
    scaleAnim->setEndValue(endRect);
    scaleAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // 连接信号，恢复原始大小
    connect(scaleAnim, &QPropertyAnimation::finished, this, [this, startRect]() {
        QPropertyAnimation *recoverAnim = new QPropertyAnimation(this, "geometry");
        recoverAnim->setDuration(100);
        recoverAnim->setStartValue(geometry());
        recoverAnim->setEndValue(startRect);
        recoverAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// 开始脉冲
void TechPushButton::startPulse()
{
    if (m_pulseAnimation && m_pulseAnimation->state() != QAbstractAnimation::Running) {
        m_pulseAnimation->start();
    }
}

// 停止脉冲
void TechPushButton::stopPulse()
{
    if (m_pulseAnimation) {
        m_pulseAnimation->stop();
        m_pulseScale = 1.0;
        update();
    }
}

// 属性设置器
void TechPushButton::setGlowOpacity(qreal opacity)
{
    if (qFuzzyCompare(m_glowOpacity, opacity)) return;

    m_glowOpacity = qBound(0.0, opacity, 1.0);

    // 更新阴影效果
    if (m_shadowEffect) {
        QColor shadowColor = m_glowColor;
        shadowColor.setAlphaF(m_glowOpacity);
        m_shadowEffect->setColor(shadowColor);
        m_shadowEffect->setBlurRadius(m_glowOpacity < 0.05 ? 0.0 : 12.0 + m_glowOpacity * 8.0);
    }

    update();
}

void TechPushButton::setPulseScale(qreal scale)
{
    if (qFuzzyCompare(m_pulseScale, scale)) return;
    m_pulseScale = scale;
    update();
}

void TechPushButton::setScanPosition(qreal position)
{
    if (qFuzzyCompare(m_scanPosition, position)) return;
    m_scanPosition = position;
    update();
}

// 事件处理
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
void TechPushButton::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
#else
void TechPushButton::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
#endif



    if (!isEnabled()) return;

    m_state = StateHovered;

    if (m_hoverAnimationEnabled) {
        // 启动悬停动画
        m_glowAnimation->stop();
        m_glowAnimation->setStartValue(m_glowOpacity);
        m_glowAnimation->setEndValue(0.7);
        m_glowAnimation->start();
    }

    updateHoverEffect();
}

void TechPushButton::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);

    if (!isEnabled()) return;

    m_state = StateNormal;

    if (m_hoverAnimationEnabled) {
        // 恢复原始状态
        m_glowAnimation->stop();
        m_glowAnimation->setStartValue(m_glowOpacity);
        m_glowAnimation->setEndValue(0.0);
        m_glowAnimation->start();
    }

    updateNormalEffect();
}

void TechPushButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_state = StatePressed;

        if (m_clickAnimationEnabled) {
            // 点击效果
            setGlowOpacity(1.0);
        }

        updatePressedEffect();
    }

    QPushButton::mousePressEvent(event);
}

void TechPushButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_state = StateHovered;
        applySelectionVisual(isCheckable() && isChecked());
        updateHoverEffect();
    }

    QPushButton::mouseReleaseEvent(event);
}

void TechPushButton::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    m_cacheValid = false;
    QPushButton::resizeEvent(event);
}

// 更新效果
void TechPushButton::updateHoverEffect()
{
    m_cacheValid = false;
    update();
}

void TechPushButton::updatePressedEffect()
{
    m_cacheValid = false;
    update();
}

void TechPushButton::updateNormalEffect()
{
    m_cacheValid = false;
    update();
}

// 主绘制函数
void TechPushButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    QRect rect = this->rect();

    // 应用脉冲缩放
    if (m_pulseEffectEnabled && m_pulseScale != 1.0) {
        qreal scale = m_pulseScale;
        QRectF scaledRect = rect;
        scaledRect.setWidth(scaledRect.width() * scale);
        scaledRect.setHeight(scaledRect.height() * scale);
        scaledRect.moveCenter(rect.center());

        rect = scaledRect.toRect();
    }

    // 根据样式绘制按钮
    switch (m_style) {
    case StyleDefault:
        drawBaseButton(painter, rect);
        break;
    case StyleHolographic:
        drawHolographicButton(painter, rect);
        break;
    case StyleEnergy:
        drawEnergyButton(painter, rect);
        break;
    case StyleCircuit:
        drawCircuitButton(painter, rect);
        break;
    case StyleCyber:
        drawCyberButton(painter, rect);
        break;
    }

    // 绘制图标和文字
    drawIcon(painter, rect);
    drawText(painter, rect);

    // 绘制附加效果
    if (m_scanLineEnabled) {
        drawScanLine(painter, rect);
    }

    if (m_dataFlowEnabled) {
        drawDataFlow(painter, rect);
    }
}

// 绘制基础按钮
void TechPushButton::drawBaseButton(QPainter &painter, const QRect &rect)
{
    // 创建路径
    QPainterPath path;
    path.addRoundedRect(rect, m_cornerRadius, m_cornerRadius);

    // 绘制背景渐变
    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());

    // 根据状态调整颜色
    QColor baseColor = m_primaryColor;
    QColor lightColor = m_primaryColor.lighter(120);
    QColor darkColor = m_primaryColor.darker(120);

    switch (m_state) {
    case StateNormal:
        gradient.setColorAt(0, baseColor);
        gradient.setColorAt(1, darkColor);
        break;
    case StateHovered:
        gradient.setColorAt(0, lightColor);
        gradient.setColorAt(1, baseColor);
        break;
    case StatePressed:
        gradient.setColorAt(0, darkColor);
        gradient.setColorAt(1, baseColor.darker(150));
        break;
    case StateDisabled:
        gradient.setColorAt(0, baseColor.darker(200));
        gradient.setColorAt(1, darkColor.darker(200));
        break;
    }

    painter.fillPath(path, QBrush(gradient));

    // 绘制边框
    if (m_borderWidth > 0) {
        QPen pen(m_secondaryColor, m_borderWidth);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    // 绘制3D效果
    if (m_3dEffectEnabled) {
        // 上边框高光
        QLinearGradient topGradient(rect.topLeft(), rect.topRight());
        topGradient.setColorAt(0, QColor(255, 255, 255, 80));
        topGradient.setColorAt(1, QColor(255, 255, 255, 20));

        QPen topPen(QBrush(topGradient), 1);
        painter.setPen(topPen);
        painter.drawLine(rect.topLeft() + QPoint(m_cornerRadius/2, 1),
                         rect.topRight() + QPoint(-m_cornerRadius/2, 1));

        // 下边框阴影
        QLinearGradient bottomGradient(rect.bottomLeft(), rect.bottomRight());
        bottomGradient.setColorAt(0, QColor(0, 0, 0, 50));
        bottomGradient.setColorAt(1, QColor(0, 0, 0, 20));

        QPen bottomPen(QBrush(bottomGradient), 1);
        painter.setPen(bottomPen);
        painter.drawLine(rect.bottomLeft() + QPoint(m_cornerRadius/2, -1),
                         rect.bottomRight() + QPoint(-m_cornerRadius/2, -1));
    }
}

// 绘制全息按钮
void TechPushButton::drawHolographicButton(QPainter &painter, const QRect &rect)
{
    // 半透明背景
    QColor bgColor = m_primaryColor;
    bgColor.setAlpha(100);

    QPainterPath path;
    path.addRoundedRect(rect, m_cornerRadius, m_cornerRadius);

    // 多层半透明效果
    for (int i = 0; i < 3; i++) {
        int offset = i * 2;
        qreal alpha = 0.3 - i * 0.1;

        QPainterPath layerPath;
        layerPath.addRoundedRect(rect.adjusted(offset, offset, -offset, -offset),
                                 m_cornerRadius, m_cornerRadius);

        QColor layerColor = m_primaryColor;
        layerColor.setAlphaF(alpha);

        painter.fillPath(layerPath, layerColor);
    }

    // 边框
    QPen borderPen(m_glowColor, m_borderWidth);
    borderPen.setStyle(Qt::DotLine);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // 内部发光
    QRadialGradient innerGlow(rect.center(), qMax(rect.width(), rect.height()) / 2);
    innerGlow.setColorAt(0, QColor(255, 255, 255, 50));
    innerGlow.setColorAt(1, QColor(255, 255, 255, 0));

    painter.setBrush(QBrush(innerGlow));
    painter.setPen(Qt::NoPen);
    painter.drawPath(path);
}

// 绘制能量按钮
void TechPushButton::drawEnergyButton(QPainter &painter, const QRect &rect)
{
    // 能量核心效果
    QRadialGradient coreGrad(rect.center(), qMin(rect.width(), rect.height()) / 2);
    coreGrad.setColorAt(0, QColor(255, 255, 255, 200));
    coreGrad.setColorAt(0.5, m_primaryColor);
    coreGrad.setColorAt(1, m_primaryColor.darker(150));

    QPainterPath path;
    path.addRoundedRect(rect, m_cornerRadius, m_cornerRadius);

    painter.fillPath(path, QBrush(coreGrad));

    // 能量光束边框
    QPen energyPen(m_secondaryColor, m_borderWidth);
    energyPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(energyPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // 能量流动效果
    if (m_scanLineEnabled) { // 直接检查扫描线开关
        QLinearGradient flowGrad(rect.left(), rect.top() + rect.height() * m_scanPosition,
                                 rect.right(), rect.top() + rect.height() * m_scanPosition);
        flowGrad.setColorAt(0, QColor(255, 255, 255, 0));
        flowGrad.setColorAt(0.5, QColor(255, 255, 255, 150));
        flowGrad.setColorAt(1, QColor(255, 255, 255, 0));

        painter.setBrush(QBrush(flowGrad));
        painter.setPen(Qt::NoPen);
        painter.drawRect(rect);
    }
}

// 绘制电路板按钮
void TechPushButton::drawCircuitButton(QPainter &painter, const QRect &rect)
{
    // 深色背景
    painter.fillRect(rect, QColor(30, 30, 40));

    // 绘制电路走线
    drawCircuitLines(painter, rect);

    // 绘制IC芯片
    int chipSize = 20;
    QRect chipRect(rect.center().x() - chipSize/2, rect.center().y() - chipSize/2, chipSize, chipSize);
    painter.setBrush(QColor(52, 73, 94));
    painter.setPen(QPen(QColor(189, 195, 199), 1));
    painter.drawRect(chipRect);

    // 芯片引脚
    for (int i = 0; i < 4; i++) {
        int x = chipRect.left() + i * (chipRect.width() / 3);
        painter.drawLine(x, chipRect.top() - 2, x, chipRect.top());
        painter.drawLine(x, chipRect.bottom() + 2, x, chipRect.bottom());
    }

    // 绿色LED指示灯
    QRadialGradient ledGrad(chipRect.center(), 5);
    ledGrad.setColorAt(0, QColor(46, 204, 113, 200));
    ledGrad.setColorAt(1, QColor(46, 204, 113, 0));

    painter.setBrush(QBrush(ledGrad));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(chipRect.center(), 5, 5);

    // 边框
    QPen borderPen(m_primaryColor, m_borderWidth);
    borderPen.setStyle(Qt::DashLine);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, m_cornerRadius, m_cornerRadius);
}

// 绘制赛博按钮
void TechPushButton::drawCyberButton(QPainter &painter, const QRect &rect)
{
    // 网格背景
    drawCyberGrid(painter, rect);

    // 渐变填充
    QConicalGradient conicalGrad(rect.center(), 0);
    conicalGrad.setColorAt(0, m_primaryColor);
    conicalGrad.setColorAt(0.5, m_secondaryColor);
    conicalGrad.setColorAt(1, m_primaryColor);

    QPainterPath path;
    path.addRoundedRect(rect, m_cornerRadius, m_cornerRadius);

    painter.fillPath(path, QBrush(conicalGrad));

    // 霓虹边框
    QPen neonPen(m_glowColor, m_borderWidth * 2);
    neonPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(neonPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // 内层边框
    QPen innerPen(QColor(255, 255, 255, 100), 1);
    painter.setPen(innerPen);
    QPainterPath innerPath;
    innerPath.addRoundedRect(rect.adjusted(2, 2, -2, -2), m_cornerRadius, m_cornerRadius);
    painter.drawPath(innerPath);

    // 扫描线效果
    if (m_scanPosition > 0) {
        int scanY = rect.top() + rect.height() * m_scanPosition;
        QLinearGradient scanGrad(rect.left(), scanY, rect.right(), scanY);
        scanGrad.setColorAt(0, QColor(255, 255, 255, 0));
        scanGrad.setColorAt(0.5, QColor(255, 255, 255, 100));
        scanGrad.setColorAt(1, QColor(255, 255, 255, 0));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(scanGrad));
        painter.drawRect(rect.left(), scanY - 2, rect.width(), 4);
    }
}

// 绘制图标
void TechPushButton::drawIcon(QPainter &painter, const QRect &rect)
{
    if (m_techIcon.isNull() && icon().isNull()) return;

    QIcon iconToDraw = m_techIcon.isNull() ? icon() : m_techIcon;
    QSize iconSize = m_iconSize.isValid() ? m_iconSize : QSize(24, 24);

    // 计算图标位置
    int iconX = rect.left() + 10;
    int iconY = rect.center().y() - iconSize.height() / 2;

    // 绘制图标
    QPixmap pixmap = iconToDraw.pixmap(iconSize);
    painter.drawPixmap(iconX, iconY, pixmap);
}

// 绘制文字
void TechPushButton::drawText(QPainter &painter, const QRect &rect)
{
    if (text().isEmpty()) return;

    // 第一步：获取当前字体，并输出调试信息（用于定位问题）
    QFont font = this->font();
    // 第二步：安全地设置字体大小
    int targetPointSize = -1;

    if (font.pointSize() > 0) {
        // 情况A：点大小有效，则在原基础上+1
        targetPointSize = font.pointSize() + 1;
    } else if (font.pixelSize() > 0) {
        // 情况B：像素大小有效，则转换为近似的点大小并+1
        // 避免使用 qMax，改用显式判断
        int pixelBasedSize = static_cast<int>(font.pixelSize() / 1.33);
        if (pixelBasedSize < 9) {
            pixelBasedSize = 9; // 设置最小值
        }
        targetPointSize = pixelBasedSize + 1;
    } else {
        // 情况C：两者都无效，使用一个明确且安全的默认值
        targetPointSize = 10;
    }

    // 应用计算出的安全大小
    font.setPointSize(targetPointSize);
    font.setBold(true);
    painter.setFont(font);

    // 第三步：原有的文字位置计算、颜色和发光效果等代码保持不变...
    QRect textRect = rect;
    if (!m_techIcon.isNull() || !icon().isNull()) {
        textRect.adjust(m_iconSize.width() + 20, 0, 0, 0);
    }

    QColor textColor = m_textColor;
    if (!isEnabled()) {
        textColor = textColor.darker(150);
    }

    // ... 你原有的文字发光和绘制逻辑（使用QPainterPath等）继续放在这里...
    // 注意：如果发光效果里也创建了QFont，也需要进行同样的安全检查。

    // 第四步：绘制文字
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignCenter, text());
}

// 绘制扫描线
void TechPushButton::drawScanLine(QPainter &painter, const QRect &rect)
{
    int scanY = rect.top() + rect.height() * m_scanPosition;

    QLinearGradient scanGrad(rect.left(), scanY, rect.right(), scanY);
    scanGrad.setColorAt(0, QColor(255, 255, 255, 0));
    scanGrad.setColorAt(0.5, QColor(255, 255, 255, 80));
    scanGrad.setColorAt(1, QColor(255, 255, 255, 0));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(scanGrad));
    painter.drawRect(rect.left(), scanY - 1, rect.width(), 2);
}

// 绘制数据流
void TechPushButton::drawDataFlow(QPainter &painter, const QRect &rect)
{
    // 绘制数据流点
    int pointCount = 8;
    qreal spacing = rect.width() / (pointCount + 1);

    for (int i = 0; i < pointCount; i++) {
        qreal x = rect.left() + spacing * (i + 1);
        qreal t = (m_dataFlowProgress + i * 0.1);
        while (t > 1.0) t -= 1.0;

        qreal y = rect.top() + rect.height() * t;

        // 点的颜色根据位置变化
        QColor pointColor = m_primaryColor;
        pointColor.setAlphaF(0.5 + 0.5 * qSin(t * M_PI));

        painter.setBrush(pointColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, y), 2, 2);
    }
}

// 绘制电路走线
void TechPushButton::drawCircuitLines(QPainter &painter, const QRect &rect)
{
    QPen tracePen(m_primaryColor, 1);
    tracePen.setStyle(Qt::DotLine);
    painter.setPen(tracePen);

    // 水平走线
    for (int y = rect.top() + 10; y < rect.bottom(); y += 15) {
        painter.drawLine(rect.left() + 5, y, rect.right() - 5, y);
    }

    // 垂直走线
    for (int x = rect.left() + 10; x < rect.right(); x += 15) {
        painter.drawLine(x, rect.top() + 5, x, rect.bottom() - 5);
    }

    // 焊点
    painter.setBrush(m_secondaryColor);
    painter.setPen(Qt::NoPen);

    for (int y = rect.top() + 10; y < rect.bottom(); y += 15) {
        for (int x = rect.left() + 10; x < rect.right(); x += 15) {
            if (m_random->bounded(100) > 70) {
                painter.drawEllipse(QPointF(x, y), 1.5, 1.5);
            }
        }
    }
}

// 绘制赛博网格
void TechPushButton::drawCyberGrid(QPainter &painter, const QRect &rect)
{
    // 绘制网格线
    QPen gridPen(QColor(255, 255, 255, 30), 0.5);
    painter.setPen(gridPen);

    // 水平线
    for (int y = rect.top(); y < rect.bottom(); y += 10) {
        painter.drawLine(rect.left(), y, rect.right(), y);
    }

    // 垂直线
    for (int x = rect.left(); x < rect.right(); x += 10) {
        painter.drawLine(x, rect.top(), x, rect.bottom());
    }

    // 对角线
    QPen diagPen(QColor(255, 255, 255, 20), 0.5);
    painter.setPen(diagPen);
    painter.drawLine(rect.topLeft(), rect.bottomRight());
    painter.drawLine(rect.topRight(), rect.bottomLeft());
}
