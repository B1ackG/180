#ifndef NAVIGATIONICON_H
#define NAVIGATIONICON_H

#include <QColor>
#include <QIcon>
#include <QSize>

enum class NavIconKind {
    SystemMenu,
    Home,
    Permission,
    History,
    Craft,
    DeviceMenu,
    Fixture,
    Tighten,
    Chassis,
    SixAxis,
    ControlMenu,
    StepMove,
    WiredControl,
    JointMode,
    ClearAlarm
};

QIcon navigationIcon(NavIconKind kind,
                     const QSize &size,
                     const QColor &color = QColor(0, 200, 255));

#endif // NAVIGATIONICON_H
