#include "navigationicon.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

constexpr qreal kDefaultLineWidth = 2.0;

void beginIconStroke(QPainter &painter, const QColor &color, qreal lineWidth = kDefaultLineWidth)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
}

qreal edge(const QSize &size)
{
    return qMin(size.width(), size.height()) * 0.14;
}

void drawSystemMenu(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const qreal gap = r.height() / 4.0;
    for (int i = 0; i < 3; ++i) {
        const qreal y = r.top() + gap * (i + 1);
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }
}

void drawHome(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const qreal w = r.width();
    const qreal h = r.height();
    QPainterPath roof;
    roof.moveTo(r.center().x(), r.top());
    roof.lineTo(r.right(), r.top() + h * 0.42);
    roof.lineTo(r.left(), r.top() + h * 0.42);
    roof.closeSubpath();
    p.drawPath(roof);
    p.drawRect(QRectF(r.left() + w * 0.2, r.top() + h * 0.42, w * 0.6, h * 0.48));
}

void drawPermission(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    QPainterPath shield;
    const qreal cx = r.center().x();
    shield.moveTo(cx, r.top());
    shield.lineTo(r.right(), r.top() + r.height() * 0.28);
    shield.lineTo(r.right(), r.top() + r.height() * 0.62);
    shield.quadTo(cx, r.bottom(), r.left(), r.top() + r.height() * 0.62);
    shield.lineTo(r.left(), r.top() + r.height() * 0.28);
    shield.closeSubpath();
    p.drawPath(shield);
    p.drawLine(QPointF(cx, r.top() + r.height() * 0.35),
               QPointF(cx, r.top() + r.height() * 0.72));
}

void drawHistory(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const QPointF c = r.center();
    const qreal radius = qMin(r.width(), r.height()) * 0.38;
    p.drawEllipse(c, radius, radius);
    p.drawLine(c, QPointF(c.x(), c.y() - radius * 0.15));
    p.drawLine(c, QPointF(c.x() + radius * 0.45, c.y() + radius * 0.2));
}

void drawCraft(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const qreal w = r.width();
    const qreal h = r.height();
    p.drawLine(QPointF(r.left() + w * 0.15, r.bottom() - h * 0.15),
               QPointF(r.right() - w * 0.2, r.top() + h * 0.25));
    p.drawLine(QPointF(r.right() - w * 0.35, r.top() + h * 0.1),
               QPointF(r.right() - w * 0.05, r.top() + h * 0.4));
    p.drawEllipse(QRectF(r.left() + w * 0.05, r.bottom() - h * 0.32, w * 0.22, h * 0.22));
}

void drawDeviceMenu(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    QRectF box(r.left() + r.width() * 0.18, r.top() + r.height() * 0.18,
               r.width() * 0.64, r.height() * 0.64);
    p.drawRoundedRect(box, 3, 3);
    const qreal dot = qMax(2.0, r.width() * 0.08);
    p.setBrush(color);
    p.drawEllipse(QRectF(box.left() - dot * 0.3, box.top() - dot * 0.3, dot, dot));
    p.drawEllipse(QRectF(box.right() - dot * 0.7, box.top() - dot * 0.3, dot, dot));
    p.setBrush(Qt::NoBrush);
}

void drawFixture(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const qreal mid = r.center().x();
    const qreal top = r.top() + r.height() * 0.2;
    const qreal bottom = r.bottom() - r.height() * 0.2;
    p.drawLine(QPointF(r.left() + r.width() * 0.15, top),
               QPointF(mid - r.width() * 0.08, bottom));
    p.drawLine(QPointF(r.left() + r.width() * 0.15, bottom),
               QPointF(mid - r.width() * 0.08, top));
    p.drawLine(QPointF(r.right() - r.width() * 0.15, top),
               QPointF(mid + r.width() * 0.08, bottom));
    p.drawLine(QPointF(r.right() - r.width() * 0.15, bottom),
               QPointF(mid + r.width() * 0.08, top));
}

