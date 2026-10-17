#ifndef REGISTERBITUTILS_H
#define REGISTERBITUTILS_H

#include <QPair>
#include <QVector>
#include <QtGlobal>

namespace RegisterBitUtils {

bool applyMaskedWrite(quint16 current, quint16 orMask, quint16 andMask, quint16 &out);
bool applyBitUpdates(quint16 current,
                     const QVector<QPair<int, bool>> &updates,
                     quint16 &out);

}

#endif
