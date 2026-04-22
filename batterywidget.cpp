#include "batterywidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QtMath>

BatteryWidget::BatteryWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(60);
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        const qreal step = 0.05;
        if (m_pulseDown) {
            m_pulseOpacity -= step;
            if (m_pulseOpacity <= 0.45) {
                m_pulseOpacity = 0.45;
                m_pulseDown = false;
            }
        } else {
            m_pulseOpacity += step;
            if (m_pulseOpacity >= 1.0) {
                m_pulseOpacity = 1.0;
                m_pulseDown = true;
            }
        }
        update();
    });
}

void BatteryWidget::setLevel(double val)
{
    const double clamped = qBound(0.0, val, 100.0);
    if (qFuzzyCompare(m_level, clamped)) {
        return;
    }
    m_level = clamped;
    emit levelChanged();
    update();
}

void BatteryWidget::setCharging(bool val)
{
    if (m_charging == val) {
        return;
    }
    m_charging = val;
    if (m_charging) {
        m_pulseTimer->start();
    } else {
        m_pulseTimer->stop();
        m_pulseOpacity = 1.0;
        m_pulseDown = false;
    }
    emit chargingChanged();
    update();
}

QSize BatteryWidget::sizeHint() const
{
    return QSize(60, 30);
}

QSize BatteryWidget::minimumSizeHint() const
{
    return QSize(48, 24);
}

QColor BatteryWidget::batteryColor() const
{
    if (m_level < 20.0) {
        return QColor("#ff4d4d");
    }
    if (m_level < 40.0) {
        return QColor("#ffa500");
    }
    return QColor("#00f0ff");
}

void BatteryWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF fullRect = rect().adjusted(1, 1, -1, -1);
    if (fullRect.width() < 12 || fullRect.height() < 10) {
        return;
    }

    const qreal capWidth = qMax<qreal>(3.0, fullRect.width() * 0.08);
    QRectF bodyRect = fullRect;
    bodyRect.setRight(fullRect.right() - capWidth - 2);

    QRectF capRect(bodyRect.right() + 2,
                   bodyRect.top() + bodyRect.height() * 0.3,
                   capWidth,
                   bodyRect.height() * 0.4);

    const QColor borderColor("#a9d4ff");
    const QColor fillColor = batteryColor();
    const qreal alpha = m_charging ? m_pulseOpacity : 0.9;

    painter.setPen(QPen(borderColor, 1.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bodyRect, 3, 3);

    painter.setPen(Qt::NoPen);
    painter.setBrush(borderColor);
    painter.drawRoundedRect(capRect, 1, 1);

    QRectF inner = bodyRect.adjusted(2.5, 2.5, -2.5, -2.5);
    inner.setWidth(inner.width() * (m_level / 100.0));
    if (inner.width() > 0.5) {
        QColor dynamicFill = fillColor;
        dynamicFill.setAlphaF(alpha);
        painter.setBrush(dynamicFill);
        painter.drawRoundedRect(inner, 1.5, 1.5);
    }

    // 充电态叠加闪电图标，保证在明亮背景下也清晰可见。
    if (m_charging) {
        const qreal iconSize = qMax<qreal>(8.0, qMin(bodyRect.width(), bodyRect.height()) * 0.55);
        const qreal iconX = bodyRect.left() + bodyRect.width() * 0.16;
        const qreal iconY = bodyRect.center().y() - iconSize * 0.5;
        const QRectF iconRect(iconX, iconY, iconSize, iconSize);

        QPolygonF bolt;
        bolt << QPointF(iconRect.left() + iconRect.width() * 0.55, iconRect.top())
             << QPointF(iconRect.left() + iconRect.width() * 0.28, iconRect.top() + iconRect.height() * 0.45)
             << QPointF(iconRect.left() + iconRect.width() * 0.54, iconRect.top() + iconRect.height() * 0.45)
             << QPointF(iconRect.left() + iconRect.width() * 0.35, iconRect.bottom())
             << QPointF(iconRect.left() + iconRect.width() * 0.72, iconRect.top() + iconRect.height() * 0.56)
             << QPointF(iconRect.left() + iconRect.width() * 0.46, iconRect.top() + iconRect.height() * 0.56);

        QColor boltFill("#ffd84d");
        boltFill.setAlphaF(0.55 + 0.45 * m_pulseOpacity);
        painter.setPen(QPen(QColor("#fff3a8"), 1.0));
        painter.setBrush(boltFill);
        painter.drawPolygon(bolt);
    }

    if (width() >= 48) {
        QString txt = QString::number(qRound(m_level)) + "%";
        QColor textColor = (m_level > 50.0) ? QColor("#003344") : QColor("#ffffff");
        painter.setPen(textColor);
        QFont f = painter.font();
        f.setBold(true);
        f.setPixelSize(qMax(9, height() / 3));
        painter.setFont(f);
        painter.drawText(bodyRect, Qt::AlignCenter, txt);
    }
}
