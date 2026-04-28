#include "techarcgauge.h"
#include <QPainterPath>
#include <QConicalGradient>
#include <QDebug>

TechArcGauge::TechArcGauge(QWidget *parent)
    : QWidget(parent)
    , m_value(0.0)
    , m_minimum(0.0)
    , m_maximum(100.0)
    , m_secondValue(0.0)
    , m_secondMaximum(100.0)
    , m_precision(0)
    , m_suffix("")
    , m_secondSuffix("")
    , m_labelText("Parameter")
    , m_modbusAddress(-1)
    , m_primaryColor(QColor(0, 200, 255))
    , m_glowColor(QColor(0, 255, 255, 120))
    , m_forceControlEnabled(false)
    , m_scanLinePhase(0)
    , m_repaintPending(false)
{
    m_originalPrimaryColor = m_primaryColor;
    m_originalGlowColor = m_glowColor;
    
    // 向动画管理器注册
    AnimationManager::instance()->registerWidget(this);
}

TechArcGauge::~TechArcGauge()
{
}

void TechArcGauge::setValue(double value)
{
    value = qBound(m_minimum, value, m_maximum);
    if (qAbs(m_value - value) > 0.0001) {
        m_value = value;
        
        emit valueChanged(m_value);
        requestRepaint();
    }
}

void TechArcGauge::setSecondValue(double value)
{
    value = qBound(0.0, value, m_secondMaximum);
    if (qAbs(m_secondValue - value) > 0.0001) {
        m_secondValue = value;
        emit secondValueChanged(m_secondValue);
        requestRepaint();
    }
}

void TechArcGauge::setSecondMaximum(double max)
{
    m_secondMaximum = qMax(0.1, max);
    requestRepaint();
}

void TechArcGauge::setMinimum(double min)
{
    m_minimum = min;
    setValue(m_value);
}

void TechArcGauge::setMaximum(double max)
{
    m_maximum = max;
    setValue(m_value);
}

void TechArcGauge::setRange(double min, double max)
{
    m_minimum = min;
    m_maximum = max;
    setValue(m_value);
}

void TechArcGauge::setLabelText(const QString &text)
{
    m_labelText = text;
    requestRepaint();
}

void TechArcGauge::setSuffix(const QString &suffix)
{
    m_suffix = suffix;
    requestRepaint();
}

void TechArcGauge::setSecondSuffix(const QString &suffix)
{
    m_secondSuffix = suffix;
    requestRepaint();
}

void TechArcGauge::setPrecision(int precision)
{
    m_precision = qMax(0, precision);
    requestRepaint();
}

void TechArcGauge::setForceControlMode(bool enabled)
{
    Q_UNUSED(enabled);
    m_forceControlEnabled = false;
    m_primaryColor = m_originalPrimaryColor;
    m_glowColor = m_originalGlowColor;
    requestRepaint();
}

void TechArcGauge::updateFromModbus(double value)
{
    setValue(value);
}

void TechArcGauge::updateAnimation()
{
    m_scanLinePhase += 2.0f;
    if (m_scanLinePhase > 360.0f) m_scanLinePhase -= 360.0f;
    requestRepaint();
}

