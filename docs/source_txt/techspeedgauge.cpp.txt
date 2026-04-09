#include "techspeedgauge.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QPropertyAnimation>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QtMath>

TechSpeedGauge::TechSpeedGauge(QWidget *parent)
    : QWidget{parent},
    m_style(StyleCyberFuturistic),
    m_minValue(0.0),
    m_maxValue(100.0),
    m_currentValue(0.0),
    m_targetValue(0.0),
    m_unit("km/h"),
    m_title("当前速度"),
    m_precision(1),
    m_primaryColor(QColor(0, 200, 255)),      // 科技蓝
    m_secondaryColor(QColor(138, 43, 226)),   // 蓝紫色
    m_glowColor(QColor(0, 255, 255, 100)),
    m_glowEnabled(true),
    m_scanLineEnabled(false),  // 关键优化：默认禁用扫描线（减少每秒15次的全屏重绘）
    m_pulseEffectEnabled(false),
    m_scanAngle(0.0),
    m_pulseIntensity(0.0),
    // m_animationTimer(nullptr),
    m_valueAnimation(nullptr),
    m_cacheValid(false)
{
    // 设置默认大小
    // setMinimumSize(200, 200);

    // 创建动画定时器
    // m_animationTimer = new QTimer(this);
    // connect(m_animationTimer, &QTimer::timeout, this, &TechSpeedGauge::updateAnimation);
    // m_animationTimer->start(66); // 约33帧/秒
    // 改为注册到全局管理器
    AnimationManager::instance()->registerWidget(this);

    // 创建值动画
    m_valueAnimation = new QPropertyAnimation(this, "currentValue");
    m_valueAnimation->setDuration(800); // 800毫秒动画
    m_valueAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_valueAnimation, &QPropertyAnimation::finished,
            this, &TechSpeedGauge::valueAnimationFinished);

    // 初始值
    setValue(0.0);
}
// TechSpeedGauge::~TechSpeedGauge()
// {
//     // 从动画管理器注销
//     AnimationManager::instance()->unregisterWidget(this);

//     // 其他清理...
// }
void TechSpeedGauge::setRange(qreal min, qreal max)
{
    if (min < max) {
        m_minValue = min;
        m_maxValue = max;
        m_cacheValid = false;
        update();
    }
}

void TechSpeedGauge::setValue(qreal value)
{
    // 限制值在范围内
    value = qBound(m_minValue, value, m_maxValue);

    if (!qFuzzyCompare(m_currentValue, value)) {
        m_currentValue = value;
        m_cacheValid = false;
        emit valueChanged(value);
        update();
    }
}

void TechSpeedGauge::setValueSmooth(qreal value)
{
    // 限制值在范围内
    value = qBound(m_minValue, value, m_maxValue);
    m_targetValue = value;

    // 停止当前动画
    if (m_valueAnimation->state() == QAbstractAnimation::Running) {
        m_valueAnimation->stop();
    }

    // 开始新动画
    m_valueAnimation->setStartValue(m_currentValue);
    m_valueAnimation->setEndValue(value);
    m_valueAnimation->start();
}

void TechSpeedGauge::setCurrentValue(qreal value)
{
    // 这是用于属性动画的setter
    setValue(value);
}

void TechSpeedGauge::setGaugeStyle(GaugeStyle style)
{
    if (m_style != style) {
        m_style = style;

        // 根据样式设置默认颜色
        switch (style) {
        case StyleCyberFuturistic:
            m_primaryColor = QColor(0, 200, 255);   // 科技蓝
            m_secondaryColor = QColor(138, 43, 226); // 蓝紫
            m_glowColor = QColor(0, 255, 255, 100);
            break;
        case StyleEnergyArc:
            m_primaryColor = QColor(255, 100, 0);   // 橙红
            m_secondaryColor = QColor(255, 220, 0); // 金黄
            m_glowColor = QColor(255, 200, 0, 150);
            break;
        case StyleHolographic:
            m_primaryColor = QColor(0, 200, 255, 150); // 半透明青
            m_secondaryColor = QColor(255, 0, 255, 150); // 半透明品红
            m_glowColor = QColor(255, 255, 255, 100);
            break;
        case StyleClassicDial:
            m_primaryColor = QColor(46, 204, 113);   // 电路绿
            m_secondaryColor = QColor(52, 152, 219); // 科技蓝
            m_glowColor = QColor(0, 255, 127, 100);
            break;
        }

        m_cacheValid = false;
        update();
    }
}

