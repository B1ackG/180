#ifndef MODBUSREGISTERCODEC_H
#define MODBUSREGISTERCODEC_H

#include <array>
#include <QtGlobal>

namespace ModbusRegisterCodec {

std::array<quint16, 2> floatToCdab(float value);
float floatFromCdab(quint16 first, quint16 second);
std::array<quint16, 4> doubleToGhefCdab(double value);
double doubleFromGhefCdab(const std::array<quint16, 4> &regs);

}

#endif
