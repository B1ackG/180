#include "registerbitutils.h"

namespace RegisterBitUtils {

bool applyMaskedWrite(quint16 current, quint16 orMask, quint16 andMask, quint16 &out)
{
    out = static_cast<quint16>((current | orMask) & andMask);
    return true;
}

bool applyBitUpdates(quint16 current,
                     const QVector<QPair<int, bool>> &updates,
                     quint16 &out)
{
    quint16 next = current;
    for (const QPair<int, bool> &update : updates) {
        const int bit = update.first;
        if (bit < 0 || bit > 15) {
            out = current;
            return false;
        }
        const quint16 mask = static_cast<quint16>(1u << bit);
        if (update.second) {
            next = static_cast<quint16>(next | mask);
        } else {
            next = static_cast<quint16>(next & static_cast<quint16>(~mask));
        }
    }
    out = next;
    return true;
}

}