void TechSpeedGauge::setPrimaryColor(const QColor &color)
{
    if (m_primaryColor != color) {
        m_primaryColor = color;
        m_cacheValid = false;
        update();
    }
}

void TechSpeedGauge::setSecondaryColor(const QColor &color)
{
    if (m_secondaryColor != color) {
        m_secondaryColor = color;
        m_cacheValid = false;
        update();
    }
}

void TechSpeedGauge::setUnit(const QString &unit)
{
    if (m_unit != unit) {
        m_unit = unit;
        update();
    }
}

void TechSpeedGauge::enableGlowEffect(bool enable)
{
    if (m_glowEnabled != enable) {
        m_glowEnabled = enable;
        m_cacheValid = false;
        update();
    }
}

void TechSpeedGauge::enableScanLine(bool enable)
{
    if (m_scanLineEnabled != enable) {
        m_scanLineEnabled = enable;
        update();
    }
}

void TechSpeedGauge::enablePulseEffect(bool enable)
{
    if (m_pulseEffectEnabled != enable) {
        m_pulseEffectEnabled = enable;
        update();
    }
}

void TechSpeedGauge::setPrecision(int precision)
{
    if (m_precision != precision && precision >= 0) {
        m_precision = precision;
        update();
    }
}

void TechSpeedGauge::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        update();
    }
}

void TechSpeedGauge::setScanAngle(qreal angle)
{
    // 规范化角度到0-360度
    while (angle < 0.0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;

    if (!qFuzzyCompare(m_scanAngle, angle)) {
        m_scanAngle = angle;
        update();
    }
}

void TechSpeedGauge::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    // 计算中心点和半径
    m_gaugeRect = rect().adjusted(10, 10, -10, -10);
    m_center = m_gaugeRect.center();
    m_radius = qMin(m_gaugeRect.width(), m_gaugeRect.height()) / 2.0;

    m_cacheValid = false;
}

void TechSpeedGauge::setObstacleStatus(bool front, bool back, bool left, bool right)
{
    if (m_obstacleFront != front || m_obstacleBack != back ||
        m_obstacleLeft != left || m_obstacleRight != right) {
        m_obstacleFront = front;
        m_obstacleBack = back;
        m_obstacleLeft = left;
        m_obstacleRight = right;
        update(); // 触发重绘
    }
}

void TechSpeedGauge::drawObstacleIndicators(QPainter &painter)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    // 使用环形扇区，避免 drawPie 产生“从圆心到边缘”的径向线
    qreal outerRadius = m_radius * 1.05;
    qreal innerRadius = m_radius * 0.95;
    QRectF outerRect(-outerRadius, -outerRadius, outerRadius * 2, outerRadius * 2);
    QRectF innerRect(-innerRadius, -innerRadius, innerRadius * 2, innerRadius * 2);

    auto drawSector = [&](bool active, qreal startAngle, qreal spanAngle) {
        if (!active) return;

        // 设置红色半透明渐变
        QRadialGradient gradient(0, 0, outerRadius);
        gradient.setColorAt(0.8, QColor(255, 0, 0, 0));      // 内部透明
        gradient.setColorAt(0.95, QColor(255, 0, 0, 150));   // 边缘处最亮
        gradient.setColorAt(1.0, QColor(255, 0, 0, 200));    // 最外圈稍微深一点

        QPainterPath path;
        path.arcMoveTo(outerRect, startAngle);
        path.arcTo(outerRect, startAngle, spanAngle);
        path.arcTo(innerRect, startAngle + spanAngle, -spanAngle);
        path.closeSubpath();

        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawPath(path);

        // 外弧发光边框
        QPainterPath outerArcPath;
        outerArcPath.arcMoveTo(outerRect, startAngle);
        outerArcPath.arcTo(outerRect, startAngle, spanAngle);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 50, 50, 200), 2));
        painter.drawPath(outerArcPath);
    };

    painter.translate(m_center);

    // 1. 前方 (Top/90度附近，Qt中是90度左右)
    drawSector(m_obstacleFront, 45, 90);
    // 2. 后方 (Bottom/270度附近)
    drawSector(m_obstacleBack, 225, 90);
    // 3. 左侧 (Left/180度附近)
    drawSector(m_obstacleLeft, 135, 90);
    // 4. 右侧 (Right/0度附近)
    drawSector(m_obstacleRight, 315, 90);

    painter.restore();
}

void TechSpeedGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                           QPainter::SmoothPixmapTransform);

    // 绘制背景
    drawBackground(painter);

    // 绘制障碍物指示器 (在背景和数值之间)
    drawObstacleIndicators(painter);

    // 绘制数值弧
    drawValueArc(painter);

    // 注释掉指针绘制
    // drawNeedle(painter);

    // 绘制中央数值显示
    drawValueDisplay(painter);

    // 绘制标题
    drawTitle(painter);

    // 绘制前景特效
    if (m_scanLineEnabled || m_pulseEffectEnabled) {
        drawForegroundEffects(painter);
    }
}


void TechSpeedGauge::updateAnimation()
{
    // m_animationTimer = new QTimer(this);
    // connect(m_animationTimer, &QTimer::timeout, this, &TechSpeedGauge::updateAnimation);
    // 更新扫描线角度
    if (m_scanLineEnabled) {
        m_scanAngle += 2.0; // 每次增加2度
        if (m_scanAngle >= 360.0) {
            m_scanAngle -= 360.0;
        }
    }

    // 更新脉冲强度
    if (m_pulseEffectEnabled) {
        m_pulseIntensity = 0.5 + 0.5 * qSin(QDateTime::currentMSecsSinceEpoch() / 500.0);
    }

    // 触发重绘
    if (m_scanLineEnabled || m_pulseEffectEnabled) {
        update();
    }
}

