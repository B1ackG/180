#ifndef MODBUSSTRINGREGISTERS_H
#define MODBUSSTRINGREGISTERS_H

#include <QtGlobal>
#include <QString>
#include <QVector>

/** Modbus 字符串寄存器默认长度（与 ModbusTCPAssistant 工具端一致） */
constexpr int kModbusUtf8StringRegisterCount = 15;

/**
 * @brief 将 Modbus 保持寄存器解码为 UTF-8 字符串（高字节在前，末尾 0x00 截断）。
 */
QString decodeUtf8FromRegisters(const quint16 *regs, int regCount);

QString decodeUtf8FromRegisters(const QVector<quint16> &regs);

/**
 * @brief 将 UTF-8 字符串编码为 Modbus 保持寄存器（不足补 0x00，超出截断）。
 */
void encodeUtf8ToRegisters(const QString &utf8, int regCount, quint16 *regs);

QVector<quint16> encodeUtf8ToRegisters(const QString &utf8, int regCount = kModbusUtf8StringRegisterCount);

#endif // MODBUSSTRINGREGISTERS_H