void TechArcGauge::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int size = qMin(width(), height()) - 10;
    QRectF rect((width() - size) / 2, (height() - size) / 2, size, size);

    // 绘制背景半环 (180度, 底部开口)
    float startAngle = -225;
    float spanAngle = 270;
    
    QPen backPen;
    backPen.setColor(QColor(50, 50, 50, 100));
    backPen.setWidth(8);
    backPen.setCapStyle(Qt::RoundCap);
    painter.setPen(backPen);
    painter.drawArc(rect.adjusted(10, 10, -10, -10), startAngle * 16, spanAngle * 16);

    // 绘制当前值环
    float ratio = (m_value - m_minimum) / (m_maximum - m_minimum);
    float valueSpan = spanAngle * ratio;
    
    QPen valuePen;
    valuePen.setColor(m_primaryColor);
    valuePen.setWidth(10);
    valuePen.setCapStyle(Qt::RoundCap);
    
    // 发光效果
    painter.save();
    QPen glowPen = valuePen;
    glowPen.setColor(m_glowColor);
    glowPen.setWidth(15);
    painter.setPen(glowPen);
    painter.drawArc(rect.adjusted(10, 10, -10, -10), startAngle * 16, valueSpan * 16);
    painter.restore();

    painter.setPen(valuePen);
    painter.drawArc(rect.adjusted(10, 10, -10, -10), startAngle * 16, valueSpan * 16);

    // 绘制刻度点 (装饰用的点阵)
    painter.setPen(QPen(m_primaryColor.lighter(), 2));
    for (int i = 0; i <= 10; ++i) {
        float angle = startAngle + (spanAngle * i / 10.0);
        float rad = qDegreesToRadians(-angle);
        float r = size / 2.0 - 5;
        float centerX = width() / 2.0;
        float centerY = height() / 2.0;
        painter.drawPoint(centerX + r * cos(rad), centerY + r * sin(rad));
    }

    // 绘制扫描线特效 (一个小亮点在环上滑动)
    float scanAngle = startAngle + m_scanLinePhase * (spanAngle / 360.0);
    float scanRad = qDegreesToRadians(-scanAngle);
    float scanR = size / 2.0 - 10;
    painter.setBrush(m_primaryColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(width() / 2.0 + scanR * cos(scanRad), 
                                height() / 2.0 + scanR * sin(scanRad)), 3, 3);

    // --- 绘制第二数值（例如速度）的进度条（在内圈） ---
    if (m_secondMaximum > 0) {
        float innerRatio = qBound(0.0, m_secondValue / m_secondMaximum, 1.0);
        float secondSpan = spanAngle * innerRatio;
        
        QPen secondPen;
        secondPen.setColor(QColor(255, 165, 0, 180)); // 科技橙 (Orange)
        secondPen.setWidth(4);
        secondPen.setCapStyle(Qt::RoundCap);
        
        painter.setPen(secondPen);
        painter.drawArc(rect.adjusted(25, 25, -25, -25), startAngle * 16, secondSpan * 16);
    }

    // 绘制文字
    painter.setPen(Qt::white);
    QFont font = painter.font();
    
    // 当前主数值（长度/角度）
    font.setPixelSize(size / 6);
    font.setBold(true);
    painter.setFont(font);
    QString valueStr = QString::number(m_value, 'f', m_precision);
    painter.drawText(rect.adjusted(0, -size/15, 0, -size/15), Qt::AlignCenter, valueStr);

    // 主数值单位
    font.setPixelSize(size / 15);
    font.setBold(false);
    painter.setFont(font);
    painter.drawText(rect.adjusted(0, size/8, 0, size/8), Qt::AlignCenter, m_suffix);

    // Min / Max
    font.setPixelSize(size / 12); // 增大字体由 20 变为 15
    painter.setFont(font);
    painter.setPen(QColor(180, 180, 180));
    
    // 计算圆弧起点 (-225度) 和 终点 (45度) 的坐标
    // 注意：Qt 的 drawArc 使用的角度单位是 1/16 度，且顺时针为负，我们计算位置使用弧度
    float rText = size / 2.0 - 5; // 稍微靠外一点
    float centerX = width() / 2.0;
    float centerY = height() / 2.0;

    // 起点 (Min)
    float minRad = qDegreesToRadians(225.0f); // -(-225)
    QPointF minPos(centerX + rText * cos(minRad), centerY + rText * sin(minRad));
    
    // 终点 (Max)
    float maxRad = qDegreesToRadians(-45.0f); // -(45)
    QPointF maxPos(centerX + rText * cos(maxRad), centerY + rText * sin(maxRad));

    // 绘制第二数值文字（速度）：放在最小值和最大值中间上方
    if (!m_secondSuffix.isEmpty() || m_secondValue != 0) {
        font.setPixelSize(size / 13);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor(255, 120, 0));
        const QString secondStr = QString("V: %1 %2")
                                      .arg(QString::number(m_secondValue, 'f', 1))
                                      .arg(m_secondSuffix);
        const qreal midX = (minPos.x() + maxPos.x()) * 0.5;
        const qreal midY = (minPos.y() + maxPos.y()) * 0.5 - size / 14.0;
        painter.drawText(QRectF(midX - size / 4.0, midY - size / 18.0, size / 2.0, size / 9.0),
                         Qt::AlignCenter,
                         secondStr);
    }

    // 参数名称：若存在速度副数值，移到下方避免与速度文字重叠
    font.setPixelSize(size / 10);
    font.setBold(false);
    painter.setPen(Qt::white);
    painter.setFont(font);
    const bool hasSecondValueDisplay = (!m_secondSuffix.isEmpty() || m_secondValue != 0);
    const qreal labelOffset = hasSecondValueDisplay ? (size / 2.9) : (-size / 3.5);
    painter.drawText(rect.adjusted(0, labelOffset, 0, labelOffset), Qt::AlignCenter, m_labelText);

    // 绘制文本，稍微偏移以避免覆盖圆弧
    painter.drawText(QRectF(minPos.x() - 35, minPos.y() - 20, 40, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(m_minimum));
    painter.drawText(QRectF(maxPos.x()- 5, maxPos.y() - 20, 40, 20), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_maximum));
}

void TechArcGauge::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void TechArcGauge::requestRepaint()
{
    if (!m_repaintElapsed.isValid()) {
        m_repaintElapsed.start();
        update();
        return;
    }
    if (m_repaintElapsed.elapsed() >= m_minRepaintIntervalMs) {
        m_repaintElapsed.restart();
        update();
    }
}

QColor TechArcGauge::interpolate(const QColor &start, const QColor &end, double t)
{
    return QColor(
        start.red() + (end.red() - start.red()) * t,
        start.green() + (end.green() - start.green()) * t,
        start.blue() + (end.blue() - start.blue()) * t,
        start.alpha() + (end.alpha() - start.alpha()) * t
    );
}