// 绘制背景
void TechSpeedGauge::drawBackground(QPainter &painter)
{
    QPainterPath backgroundPath;
    backgroundPath.addEllipse(m_center, m_radius, m_radius);

    // 根据样式绘制不同背景
    switch (m_style) {
    case StyleCyberFuturistic:
    {
        // 赛博风格：深色渐变背景
        QRadialGradient gradient(m_center, m_radius);
        gradient.setColorAt(0, QColor(20, 25, 45, 200));
        gradient.setColorAt(0.7, QColor(10, 15, 30, 180));
        gradient.setColorAt(1, QColor(5, 10, 20, 150));

        painter.fillPath(backgroundPath, QBrush(gradient));

        // 绘制网格线
        painter.save();
        painter.setPen(QPen(QColor(50, 60, 80, 80), 1));
        for (int i = 0; i < 36; i++) {
            qreal angle = i * 10.0;
            QPointF p1 = pointOnCircle(m_radius * 0.9, angle);
            QPointF p2 = pointOnCircle(m_radius * 0.7, angle);
            painter.drawLine(m_center + p1, m_center + p2);
        }
        painter.restore();
        break;
    }

    case StyleEnergyArc:
    {
        // 能量风格：深色到亮色的径向渐变
        QRadialGradient gradient(m_center, m_radius);
        gradient.setColorAt(0, QColor(30, 15, 10, 220));
        gradient.setColorAt(0.5, QColor(20, 10, 5, 180));
        gradient.setColorAt(1, QColor(10, 5, 0, 150));

        painter.fillPath(backgroundPath, QBrush(gradient));
        break;
    }

    case StyleHolographic:
    {
        // 全息风格：多层半透明圆环
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < 4; i++) {
            qreal r = m_radius - i * 5;
            QColor color = m_primaryColor;
            color.setAlpha(30 - i * 5);

            painter.setPen(QPen(color, 1.5, Qt::DashLine));
            painter.drawEllipse(m_center, r, r);
        }
        break;
    }

    case StyleClassicDial:
    {
        // 经典风格：光滑渐变
        QRadialGradient gradient(m_center, m_radius);
        gradient.setColorAt(0, QColor(40, 40, 50));
        gradient.setColorAt(1, QColor(20, 20, 30));

        painter.fillPath(backgroundPath, QBrush(gradient));

        // 添加金属边框
        QLinearGradient borderGrad(m_center - QPointF(m_radius, 0),
                                   m_center + QPointF(m_radius, 0));
        borderGrad.setColorAt(0, QColor(100, 100, 120));
        borderGrad.setColorAt(0.5, QColor(180, 180, 200));
        borderGrad.setColorAt(1, QColor(100, 100, 120));

        painter.setPen(QPen(QBrush(borderGrad), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(m_center, m_radius, m_radius);
        break;
    }
    }
}

// 绘制刻度和数字
// void TechSpeedGauge::drawTicksAndNumbers(QPainter &painter)
// {
//     painter.save();

//     // 设置刻度颜色
//     QColor tickColor = (m_style == StyleHolographic) ?
//                            QColor(255, 255, 255, 150) : QColor(200, 220, 255, 180);

//     painter.setPen(QPen(tickColor, 1.5));

//     // 绘制主要刻度（每10单位）
//     int majorTicks = 10;
//     qreal tickAngleRange = 240.0; // 仪表盘角度范围（从-120度到120度）
//     qreal startAngle = 150.0;     // 起始角度（从左侧开始）

//     for (int i = 0; i <= majorTicks; i++) {
//         qreal value = m_minValue + (m_maxValue - m_minValue) * i / majorTicks;
//         qreal angle = startAngle - (tickAngleRange * i / majorTicks);
//         qreal rad = qDegreesToRadians(angle);

//         // 计算刻度点
//         QPointF innerPoint = pointOnCircle(m_radius * 0.85, rad);
//         QPointF outerPoint = pointOnCircle(m_radius * 0.75, rad);

//         painter.drawLine(m_center + innerPoint, m_center + outerPoint);

//         // 绘制刻度值
//         if (i % 2 == 0) { // 每2个刻度显示一个数字
//             QPointF textPoint = pointOnCircle(m_radius * 0.65, rad);
//             QFont font = painter.font();
//             font.setPointSize(9);
//             font.setBold(true);
//             painter.setFont(font);

//             painter.save();
//             painter.translate(m_center + textPoint);
//             // painter.rotate(-angle + 90); // 使文字朝向圆心
//             painter.drawText(QRect(-30, -15, 60, 30), Qt::AlignCenter,
//                              QString::number(value, 'f', 0));
//             painter.restore();
//         }
//     }

//     // 绘制次要刻度
//     int minorTicksPerMajor = 5;
//     painter.setPen(QPen(tickColor.lighter(120), 1));

//     for (int i = 0; i < majorTicks * minorTicksPerMajor; i++) {
//         qreal angle = startAngle - (tickAngleRange * i / (majorTicks * minorTicksPerMajor));
//         qreal rad = qDegreesToRadians(angle);

//         QPointF innerPoint = pointOnCircle(m_radius * 0.82, rad);
//         QPointF outerPoint = pointOnCircle(m_radius * 0.78, rad);

//         painter.drawLine(m_center + innerPoint, m_center + outerPoint);
//     }

//     painter.restore();
// }

// 绘制数值弧
void TechSpeedGauge::drawValueArc(QPainter &painter)
{
    painter.save();

    // 计算当前值对应的角度
    qreal currentAngle = angleFromValue(m_currentValue);
    qreal startAngle = 150.0; // 起始角度
    qreal spanAngle = startAngle - currentAngle;

    // 创建弧形路径
    QPainterPath arcPath;
    arcPath.arcMoveTo(m_gaugeRect, startAngle);
    arcPath.arcTo(m_gaugeRect, startAngle, -spanAngle);

    // 根据样式设置弧线样式
    switch (m_style) {
    case StyleCyberFuturistic:
    {
        // 赛博风格：锥形渐变
        QConicalGradient gradient(m_center, -startAngle);
        gradient.setColorAt(0.0, m_primaryColor);
        gradient.setColorAt(0.5, m_secondaryColor);
        gradient.setColorAt(1.0, m_primaryColor);

        QPen arcPen(QBrush(gradient), 12);
        arcPen.setCapStyle(Qt::FlatCap);
        painter.setPen(arcPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(arcPath);
        break;
    }

    case StyleEnergyArc:
    {
        // 能量风格：实心弧带
        QPainterPath thickArc;
        thickArc.arcMoveTo(m_gaugeRect.adjusted(-8, -8, 8, 8), startAngle);
        thickArc.arcTo(m_gaugeRect.adjusted(-8, -8, 8, 8), startAngle, -spanAngle);

        QLinearGradient gradient(m_center - QPointF(m_radius, 0),
                                 m_center + QPointF(m_radius, 0));
        gradient.setColorAt(0.0, m_primaryColor);
        gradient.setColorAt(0.5, m_secondaryColor);
        gradient.setColorAt(1.0, m_primaryColor);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(gradient));
        painter.drawPath(thickArc);
        break;
    }

    case StyleHolographic:
    {
        // 全息风格：多层半透明弧线
        for (int i = 0; i < 3; i++) {
            QPen arcPen(m_primaryColor, 4 - i);
            arcPen.setStyle(Qt::DotLine);
            arcPen.setCapStyle(Qt::RoundCap);
            painter.setPen(arcPen);
            painter.drawPath(arcPath);
        }
        break;
    }

    case StyleClassicDial:
    {
        // 经典风格：光滑渐变弧线
        QPen arcPen(QBrush(m_primaryColor), 10);
        arcPen.setCapStyle(Qt::RoundCap);
        painter.setPen(arcPen);
        painter.drawPath(arcPath);
        break;
    }
    }

    // 添加发光效果
    if (m_glowEnabled) {
        painter.setPen(QPen(m_glowColor, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(arcPath);
    }

    painter.restore();
}

// 绘制指针
// void TechSpeedGauge::drawNeedle(QPainter &painter)
// {
//     painter.save();

//     // 计算指针角度
//     qreal needleAngle = angleFromValue(m_currentValue);
//     qreal rad = qDegreesToRadians(needleAngle);

//     // 指针形状
//     QPolygonF needle;
//     needle << QPointF(0, -m_radius * 0.85)    // 顶部
//            << QPointF(-8, 0)                  // 左侧底部
//            << QPointF(0, m_radius * 0.1)      // 中心点（稍长）
//            << QPointF(8, 0);                  // 右侧底部

//     // 绘制指针
//     painter.translate(m_center);
//     painter.rotate(needleAngle);

//     // 指针渐变
//     QLinearGradient needleGrad(QPointF(-10, 0), QPointF(10, 0));
//     needleGrad.setColorAt(0, m_secondaryColor);
//     needleGrad.setColorAt(0.5, Qt::white);
//     needleGrad.setColorAt(1, m_secondaryColor);

//     painter.setBrush(QBrush(needleGrad));
//     painter.setPen(QPen(Qt::black, 1));
//     painter.drawPolygon(needle);

//     // 绘制指针中心圆
//     QRadialGradient centerGrad(QPointF(0, 0), 10);
//     centerGrad.setColorAt(0, Qt::white);
//     centerGrad.setColorAt(1, m_primaryColor);

//     painter.setBrush(QBrush(centerGrad));
//     painter.setPen(QPen(Qt::black, 2));
//     painter.drawEllipse(QPointF(0, 0), 8, 8);

//     painter.restore();
// }

// 绘制中央数值显示
void TechSpeedGauge::drawValueDisplay(QPainter &painter)
{
    painter.save();

    // 显示背景圆（可以调整大小，让数值区域更大）
    qreal displayRadius = m_radius * 0.4;  // 从0.3改为0.4，增大显示区域
    QRadialGradient bgGrad(m_center, displayRadius);
    bgGrad.setColorAt(0, QColor(255, 255, 255, 50));
    bgGrad.setColorAt(1, QColor(100, 100, 120, 100));

    painter.setBrush(QBrush(bgGrad));
    painter.setPen(QPen(QColor(200, 220, 255, 150), 2));
    painter.drawEllipse(m_center, displayRadius, displayRadius);

    // 显示当前值（增大字体）
    QFont valueFont = painter.font();
    valueFont.setPointSize(24);  // 从12增大到24
    valueFont.setBold(true);
    valueFont.setFamily("Microsoft YaHei, Segoe UI");
    painter.setFont(valueFont);

    QString valueText = QString::number(m_currentValue, 'f', m_precision);
    QRectF valueRect(m_center.x() - displayRadius * 0.8,
                     m_center.y() - displayRadius * 0.8,  // 调整位置
                     displayRadius * 1.6,
                     displayRadius * 1.0);

    // 数值颜色渐变
    QLinearGradient textGrad(valueRect.topLeft(), valueRect.bottomLeft());
    textGrad.setColorAt(0, Qt::white);
    textGrad.setColorAt(1, m_primaryColor);

    painter.setPen(QPen(QBrush(textGrad), 2));  // 加粗线条
    painter.drawText(valueRect, Qt::AlignCenter, valueText);

    // 显示单位（调整位置，现在数值变大，单位需要下移）
    QFont unitFont = painter.font();
    unitFont.setPointSize(14);  // 稍微增大单位字体
    painter.setFont(unitFont);

    QRectF unitRect(m_center.x() - displayRadius * 0.8,
                    m_center.y() + displayRadius * 0.6,  // 下移单位显示
                    displayRadius * 1.6,
                    displayRadius * 0.5);

    painter.setPen(QPen(QColor(180, 200, 255), 2));  // 加粗单位线条
    painter.drawText(unitRect, Qt::AlignCenter, m_unit);

    painter.restore();
}

// 绘制标题
void TechSpeedGauge::drawTitle(QPainter &painter)
{
    if (m_title.isEmpty()) return;

    painter.save();

    QFont titleFont = painter.font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    painter.setFont(titleFont);

    QRectF titleRect(m_center.x() - m_radius * 0.8,
                     m_center.y() + m_radius * 0.6,
                     m_radius * 1.6,
                     30);

    // 标题颜色渐变
    QLinearGradient titleGrad(titleRect.topLeft(), titleRect.bottomLeft());
    titleGrad.setColorAt(0, m_primaryColor);
    titleGrad.setColorAt(1, m_secondaryColor);

    painter.setPen(QPen(QBrush(titleGrad), 2));
    painter.drawText(titleRect, Qt::AlignCenter, m_title);

    painter.restore();
}

// 绘制前景特效
void TechSpeedGauge::drawForegroundEffects(QPainter &painter)
{
    painter.save();

    // 扫描线效果
    if (m_scanLineEnabled) {
        qreal rad = qDegreesToRadians(m_scanAngle);
        QPointF scanEnd = pointOnCircle(m_radius * 0.9, rad);

        QLinearGradient scanGrad(m_center, m_center + scanEnd);
        scanGrad.setColorAt(0, QColor(255, 255, 255, 0));
        scanGrad.setColorAt(0.3, QColor(255, 255, 255, 100));
        scanGrad.setColorAt(0.7, QColor(255, 255, 255, 100));
        scanGrad.setColorAt(1, QColor(255, 255, 255, 0));

        painter.setPen(QPen(QBrush(scanGrad), 3));
        painter.drawLine(m_center, m_center + scanEnd);

        // 扫描线端点光晕
        QRadialGradient endGrad(m_center + scanEnd, 10);
        endGrad.setColorAt(0, QColor(255, 255, 255, 200));
        endGrad.setColorAt(1, QColor(255, 255, 255, 0));

        painter.setBrush(QBrush(endGrad));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(m_center + scanEnd, 8, 8);
    }

    // 脉冲效果
    if (m_pulseEffectEnabled && m_pulseIntensity > 0) {
        qreal pulseRadius = m_radius * (0.7 + m_pulseIntensity * 0.1);
        QColor pulseColor = m_primaryColor;
        pulseColor.setAlphaF(m_pulseIntensity * 0.3);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(pulseColor, 2));
        painter.drawEllipse(m_center, pulseRadius, pulseRadius);
    }

    painter.restore();
}

// 辅助函数：根据值计算角度
qreal TechSpeedGauge::angleFromValue(qreal value) const
{
    qreal normalized = (value - m_minValue) / (m_maxValue - m_minValue);
    return 150.0 - normalized * 240.0; // 从150度到-90度
}

// 辅助函数：计算圆上点的坐标
QPointF TechSpeedGauge::pointOnCircle(qreal radius, qreal angle) const
{
    qreal rad = qDegreesToRadians(angle);
    return QPointF(radius * qCos(rad), -radius * qSin(rad));
}

// 辅助函数：混合颜色
QColor TechSpeedGauge::blendColors(const QColor &c1, const QColor &c2, qreal factor) const
{
    factor = qBound(0.0, factor, 1.0);
    return QColor(
        c1.red() * (1 - factor) + c2.red() * factor,
        c1.green() * (1 - factor) + c2.green() * factor,
        c1.blue() * (1 - factor) + c2.blue() * factor,
        c1.alpha() * (1 - factor) + c2.alpha() * factor
        );
}
void TechSpeedGauge::setGlowColor(const QColor &color) {
    if (m_glowColor != color) {
        m_glowColor = color;
        update(); // 触发重绘，使新颜色生效
    }
}
//模拟数据
bool TechSpeedGauge::scanLineEnabled() const
{
    return m_scanLineEnabled;
}
