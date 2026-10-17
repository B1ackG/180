#include "motionwriteplan.h"

#include "modbusregistercodec.h"
#include "modbusthreadmanager.h"

#include <array>

bool buildSixAxisStepWritePlan(int axisIndex,
                               float stepValue,
                               quint64 generation,
                               SixAxisStepWritePlan &out)
{
    out.steps.clear();
    if (axisIndex < 1 || axisIndex > 6) {
        return false;
    }

    const std::array<quint16, 2> regs = ModbusRegisterCodec::floatToCdab(stepValue);
    ModbusWriteCommand axisCommand;
    axisCommand.start = 614;
    axisCommand.values = {static_cast<quint16>(axisIndex)};
    axisCommand.generation = generation;
    axisCommand.generationChannel = ModbusThreadManager::kSixAxisExternalWriteChannel;

    ModbusWriteCommand stepCommand;
    stepCommand.start = 601;
    stepCommand.values = {regs[0], regs[1]};
    stepCommand.generation = generation;
    stepCommand.generationChannel = ModbusThreadManager::kSixAxisExternalWriteChannel;

    ModbusWriteCommand triggerCommand;
    triggerCommand.start = 615;
    triggerCommand.values = {1};
    triggerCommand.generation = generation;
    triggerCommand.generationChannel = ModbusThreadManager::kSixAxisExternalWriteChannel;

    out.steps = {axisCommand, stepCommand, triggerCommand};
    return true;
}
