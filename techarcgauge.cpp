#include "techarcgauge.h"
#include <QPainterPath>
#include <QConicalGradient>
#include <QDebug>

TechArcGauge::TechArcGauge(QWidget *parent)
    : QWidget(parent)
    , m_value(0.0)
    , m_minimum(0.0)
    , m_maximum(100.0)
    , m_precision(0)
    , m_suffix("")
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
        
        if (m_forceControlEnabled) {
            double range = m_maximum - m_minimum;
            if (range > 0) {
                double center = (m_maximum + m_minimum) / 2.0;
                double deviation = qAbs(m_value - center) / (range / 2.0);
                deviation = qBound(0.0, deviation, 1.0);
                
                QColor targetRed(255, 51, 51);
                m_primaryColor = interpolate(m_originalPrimaryColor, targetRed, deviation);
                m_glowColor = interpolate(m_originalGlowColor, QColor(255, 51, 51, 120), deviation);
            }
        }
        
        emit valueChanged(m_value);
        requestRepaint();
    }
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

void TechArcGauge::setPrecision(int precision)
{
    m_precision = qMax(0, precision);
    requestRepaint();
}

void TechArcGauge::setForceControlMode(bool enabled)
{
    m_forceControlEnabled = enabled;
    if (!enabled) {
        m_primaryColor = m_originalPrimaryColor;
        m_glowColor = m_originalGlowColor;
    } else {
        setValue(m_value);
    }
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

    // 绘制文字
    painter.setPen(Qt::white);
    QFont font = painter.font();
    
    // 当前值
    font.setPixelSize(size / 6);
    font.setBold(true);
    painter.setFont(font);
    QString valueStr = QString::number(m_value, 'f', m_precision);
    painter.drawText(rect, Qt::AlignCenter, valueStr);

    // 后缀/单位 (在数值下方)
    font.setPixelSize(size / 12);
    font.setBold(false);
    painter.setFont(font);
    painter.drawText(rect.adjusted(0, size/4, 0, 0), Qt::AlignCenter, m_suffix);

    // 参数名称 (在数值上方)
    font.setPixelSize(size / 14);
    painter.setFont(font);
    painter.drawText(rect.adjusted(0, -size/4, 0, 0), Qt::AlignCenter, m_labelText);
    
    // Min / Max
    font.setPixelSize(size / 18);
    painter.setFont(font);
    painter.setPen(QColor(200, 200, 200));
    // 稍微计算一下起止点位置绘制Min/Max文本
    painter.drawText(rect.adjusted(size*0.1, size*0.35, -size*0.1, -size*0.05), Qt::AlignLeft | Qt::AlignBottom, QString::number(m_minimum));
    painter.drawText(rect.adjusted(size*0.1, size*0.35, -size*0.1, -size*0.05), Qt::AlignRight | Qt::AlignBottom, QString::number(m_maximum));
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
