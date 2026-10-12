#ifndef MODBUSASYNC_H
#define MODBUSASYNC_H

#include <QMetaType>
#include <QVector>
#include <QtGlobal>

enum class ModbusAsyncError : quint8 {
    None = 0,
    NotConnected,
    IoFailed,
    StaleGeneration,
    GateDenied,
    ReadDisabled,
    WriteDisabled,
    Timeout,
    Rejected
};

struct ModbusReadRequest {
    quint64 id = 0;
    int start = 0;
    int count = 0;
    quint64 generation = 0;
};

struct ModbusWriteCommand {
    quint64 id = 0;
    int start = 0;
    QVector<quint16> values;
    quint64 generation = 0;
    int generationChannel = 0;
};

struct ModbusResult {
    quint64 id = 0;
    bool ok = false;
    ModbusAsyncError error = ModbusAsyncError::None;
    int start = 0;
    QVector<quint16> values;
    quint64 generation = 0;
};

struct GateDecision {
    quint64 commandId = 0;
    bool allowed = false;
    ModbusAsyncError error = ModbusAsyncError::None;
};

Q_DECLARE_METATYPE(ModbusReadRequest)
Q_DECLARE_METATYPE(ModbusWriteCommand)
Q_DECLARE_METATYPE(ModbusResult)
Q_DECLARE_METATYPE(GateDecision)

#endif
