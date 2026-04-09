#include "mainmodbuslabelmapper.h"

#include <QLabel>
#include <QStringList>
#include <QWidget>

namespace {
QString extractAddressToken(const QString &objectName)
{
    if (objectName.contains("_MX") || objectName.contains("_MW")) {
        const int mxPos = objectName.indexOf("_MX");
        const int mwPos = objectName.indexOf("_MW");
        const int pos = (mxPos != -1) ? mxPos : mwPos;
        if (pos != -1) {
            return objectName.mid(pos + 1);
        }
    }
    return objectName;
}
} // namespace

QMap<int, QLabel *> MainModbusLabelMapper::buildMap(QWidget *rootWidget)
{
    QMap<int, QLabel *> map;
    if (!rootWidget) {
        return map;
    }

    const QList<QLabel *> allLabels = rootWidget->findChildren<QLabel *>();
    for (QLabel *label : allLabels) {
        const QString objectName = label->objectName();
        if (!objectName.contains("MX") && !objectName.contains("MW")) {
            continue;
        }

        const QString addrStr = extractAddressToken(objectName);
        if (addrStr.startsWith("MX")) {
            const QStringList parts = addrStr.mid(2).split('_');
            if (parts.size() != 2) {
                continue;
            }
            const int address = parts[0].toInt();
            const int bitPos = parts[1].toInt();
            const int key = address * 1000 + bitPos;
            map[key] = label;
            label->setText("等待读取...");
            label->setStyleSheet("color: gray;");
        } else if (addrStr.startsWith("MW")) {
            const int address = addrStr.mid(2).toInt();
            map[address] = label;
            label->setText("等待读取...");
            label->setStyleSheet("color: gray;");
        }
    }

    return map;
}
