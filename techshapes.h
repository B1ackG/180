#ifndef TECHSHAPES_H
#define TECHSHAPES_H

#include <QPainterPath>
#include <QRectF>

namespace TechShapes {

QPainterPath chamferedRect(const QRectF &rect, qreal cut);
QPainterPath parallelogramRect(const QRectF &rect, qreal slant);
QPainterPath parallelogramChamferedRect(const QRectF &rect, qreal slant, qreal cornerCut);

} // namespace TechShapes

#endif // TECHSHAPES_H
