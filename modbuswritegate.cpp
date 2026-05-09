#include "modbuswritegate.h"
#include "modbusthreadmanager.h"

#include <QSettings>

namespace ModbusWriteGate {

quint16 configuredTeachingDeviceId()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Polling"));
    const QVariant v = settings.value(QStringLiteral("teaching_write_device_id"), 0);
    settings.endGroup();
    bool ok = false;
    const int n = v.toInt(&ok);
    if (!ok || n < 0) {
        return 0;
    }
    if (n > 65535) {
        return 65535;
    }
    return static_cast<quint16>(n);
}

bool readInterlockRegister(ModbusThreadManager *main, quint16 &outValue)
{
    if (!main || !main->isConnected()) {
        return false;
    }
    return main->readSingleRegister(interlockRegisterAddress(), outValue);
}

bool verifyWriteAllowed(ModbusThreadManager *main)
{
    quint16 cur = 0;
    if (!readInterlockRegister(main, cur)) {
        return false;
    }
    return cur == configuredTeachingDeviceId();
}

QString deniedReason()
{
    return QStringLiteral("示教写权限：主控寄存器%1与本机配置设备号不一致，已阻止写操作")
        .arg(interlockRegisterAddress());
}

bool isExemptMainSingleWrite(int address)
{
    return address == interlockRegisterAddress();
}

bool isExemptMainMultipleWrite(int startAddress, int registerCount)
{
    return registerCount == 1 && startAddress == interlockRegisterAddress();
}

bool allowMainDeviceSingleWrite(int address)
{
    if (isExemptMainSingleWrite(address)) {
        return true;
    }
    return verifyWriteAllowed(ModbusThreadManager::instance());
}

bool allowMainDeviceMultipleWrite(int startAddress, int registerCount)
{
    if (isExemptMainMultipleWrite(startAddress, registerCount)) {
        return true;
    }
    return verifyWriteAllowed(ModbusThreadManager::instance());
}

bool allowAgvWrite(ModbusThreadManager *main)
{
    return verifyWriteAllowed(main ? main : ModbusThreadManager::instance());
}

bool messageIndicatesTeachingGateDenied(const QString &message)
{
    return message.startsWith(QStringLiteral("示教写权限："));
}

QString teachingGateUserDialogMessage()
{
    return QStringLiteral("示教器控制互锁，请切换到当前示教器。");
}

} // namespace ModbusWriteGate
