#include "modebuttonstyler.h"

void ModeButtonStyler::configureInteractiveButton(TechPushButton *button,
                                                  const QSize &size,
                                                  const QString &objectName)
{
    if (!button) {
        return;
    }

    if (!objectName.isEmpty()) {
        button->setObjectName(objectName);
    }
    button->setFixedSize(size);
    button->enableClickAnimation(true);
    button->enableHoverAnimation(true);
    button->setTextGlow(true);
}

void ModeButtonStyler::applyTextColor(const QList<TechPushButton *> &buttons,
                                      const QColor &textColor)
{
    for (TechPushButton *button : buttons) {
        if (!button) {
            continue;
        }
        button->setTextColor(textColor);
    }
}

void ModeButtonStyler::applyGroupStyle(const QList<TechPushButton *> &buttons,
                                       int activeIndex,
                                       const QColor &activeColor,
                                       const QColor &inactiveColor,
                                       const QColor &inactiveTextColor,
                                       TechPushButton::ButtonStyle style,
                                       bool enablePulseOnActive)
{
    for (int index = 0; index < buttons.size(); ++index) {
        TechPushButton *button = buttons[index];
        if (!button) {
            continue;
        }

        const bool isActive = (index == activeIndex);
        const QColor baseColor = isActive ? activeColor : inactiveColor;
        button->setPrimaryColor(baseColor);
        button->setGlowColor(isActive ? activeColor.lighter(150) : inactiveColor);
        button->setTextColor(isActive ? QColor(Qt::white) : inactiveTextColor);
        button->enablePulseEffect(isActive && enablePulseOnActive);
        button->setButtonStyle(style);
        button->update();
    }
}
