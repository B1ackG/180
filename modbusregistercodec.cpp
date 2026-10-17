#include "modbusregistercodec.h"

#include <cstring>

namespace ModbusRegisterCodec {

std::array<quint16, 2> floatToCdab(float value)
{
    quint32 raw = 0;
    std::memcpy(&raw, &value, sizeof(float));

    const quint8 A = static_cast<quint8>((raw >> 24) & 0xFF);
    const quint8 B = static_cast<quint8>((raw >> 16) & 0xFF);
    const quint8 C = static_cast<quint8>((raw >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(raw & 0xFF);

    return {
        static_cast<quint16>((static_cast<quint16>(C) << 8) | D),
        static_cast<quint16>((static_cast<quint16>(A) << 8) | B)
    };
}

float floatFromCdab(quint16 first, quint16 second)
{
    const quint8 C = static_cast<quint8>((first >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(first & 0xFF);
    const quint8 A = static_cast<quint8>((second >> 8) & 0xFF);
    const quint8 B = static_cast<quint8>(second & 0xFF);

    const quint32 combined = (static_cast<quint32>(A) << 24)
                           | (static_cast<quint32>(B) << 16)
                           | (static_cast<quint32>(C) << 8)
                           | static_cast<quint32>(D);
    float result = 0.0f;
    std::memcpy(&result, &combined, sizeof(float));
    return result;
}

std::array<quint16, 4> doubleToGhefCdab(double value)
{
    quint64 raw = 0;
    std::memcpy(&raw, &value, sizeof(double));

    const quint8 A = static_cast<quint8>((raw >> 56) & 0xFF);
    const quint8 B = static_cast<quint8>((raw >> 48) & 0xFF);
    const quint8 C = static_cast<quint8>((raw >> 40) & 0xFF);
    const quint8 D = static_cast<quint8>((raw >> 32) & 0xFF);
    const quint8 E = static_cast<quint8>((raw >> 24) & 0xFF);
    const quint8 F = static_cast<quint8>((raw >> 16) & 0xFF);
    const quint8 G = static_cast<quint8>((raw >> 8) & 0xFF);
    const quint8 H = static_cast<quint8>(raw & 0xFF);

    return {
        static_cast<quint16>((static_cast<quint16>(G) << 8) | H),
        static_cast<quint16>((static_cast<quint16>(E) << 8) | F),
        static_cast<quint16>((static_cast<quint16>(C) << 8) | D),
        static_cast<quint16>((static_cast<quint16>(A) << 8) | B)
    };
}

double doubleFromGhefCdab(const std::array<quint16, 4> &regs)
{
    const quint8 G = static_cast<quint8>((regs[0] >> 8) & 0xFF);
    const quint8 H = static_cast<quint8>(regs[0] & 0xFF);
    const quint8 E = static_cast<quint8>((regs[1] >> 8) & 0xFF);
    const quint8 F = static_cast<quint8>(regs[1] & 0xFF);
    const quint8 C = static_cast<quint8>((regs[2] >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(regs[2] & 0xFF);
    const quint8 A = static_cast<quint8>((regs[3] >> 8) & 0xFF);
    const quint8 B = static_cast<quint8>(regs[3] & 0xFF);

    const quint64 raw = (static_cast<quint64>(A) << 56)
                      | (static_cast<quint64>(B) << 48)
                      | (static_cast<quint64>(C) << 40)
                      | (static_cast<quint64>(D) << 32)
                      | (static_cast<quint64>(E) << 24)
                      | (static_cast<quint64>(F) << 16)
                      | (static_cast<quint64>(G) << 8)
                      | static_cast<quint64>(H);
    double result = 0.0;
    std::memcpy(&result, &raw, sizeof(double));
    return result;
}

}
