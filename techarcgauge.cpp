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
    , m_secondLabelText()
    , m_modbusAddress(-1)
    , m_primaryColor(QColor(0, 168, 220))
    , m_glowColor(QColor(111, 231, 255, 110))
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
    const bool changed = qAbs(m_value - value) > 0.0001;
    const QColor oldPrimary = m_primaryColor;
    m_value = value;
    updateRangeLimitColors();
    if (changed) {
        emit valueChanged(m_value);
    }
    if (changed || m_primaryColor != oldPrimary) {
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
    if (m_labelText == text) {
        return;
    }
    m_labelText = text;
    requestRepaint();
}

void TechArcGauge::setSecondLabelText(const QString &text)
{
    if (m_secondLabelText == text) {
        return;
    }
    m_secondLabelText = text;
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

void TechArcGauge::updateRangeLimitColors()
{
    constexpr double kEps = 1e-6;
    m_atRangeLimit = (m_maximum > m_minimum)
            && (m_value <= m_minimum + kEps || m_value >= m_maximum - kEps);
    if (m_atRangeLimit) {
        m_primaryColor = QColor(255, 51, 51);
        m_glowColor = QColor(255, 51, 51, 180);
    } else {
        m_primaryColor = m_originalPrimaryColor;
        m_glowColor = m_originalGlowColor;
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
    backPen.setColor(QColor(74, 190, 238, 44));
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
        secondPen.setColor(QColor(255, 194, 92, 190));
        secondPen.setWidth(4);
        secondPen.setCapStyle(Qt::RoundCap);
        
        painter.setPen(secondPen);
        painter.drawArc(rect.adjusted(25, 25, -25, -25), startAngle * 16, secondSpan * 16);
    }

    // 绘制文字
    painter.setPen(QColor("#f2fbff"));
    QFont font = painter.font();

    // 当前主数值（长度/角度）
    font.setPixelSize(size / 6);
    font.setBold(true);
    painter.setPen(QColor("#f2fbff"));
    painter.setFont(font);
    QString valueStr = QString::number(m_value, 'f', m_precision);
    painter.drawText(rect.adjusted(0, -size/15, 0, -size/15), Qt::AlignCenter, valueStr);

    // 主数值单位
    font.setPixelSize(size / 15);
    font.setBold(false);
    painter.setFont(font);
    const qreal suffixOffset = size / 8.0;
    painter.drawText(rect.adjusted(0, suffixOffset, 0, suffixOffset), Qt::AlignCenter, m_suffix);

    const qreal upperLabelOffset = -size / 4.5;
    const qreal lowerLabelOffset = size / 3.8;

    // labelText：上方（与原 secondLabelText 位置相同）
    if (!m_labelText.isEmpty()) {
        font.setPixelSize(size / 10);
        font.setBold(false);
        painter.setPen(m_atRangeLimit ? QColor(255, 51, 51) : QColor("#d8f6ff"));
        painter.setFont(font);
        const QRectF firstLabelRect = rect.adjusted(0, upperLabelOffset, 0, upperLabelOffset);
        painter.drawText(firstLabelRect,
                         Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                         m_labelText);
    }

    // secondLabelText：下方（与原 labelText 位置相同）
    if (!m_secondLabelText.isEmpty()) {
        font.setPixelSize(size / 11);
        font.setBold(false);
        painter.setPen(m_atRangeLimit ? QColor(255, 51, 51) : QColor("#a8eaff"));
        painter.setFont(font);
        const QRectF secondLabelRect = rect.adjusted(0, lowerLabelOffset, 0, lowerLabelOffset);
        painter.drawText(secondLabelRect,
                         Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                         m_secondLabelText);
    }

    // Min / Max
    font.setPixelSize(size / 12); // 增大字体由 20 变为 15
    painter.setFont(font);
    painter.setPen(QColor("#91dfff"));
    
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
        painter.setPen(QColor("#ffc25c"));
        const QString secondStr = QString("V: %1 %2")
                                      .arg(QString::number(m_secondValue, 'f', 1))
                                      .arg(m_secondSuffix);
        const qreal midX = (minPos.x() + maxPos.x()) * 0.5;
        const qreal midY = (minPos.y() + maxPos.y()) * 0.5 - size / 14.0;
        painter.drawText(QRectF(midX - size / 4.0, midY - size / 18.0, size / 2.0, size / 9.0),
                         Qt::AlignCenter,
                         secondStr);
    }

    // 绘制文本，稍微偏移以避免覆盖圆弧
    painter.drawText(QRectF(minPos.x() - 30, minPos.y() - 20, 40, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(m_minimum));
    painter.drawText(QRectF(maxPos.x()- 10, maxPos.y() - 20, 40, 20), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_maximum));
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
