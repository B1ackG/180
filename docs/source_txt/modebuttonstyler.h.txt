#ifndef MODEBUTTONSTYLER_H
#define MODEBUTTONSTYLER_H

#include <QColor>
#include <QList>
#include <QSize>
#include <QString>

#include "techpushbutton.h"

class ModeButtonStyler
{
public:
    static void configureInteractiveButton(TechPushButton *button,
                                           const QSize &size,
                                           const QString &objectName = QString());

    static void applyTextColor(const QList<TechPushButton *> &buttons,
                               const QColor &textColor);

    static void applyGroupStyle(const QList<TechPushButton *> &buttons,
                                int activeIndex,
                                const QColor &activeColor,
                                const QColor &inactiveColor,
                                const QColor &inactiveTextColor,
                                TechPushButton::ButtonStyle style,
                                bool enablePulseOnActive);
};

#endif // MODEBUTTONSTYLER_H
