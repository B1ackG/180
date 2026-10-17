#ifndef MOTIONWRITEPLAN_H
#define MOTIONWRITEPLAN_H

#include "modbusasync.h"

#include <QtGlobal>

struct SixAxisStepWritePlan {
    QVector<ModbusWriteCommand> steps;
};

bool buildSixAxisStepWritePlan(int axisIndex,
                               float stepValue,
                               quint64 generation,
                               SixAxisStepWritePlan &out);

#endif
