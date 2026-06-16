#include "modbusstringregisters.h"

#include <QByteArray>

QString decodeUtf8FromRegisters(const quint16 *regs, int regCount)
{
    if (!regs || regCount <= 0) {
        return QString();
    }

    QByteArray bytes;
    bytes.reserve(regCount * 2);
    for (int i = 0; i < regCount; ++i) {
        bytes.append(static_cast<char>((regs[i] >> 8) & 0xFF));
        bytes.append(static_cast<char>(regs[i] & 0xFF));
    }
    while (!bytes.isEmpty() && static_cast<quint8>(bytes.back()) == 0) {
        bytes.chop(1);
    }
    return QString::fromUtf8(bytes);
}

QString decodeUtf8FromRegisters(const QVector<quint16> &regs)
{
    return decodeUtf8FromRegisters(regs.constData(), regs.size());
}

void encodeUtf8ToRegisters(const QString &utf8, int regCount, quint16 *regs)
{
    if (!regs || regCount <= 0) {
        return;
    }

    QByteArray bytes = utf8.toUtf8();
    if (bytes.size() < regCount * 2) {
        bytes.append(QByteArray(regCount * 2 - bytes.size(), '\0'));
    } else if (bytes.size() > regCount * 2) {
        bytes.truncate(regCount * 2);
    }

    for (int i = 0; i < regCount; ++i) {
        const quint8 hi = static_cast<quint8>(bytes.at(i * 2));
        const quint8 lo = static_cast<quint8>(bytes.at(i * 2 + 1));
        regs[i] = static_cast<quint16>((static_cast<quint16>(hi) << 8) | lo);
    }
}

QVector<quint16> encodeUtf8ToRegisters(const QString &utf8, int regCount)
{
    QVector<quint16> regs(regCount, 0);
    encodeUtf8ToRegisters(utf8, regCount, regs.data());
    return regs;
}
