#include "techshapes.h"

#include <QLineF>
#include <QPointF>
#include <QtMath>

namespace TechShapes {

namespace {

QPointF unitVector(const QPointF &vector)
{
    const qreal length = qSqrt(vector.x() * vector.x() + vector.y() * vector.y());
    if (length < 1e-6) {
        return QPointF(0.0, 0.0);
    }
    return QPointF(vector.x() / length, vector.y() / length);
}

} // namespace

QPainterPath chamferedRect(const QRectF &rect, qreal cut)
{
    const qreal maxCut = qMin(rect.width(), rect.height()) / 4.0;
    cut = qBound(0.0, cut, maxCut);

    QPainterPath path;
    path.moveTo(rect.left() + cut, rect.top());
    path.lineTo(rect.right() - cut, rect.top());
    path.lineTo(rect.right(), rect.top() + cut);
    path.lineTo(rect.right(), rect.bottom() - cut);
    path.lineTo(rect.right() - cut, rect.bottom());
    path.lineTo(rect.left() + cut, rect.bottom());
    path.lineTo(rect.left(), rect.bottom() - cut);
    path.lineTo(rect.left(), rect.top() + cut);
    path.closeSubpath();
    return path;
}

QPainterPath parallelogramRect(const QRectF &rect, qreal slant)
{
    const qreal maxSlant = qMin(rect.width(), rect.height()) / 3.0;
    slant = qBound(0.0, slant, maxSlant);

    QPainterPath path;
    path.moveTo(rect.left() + slant, rect.top());
    path.lineTo(rect.right(), rect.top());
    path.lineTo(rect.right() - slant, rect.bottom());
    path.lineTo(rect.left(), rect.bottom());
    path.closeSubpath();
    return path;
}

QPainterPath parallelogramChamferedRect(const QRectF &rect, qreal slant, qreal cornerCut)
{
    const qreal maxSlant = qMin(rect.width(), rect.height()) / 3.0;
    slant = qBound(0.0, slant, maxSlant);

    const QPointF verts[4] = {
        QPointF(rect.left() + slant, rect.top()),
        QPointF(rect.right(), rect.top()),
        QPointF(rect.right() - slant, rect.bottom()),
        QPointF(rect.left(), rect.bottom()),
    };

    qreal maxCornerCut = cornerCut;
    for (int i = 0; i < 4; ++i) {
        const qreal edgeLength = QLineF(verts[i], verts[(i + 1) % 4]).length();
        maxCornerCut = qMin(maxCornerCut, edgeLength / 2.0);
    }
    cornerCut = qBound(0.0, cornerCut, maxCornerCut);

    if (cornerCut <= 0.0) {
        return parallelogramRect(rect, slant);
    }

    QPainterPath path;
    path.moveTo(verts[0] + cornerCut * unitVector(verts[1] - verts[0]));
    for (int i = 1; i < 4; ++i) {
        path.lineTo(verts[i] - cornerCut * unitVector(verts[i] - verts[i - 1]));
        path.lineTo(verts[i] + cornerCut * unitVector(verts[(i + 1) % 4] - verts[i]));
    }
    path.lineTo(verts[0] - cornerCut * unitVector(verts[0] - verts[3]));
    path.closeSubpath();
    return path;
}

} // namespace TechShapes
