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

int interlockRegisterAddress();
void setInterlockRegisterAddress(int address);

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

/**
 * 由 UI 轮询（如联锁按钮刷新）在每次读取 8192 后调用，供操作历史是否落库判断。
 * readOk 为 false 时表示未读到有效值，此时不暂停历史记录。
 */
void updateOperationHistoryGateFromInterlockRead(bool readOk, quint16 register8192Value);

/** 是否应将当前操作写入历史：已读到 8192 且与示教设备号不一致时为 false */
bool shouldAppendOperationHistoryRecord();

} // namespace ModbusWriteGate

#endif
