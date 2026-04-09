#include "modbusvariables.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

ModbusVariables::ModbusVariables(QObject *parent) : QObject(parent)
{
    // 初始化发送上位机变量表（硬编码，实际可以从Excel加载）
    initSendVariables();
}

void ModbusVariables::addVariable(const QString &name, const QString &address, ModbusVarType type, const QString &comment)
{
    ModbusVariable var;
    var.name = name;
    var.address = address;
    var.type = type;
    var.comment = comment;
    var.displayName = name;

    // 解析地址
    int modbusAddress = 0;
    int bitPos = -1;
    if (parseAddress(address, modbusAddress, bitPos)) {
        var.modbusAddress = modbusAddress;
        var.bitPosition = bitPos;
    } else {
        qWarning() << "无法解析地址:" << address;
        return;
    }

    // 默认值
    var.minValue = 0;
    var.maxValue = 0;

    m_variables[address] = var;
}

void ModbusVariables::addVariable(const QString &name, const QString &address, ModbusVarType type, int minValue, int maxValue, const QString &unit, const QString &comment)
{
    ModbusVariable var;
    var.name = name;
    var.address = address;
    var.type = type;
    var.comment = comment;
    var.displayName = name;
    var.minValue = minValue;
    var.maxValue = maxValue;
    var.unit = unit;

    // 解析地址
    int modbusAddress = 0;
    int bitPos = -1;
    if (parseAddress(address, modbusAddress, bitPos)) {
        var.modbusAddress = modbusAddress;
        var.bitPosition = bitPos;
    } else {
        qWarning() << "无法解析地址:" << address;
        return;
    }

    m_variables[address] = var;
}

void ModbusVariables::initSendVariables()
{
    // 发送上位机变量表
    // 根据Excel表初始化

    // %MX100.x 系列 - 位变量
    addVariable("发送上位机心跳", "%MX100.0", VAR_BOOL, "1秒1次脉冲");
    addVariable("发送上位机前触边", "%MX100.1", VAR_BOOL, "1为触发");
    addVariable("发送上位机后触边", "%MX100.2", VAR_BOOL, "1为触发");
    addVariable("发送上位机左触边", "%MX100.3", VAR_BOOL, "1为触发");
    addVariable("发送上位机右触边", "%MX100.4", VAR_BOOL, "1为触发");
    addVariable("发送上位机前避障减速", "%MX100.5", VAR_BOOL, "1为触发");
    addVariable("发送上位机前避障停止", "%MX100.6", VAR_BOOL, "1为触发");
    addVariable("发送上位机后避障减速", "%MX100.7", VAR_BOOL, "1为触发");
    addVariable("发送上位机后避障停止", "%MX101.0", VAR_BOOL, "1为触发");
    addVariable("发送上位机点动运行中", "%MX101.1", VAR_BOOL, "1为触发");
    addVariable("发送上位机转向轮回正", "%MX101.2", VAR_BOOL, "1为触发");
    addVariable("发送上位机横移模式转向轮到位", "%MX101.3", VAR_BOOL, "1为触发");
    addVariable("发送上位机原地旋转模式转向轮到位", "%MX101.4", VAR_BOOL, "1为触发");
    addVariable("发送上位机低电报警", "%MX102.0", VAR_BOOL, "1为触发");
    addVariable("发送上位机通讯故障", "%MX102.1", VAR_BOOL, "1为触发");
    addVariable("发送上位机驱动故障", "%MX102.2", VAR_BOOL, "1为触发");

    // %MW 系列 - 字变量
    addVariable("发送上位机电池1电量", "%MW102", VAR_INT, 0, 100, "%", "0-100%");
    addVariable("发送上位机电池2电量", "%MW103", VAR_INT, 0, 100, "%", "0-100%");
    addVariable("发送上位机行驶速度", "%MW104", VAR_INT, 0, 50000, "mm/min", "0-3Km/h");
    addVariable("发送上位机点动位移", "%MW105", VAR_INT, -1000, 1000, "mm", "-1000 ~ +1000 mm");

    // 故障代码系列 - 字变量，没有范围限制
    addVariable("发送上位机转向1故障代码", "%MW110", VAR_INT);
    addVariable("发送上位机转向2故障代码", "%MW111", VAR_INT);
    addVariable("发送上位机转向3故障代码", "%MW112", VAR_INT);
    addVariable("发送上位机转向4故障代码", "%MW113", VAR_INT);
    addVariable("发送上位机行走1故障代码", "%MW114", VAR_INT);
    addVariable("发送上位机行走2故障代码", "%MW115", VAR_INT);
    addVariable("发送上位机行走3故障代码", "%MW116", VAR_INT);
    addVariable("发送上位机行走4故障代码", "%MW117", VAR_INT);

    // 新增：力控报警监控
    addVariable("力控超限监控", "%MW403", VAR_INT, "0为正常，1为报警");
}

bool ModbusVariables::parseAddress(const QString &plcAddress, int &modbusAddress, int &bitPos)
{
    // 解析类似 %MX100.0 或 %MW102 的地址
    if (plcAddress.startsWith("%MX")) {
        // 位变量
        QStringList parts = plcAddress.mid(3).split('.');
        if (parts.size() == 2) {
            modbusAddress = parts[0].toInt();
            bitPos = parts[1].toInt();
            return true;
        }
    } else if (plcAddress.startsWith("%MW")) {
        // 字变量
        modbusAddress = plcAddress.mid(3).toInt();
        bitPos = -1;  // 字变量没有位位置
        return true;
    }

    return false;
}

QString ModbusVariables::getDisplayText(const ModbusVariable &var, quint16 value)
{
    switch (var.type) {
    case VAR_BOOL:
        // 对于位变量，我们假设传入的value已经是该位的值（0或1）
        return value ? "触发" : "未触发";

    case VAR_INT:
        if (!var.unit.isEmpty()) {
            return QString("%1 %2").arg(static_cast<qint16>(value)).arg(var.unit);
        }
        return QString::number(static_cast<qint16>(value));

    case VAR_UINT:
        if (!var.unit.isEmpty()) {
            return QString("%1 %2").arg(value).arg(var.unit);
        }
        return QString::number(value);

    default:
        return QString::number(value);
    }
}

bool ModbusVariables::loadFromExcel(const QString &filePath)
{
    // TODO: 实现从Excel文件加载变量表
    Q_UNUSED(filePath);
    return false;
}

ModbusVariable ModbusVariables::getVariable(const QString &plcAddress) const
{
    if (m_variables.contains(plcAddress)) {
        return m_variables[plcAddress];
    }
    return ModbusVariable();
}

QList<ModbusVariable> ModbusVariables::getAllVariables() const
{
    return m_variables.values();
}
