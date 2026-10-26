#include "modbusbackendmode.h"

#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QDebug>

namespace {

QMutex g_processModeMutex;
bool g_processModeCached = false;
ModbusBackendMode g_processMode = ModbusBackendMode::Dynamic;
bool g_invalidModeWarned = false;

void warnInvalidModeOnce(const QString &raw)
{
    if (g_invalidModeWarned) {
        return;
    }
    g_invalidModeWarned = true;
    qWarning() << "Modbus 后端模式配置无效，回退为 dynamic:" << raw;
}

} // namespace

ModbusBackendMode parseModbusBackendMode(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("static")) {
        return ModbusBackendMode::Static;
    }
    return ModbusBackendMode::Dynamic;
}

QString modbusBackendModeValue(ModbusBackendMode mode)
{
    return mode == ModbusBackendMode::Static
               ? QStringLiteral("static")
               : QStringLiteral("dynamic");
}

ModbusBackendMode loadModbusBackendMode(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ModbusBackend"));
    const QString raw = settings.value(QStringLiteral("mode"), QStringLiteral("dynamic")).toString();
    settings.endGroup();

    const QString normalized = raw.trimmed().toLower();
    if (normalized.isEmpty()
        || (normalized != QLatin1String("dynamic") && normalized != QLatin1String("static"))) {
        warnInvalidModeOnce(raw);
    }
    return parseModbusBackendMode(raw);
}

bool saveModbusBackendMode(ModbusBackendMode mode, const QString &filePath, QString *error)
{
    if (error) {
        error->clear();
    }

    const QFileInfo info(filePath);
    if (info.exists() && info.isDir()) {
        if (error) {
            *error = QStringLiteral("无法写入配置：路径是目录");
        }
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ModbusBackend"));
    settings.setValue(QStringLiteral("mode"), modbusBackendModeValue(mode));
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("无法写入配置文件");
        }
        return false;
    }
    return true;
}

ModbusBackendMode processModbusBackendMode()
{
    QMutexLocker locker(&g_processModeMutex);
    if (!g_processModeCached) {
        g_processMode = loadModbusBackendMode();
        g_processModeCached = true;
    }
    return g_processMode;
}

void resetProcessModbusBackendModeCache()
{
    QMutexLocker locker(&g_processModeMutex);
    g_processModeCached = false;
}