void drawTighten(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const QPointF c = r.center();
    const qreal radius = qMin(r.width(), r.height()) * 0.34;
    QPolygonF hex;
    for (int i = 0; i < 6; ++i) {
        const qreal angle = qDegreesToRadians(60.0 * i - 30.0);
        hex << QPointF(c.x() + radius * qCos(angle), c.y() + radius * qSin(angle));
    }
    p.drawPolygon(hex);
    p.drawEllipse(c, radius * 0.22, radius * 0.22);
}

void drawChassis(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    QRectF body(r.left() + r.width() * 0.12, r.top() + r.height() * 0.22,
                r.width() * 0.76, r.height() * 0.38);
    p.drawRoundedRect(body, 3, 3);
    const qreal wheelR = r.height() * 0.12;
    p.drawEllipse(QRectF(body.left() + body.width() * 0.18 - wheelR, body.bottom() - wheelR * 0.2,
                           wheelR * 2, wheelR * 2));
    p.drawEllipse(QRectF(body.right() - body.width() * 0.18 - wheelR, body.bottom() - wheelR * 0.2,
                           wheelR * 2, wheelR * 2));
}

void drawSixAxis(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const QPointF o(r.left() + r.width() * 0.35, r.bottom() - r.height() * 0.28);
    const qreal len = r.width() * 0.42;
    p.drawLine(o, QPointF(o.x() + len, o.y()));
    p.drawLine(o, QPointF(o.x(), o.y() - len * 0.85));
    p.drawLine(o, QPointF(o.x() - len * 0.55, o.y() - len * 0.45));
    auto arrowHead = [&](const QPointF &from, const QPointF &to) {
        const QLineF line(from, to);
        const qreal angle = qAtan2(line.dy(), line.dx());
        const qreal head = 4.0;
        p.drawLine(to, QPointF(to.x() - head * qCos(angle - 0.45), to.y() - head * qSin(angle - 0.45)));
        p.drawLine(to, QPointF(to.x() - head * qCos(angle + 0.45), to.y() - head * qSin(angle + 0.45)));
    };
    arrowHead(o, QPointF(o.x() + len, o.y()));
    arrowHead(o, QPointF(o.x(), o.y() - len * 0.85));
    arrowHead(o, QPointF(o.x() - len * 0.55, o.y() - len * 0.45));
}

void drawControlMenu(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const qreal w = r.width();
    const qreal h = r.height();
    for (int i = 0; i < 2; ++i) {
        const qreal y = r.top() + h * (0.28 + i * 0.36);
        p.drawLine(QPointF(r.left() + w * 0.12, y), QPointF(r.right() - w * 0.12, y));
        const qreal knobX = (i == 0) ? r.left() + w * 0.68 : r.left() + w * 0.32;
        p.setBrush(color);
        p.drawEllipse(QRectF(knobX - w * 0.08, y - h * 0.1, w * 0.16, h * 0.2));
        p.setBrush(Qt::NoBrush);
    }
}

void drawStepMove(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const QPointF c = r.center();
    const qreal arm = qMin(r.width(), r.height()) * 0.32;
    p.drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() + arm, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() + arm));
    auto head = [&](const QPointF &tip, qreal angle) {
        const qreal head = 4.5;
        p.drawLine(tip, QPointF(tip.x() - head * qCos(angle - 0.5), tip.y() - head * qSin(angle - 0.5)));
        p.drawLine(tip, QPointF(tip.x() - head * qCos(angle + 0.5), tip.y() - head * qSin(angle + 0.5)));
    };
    head(QPointF(c.x() + arm, c.y()), 0);
    head(QPointF(c.x() - arm, c.y()), M_PI);
    head(QPointF(c.x(), c.y() - arm), -M_PI_2);
    head(QPointF(c.x(), c.y() + arm), M_PI_2);
}

void drawWiredControl(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    QRectF plug(r.left() + r.width() * 0.42, r.top() + r.height() * 0.18,
                r.width() * 0.38, r.height() * 0.42);
    p.drawRoundedRect(plug, 2, 2);
    p.drawLine(QPointF(plug.left(), plug.top() + plug.height() * 0.35),
               QPointF(r.left() + r.width() * 0.12, plug.top() + plug.height() * 0.35));
    p.drawLine(QPointF(plug.left(), plug.top() + plug.height() * 0.65),
               QPointF(r.left() + r.width() * 0.12, plug.top() + plug.height() * 0.65));
    p.drawLine(QPointF(plug.center().x(), plug.bottom()),
               QPointF(plug.center().x(), r.bottom() - r.height() * 0.12));
}

