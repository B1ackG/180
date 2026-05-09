#ifndef MODBUSWRITEGATE_H
#define MODBUSWRITEGATE_H

#include <QString>

class ModbusThreadManager;

/**
 * 主控 192.168.1.13 与 AGV 192.168.1.88 的写操作门禁：
 * 读取主控保持寄存器 interlockRegister()，仅当与配置的示教设备号一致时允许写入。
 * 写入主控 single-address interlockRegister() 豁免门禁（用于切换示教权限寄存器本身）。
 */
namespace ModbusWriteGate {

constexpr int interlockRegisterAddress() { return 8192; }

quint16 configuredTeachingDeviceId();

bool readInterlockRegister(ModbusThreadManager *main, quint16 &outValue);

bool verifyWriteAllowed(ModbusThreadManager *main);

QString deniedReason();

/** 主控单寄存器写是否在门禁上豁免 */
bool isExemptMainSingleWrite(int address);

/** 主控批量写是否整次豁免（仅当单次只写 interlock 寄存器） */
bool isExemptMainMultipleWrite(int startAddress, int registerCount);

bool allowMainDeviceSingleWrite(int address);

bool allowMainDeviceMultipleWrite(int startAddress, int registerCount);

bool allowAgvWrite(ModbusThreadManager *main);

/** 判断是否为主控写入失败信号中的「示教写门禁」文案（用于弹窗提示） */
bool messageIndicatesTeachingGateDenied(const QString &message);

/** 门禁弹窗展示给用户的固定提示语 */
QString teachingGateUserDialogMessage();

} // namespace ModbusWriteGate

#endif
