#ifndef SIXAXISRUNMODE_H
#define SIXAXISRUNMODE_H

#include <QtGlobal>

/** 六自由度运行模式，数值与主控寄存器 600 一致。 */
enum class SixAxisRunMode : quint16 {
    Unknown = 0,
    Jog = 1,
    Step = 2,
    Diagonal = 3
};

inline SixAxisRunMode sixAxisRunModeFromRegister(quint16 value)
{
    switch (value) {
    case static_cast<quint16>(SixAxisRunMode::Jog):
        return SixAxisRunMode::Jog;
    case static_cast<quint16>(SixAxisRunMode::Step):
        return SixAxisRunMode::Step;
    case static_cast<quint16>(SixAxisRunMode::Diagonal):
        return SixAxisRunMode::Diagonal;
    default:
        return SixAxisRunMode::Unknown;
    }
}

inline SixAxisRunMode nextSixAxisRunMode(SixAxisRunMode current)
{
    switch (current) {
    case SixAxisRunMode::Jog:
        return SixAxisRunMode::Step;
    case SixAxisRunMode::Step:
        return SixAxisRunMode::Diagonal;
    case SixAxisRunMode::Diagonal:
        return SixAxisRunMode::Jog;
    case SixAxisRunMode::Unknown:
    default:
        return SixAxisRunMode::Jog;
    }
}

inline const char *sixAxisRunModeLabel(SixAxisRunMode mode)
{
    switch (mode) {
    case SixAxisRunMode::Jog:
        return "点动模式";
    case SixAxisRunMode::Step:
        return "步进模式";
    case SixAxisRunMode::Diagonal:
        return "斜向模式";
    case SixAxisRunMode::Unknown:
    default:
        return "未选择模式";
    }
}

#endif