void drawJointMode(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const QPointF hinge(r.left() + r.width() * 0.38, r.bottom() - r.height() * 0.22);
    p.drawLine(QPointF(r.left() + r.width() * 0.12, hinge.y()),
               QPointF(hinge.x(), hinge.y()));
    p.drawLine(hinge, QPointF(r.right() - r.width() * 0.12, r.top() + r.height() * 0.22));
    p.setBrush(color);
    p.drawEllipse(hinge, r.width() * 0.09, r.width() * 0.09);
    p.setBrush(Qt::NoBrush);
}

void drawClearAlarm(QPainter &p, const QRectF &r, const QColor &color)
{
    beginIconStroke(p, color);
    const qreal w = r.width();
    const qreal h = r.height();
    const QPointF apex(r.center().x(), r.top() + h * 0.08);
    const QPointF bl(r.left() + w * 0.12, r.bottom() - h * 0.08);
    const QPointF br(r.right() - w * 0.12, r.bottom() - h * 0.08);

    QPainterPath triangle;
    triangle.moveTo(apex);
    triangle.lineTo(bl);
    triangle.lineTo(br);
    triangle.closeSubpath();
    p.drawPath(triangle);

    const qreal cx = r.center().x();
    p.drawLine(QPointF(cx, r.top() + h * 0.38), QPointF(cx, r.top() + h * 0.58));
    p.setBrush(color);
    p.drawEllipse(QRectF(cx - w * 0.05, r.top() + h * 0.62, w * 0.1, h * 0.1));
    p.setBrush(Qt::NoBrush);

    p.drawLine(QPointF(r.left() + w * 0.58, r.top() + h * 0.58),
               QPointF(r.right() - w * 0.08, r.bottom() - h * 0.1));
}

void paintNavIcon(QPainter &painter, NavIconKind kind, const QSize &size, const QColor &color)
{
    const qreal m = edge(size);
    const QRectF bounds(m, m, size.width() - 2 * m, size.height() - 2 * m);

    switch (kind) {
    case NavIconKind::SystemMenu:
        drawSystemMenu(painter, bounds, color);
        break;
    case NavIconKind::Home:
        drawHome(painter, bounds, color);
        break;
    case NavIconKind::Permission:
        drawPermission(painter, bounds, color);
        break;
    case NavIconKind::History:
        drawHistory(painter, bounds, color);
        break;
    case NavIconKind::Craft:
        drawCraft(painter, bounds, color);
        break;
    case NavIconKind::DeviceMenu:
        drawDeviceMenu(painter, bounds, color);
        break;
    case NavIconKind::Fixture:
        drawFixture(painter, bounds, color);
        break;
    case NavIconKind::Tighten:
        drawTighten(painter, bounds, color);
        break;
    case NavIconKind::Chassis:
        drawChassis(painter, bounds, color);
        break;
    case NavIconKind::SixAxis:
        drawSixAxis(painter, bounds, color);
        break;
    case NavIconKind::ControlMenu:
        drawControlMenu(painter, bounds, color);
        break;
    case NavIconKind::StepMove:
        drawStepMove(painter, bounds, color);
        break;
    case NavIconKind::WiredControl:
        drawWiredControl(painter, bounds, color);
        break;
    case NavIconKind::JointMode:
        drawJointMode(painter, bounds, color);
        break;
    case NavIconKind::ClearAlarm:
        drawClearAlarm(painter, bounds, color);
        break;
    }
}

} // namespace

QIcon navigationIcon(NavIconKind kind, const QSize &size, const QColor &color)
{
    if (size.width() <= 0 || size.height() <= 0) {
        return {};
    }

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    paintNavIcon(painter, kind, size, color);
    painter.end();

    return QIcon(pixmap);
}
