#include "techchamfertoolbutton.h"
#include "techshapes.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif

namespace {
constexpr qreal kBorderWidth = 2.0;
constexpr char kTransparentToolButtonStyle[] =
    "QToolButton {"
    "  background: transparent;"
    "  border: none;"
    "  border-radius: 0;"
    "  color: #dff6ff;"
    "  padding: 4px 10px;"
    "}"
    "QToolButton:hover {"
    "  color: #ffffff;"
    "}";
}

TechChamferToolButton::TechChamferToolButton(QWidget *parent)
    : QToolButton(parent)
{
    setAutoRaise(false);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(QString::fromLatin1(kTransparentToolButtonStyle));
    connect(this, &QToolButton::toggled, this, QOverload<>::of(&QWidget::update));
}

void TechChamferToolButton::setFillColor(const QColor &color)
{
    if (m_fillColor != color) {
        m_fillColor = color;
        emit fillColorChanged();
        update();
    }
}

void TechChamferToolButton::setBorderColor(const QColor &color)
{
    if (m_borderColor != color) {
        m_borderColor = color;
        emit borderColorChanged();
        update();
    }
}

void TechChamferToolButton::setCheckedFillColor(const QColor &color)
{
    if (m_checkedFillColor != color) {
        m_checkedFillColor = color;
        emit checkedFillColorChanged();
        update();
    }
}

void TechChamferToolButton::setCheckedBorderColor(const QColor &color)
{
    if (m_checkedBorderColor != color) {
        m_checkedBorderColor = color;
        emit checkedBorderColorChanged();
        update();
    }
}

void TechChamferToolButton::setChamferSize(int size)
{
    size = qMax(0, size);
    if (m_chamferSize != size) {
        m_chamferSize = size;
        emit chamferSizeChanged();
        update();
    }
}

void TechChamferToolButton::setColors(const QColor &fill, const QColor &border)
{
    setFillColor(fill);
    setBorderColor(border);
}

qreal TechChamferToolButton::effectiveSlant() const
{
    return qMin<qreal>(12.0, height() / 3.0);
}

qreal TechChamferToolButton::effectiveCornerCut() const
{
    if (m_chamferSize > 0) {
        return m_chamferSize;
    }
    return qMin<qreal>(8.0, height() / 4.0);
}

QColor TechChamferToolButton::effectiveFillColor() const
{
    const bool checked = isCheckable() && isChecked();
    QColor base = checked ? m_checkedFillColor : m_fillColor;
    if (!isEnabled()) {
        return QColor(base.red(), base.green(), base.blue(), qMax(60, base.alpha() * 2 / 3));
    }
    if (isDown()) {
        return QColor(base.red(), base.green(), base.blue(),
                      qBound(80, static_cast<int>(base.alpha() * 1.1), 255));
    }
    if (underMouse() && !checked) {
        return QColor(qMin(base.red() + 8, 255),
                      qMin(base.green() + 10, 255),
                      qMin(base.blue() + 12, 255),
                      base.alpha());
    }
    return base;
}

QColor TechChamferToolButton::effectiveBorderColor() const
{
    const bool checked = isCheckable() && isChecked();
    QColor base = checked ? m_checkedBorderColor : m_borderColor;
    if (!isEnabled()) {
        return QColor(base.red(), base.green(), base.blue(), base.alpha() / 2);
    }
    if (isDown()) {
        return base.darker(110);
    }
    if (underMouse() && !checked) {
        return QColor(qMin(base.red() + 20, 255),
                     qMin(base.green() + 20, 255),
                     qMin(base.blue() + 20, 255),
                     qMin(base.alpha() + 30, 255));
    }
    return base;
}

QColor TechChamferToolButton::effectiveTextColor() const
{
    if (!isEnabled()) {
        return QColor(0x99, 0xaa, 0xb8);
    }
    if (underMouse()) {
        return QColor(0xff, 0xff, 0xff);
    }
    return QColor(0xdf, 0xf6, 0xff);
}

void TechChamferToolButton::drawContent(QPainter &painter, const QRect &rect) const
{
    const QSize iconSz = iconSize().isValid() ? iconSize() : QSize(22, 22);
    const bool hasIcon = !icon().isNull();
    const bool hasText = !text().isEmpty();
    const int hPad = 12;
    const int iconGap = 8;
    const int centerY = rect.center().y();

    QPixmap pixmap;
    if (hasIcon) {
        pixmap = icon().pixmap(iconSz, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    }

    QFont font = this->font();
    font.setBold(true);
    const QFontMetrics fm(font);
    const int textWidth = hasText ? fm.horizontalAdvance(text()) : 0;
    const int iconWidth = !pixmap.isNull() ? pixmap.width() : 0;
    const int iconBlockWidth = iconWidth > 0 ? iconWidth + (hasText ? iconGap : 0) : 0;
    const int totalWidth = iconBlockWidth + textWidth;

    int contentLeft = rect.left() + (rect.width() - totalWidth) / 2;
    const int minLeft = rect.left() + hPad;
    const int maxLeft = rect.right() - hPad - totalWidth;
    if (maxLeft >= minLeft) {
        contentLeft = qBound(minLeft, contentLeft, maxLeft);
    } else {
        contentLeft = minLeft;
    }

    if (!pixmap.isNull()) {
        const int iconY = centerY - pixmap.height() / 2;
        painter.drawPixmap(contentLeft, iconY, pixmap);
        contentLeft += pixmap.width() + iconGap;
    }

    if (hasText) {
        painter.setFont(font);
        painter.setPen(effectiveTextColor());

        QRect textRect(contentLeft, rect.top(), textWidth, rect.height());
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

void TechChamferToolButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath shapePath = TechShapes::parallelogramChamferedRect(
        bounds, effectiveSlant(), effectiveCornerCut());

    painter.fillPath(shapePath, effectiveFillColor());

    QPen borderPen(effectiveBorderColor(), kBorderWidth);
    borderPen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shapePath);

    drawContent(painter, rect());
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void TechChamferToolButton::enterEvent(QEnterEvent *event)
#else
void TechChamferToolButton::enterEvent(QEvent *event)
#endif
{
    QToolButton::enterEvent(event);
    update();
}

void TechChamferToolButton::leaveEvent(QEvent *event)
{
    QToolButton::leaveEvent(event);
    update();
}
