#ifndef MODBUSBACKENDMODE_H
#define MODBUSBACKENDMODE_H

#include <QString>

enum class ModbusBackendMode {
    Dynamic,
    Static
};

ModbusBackendMode parseModbusBackendMode(const QString &value);
QString modbusBackendModeValue(ModbusBackendMode mode);
ModbusBackendMode loadModbusBackendMode(const QString &filePath = QStringLiteral("config.ini"));
bool saveModbusBackendMode(ModbusBackendMode mode,
                           const QString &filePath = QStringLiteral("config.ini"),
                           QString *error = nullptr);

/** 进程内只读一次磁盘配置，主控与 AGV 共用。 */
ModbusBackendMode processModbusBackendMode();
/** 仅测试使用：允许下一条 process 读取重新看磁盘。 */
void resetProcessModbusBackendModeCache();

#endif
