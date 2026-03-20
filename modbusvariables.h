#ifndef MODBUSVARIABLES_H
/**
 * @file modbusvariables.h
 * @brief Modbus 变量定义与封装声明，便于统一管理寄存器/变量映射。
 *
 * 详细说明: 用于声明 Modbus 协议中使用的变量、寄存器索引以及访问辅助函数。
 *
 * 使用示例:
 * @code
 * #include "modbusvariables.h"
 * ModbusVariables vars;
 * vars.setCoil(1, true);
 * @endcode
 */
#define MODBUSVARIABLES_H

#include <QObject>
#include <QMap>
#include <QString>

// 变量类型枚举
enum ModbusVarType {
    VAR_BOOL,    // 位变量
    VAR_INT,     // 整型变量
    VAR_UINT,    // 无符号整型
    VAR_REAL     // 浮点型
};

// 变量信息结构体
struct ModbusVariable {
    QString name;           // 变量名称
    QString address;        // PLC地址（如 %MX100.0）
    ModbusVarType type;     // 数据类型
    QString comment;        // 注释
    QString displayName;    // 显示名称
    int modbusAddress;      // Modbus地址（计算后的）
    int bitPosition;        // 位位置（仅对BOOL类型）
    int minValue;           // 最小值
    int maxValue;           // 最大值
    QString unit;           // 单位
};

class ModbusVariables : public QObject
{
    Q_OBJECT

public:
    /**
     * 功能: 构造 ModbusVariables 实例并初始化变量表。
     * 如何使用: 在程序启动或配置加载时创建该对象以管理变量映射。
     * 如何修改: 若需支持多源变量表，可在构造中注入数据源或添加加载接口。
     */
    explicit ModbusVariables(QObject *parent = nullptr);

    // 加载变量表
    /**
     * 功能: 从 Excel 文件中加载变量定义表并构建内部映射。
     * 如何使用: 提供 xlsx/csv 等已支持的文件路径，函数返回是否加载成功。
     * 如何修改: 若需支持其他格式（JSON/数据库），在实现中增加解析分支或抽象加载器。
     */
    bool loadFromExcel(const QString &filePath);

    /**
     * 使用示例:
     * @code
     * ModbusVariables vars;
     * if(vars.loadFromExcel("/path/to/vars.xlsx")) {
     *     auto v = vars.getVariable("%MX100.0");
     * }
     * @endcode
     */

    // 根据PLC地址获取变量信息
    /**
     * 功能: 根据 PLC 风格地址字符串返回对应的 ModbusVariable 结构。
     * 如何使用: 传入诸如 "%MX100.0" 的地址，并检查返回结构的有效性。
     * 如何修改: 若地址解析规则调整，同步修改 parseAddress 的实现。
     */
    ModbusVariable getVariable(const QString &plcAddress) const;

    /**
     * 使用示例:
     * @code
     * int addr, bit;
     * if(ModbusVariables::parseAddress("%MX100.0", addr, bit)) {
     *     // 使用 addr 和 bit
     * }
     * @endcode
     */

    // 获取所有变量
    /**
     * 功能: 返回所有已加载的变量列表（按键为 PLC 地址或变量名）。
     * 如何使用: 用于 UI 列表展示或导出完整变量表。
     * 如何修改: 若需分页或按条件筛选，提供带过滤参数的变体。
     */
    QList<ModbusVariable> getAllVariables() const;

    // 解析PLC地址为Modbus地址和位位置
    /**
     * 功能: 将 PLC 风格地址解析为 Modbus 数值地址与位位置（若为位变量）。
     * 如何使用: 用于将配置中的 PLC 地址转换为底层 Modbus 操作地址。
     * 如何修改: 若支持更多地址格式或厂商差异，在此扩展解析规则。
     */
    static bool parseAddress(const QString &plcAddress, int &modbusAddress, int &bitPos);

    // 根据变量值获取显示文本
    /**
     * 功能: 根据变量定义与原始寄存器值生成用户可读的显示文本（含单位/枚举映射）。
     * 如何使用: 在显示值到 UI 前调用以得到格式化文本。
     * 如何修改: 若需本地化或枚举映射，扩展该函数以使用外部映射表或翻译系统。
     */
    static QString getDisplayText(const ModbusVariable &var, quint16 value);

    /**
     * 使用示例:
     * @code
     * ModbusVariable mv = vars.getVariable("%MW200");
     * QString text = ModbusVariables::getDisplayText(mv, 123);
     * @endcode
     */

private:
    // 初始化发送变量表
    void initSendVariables();

    // 添加变量的重载函数
    void addVariable(const QString &name, const QString &address, ModbusVarType type, const QString &comment = "");
    void addVariable(const QString &name, const QString &address, ModbusVarType type, int minValue, int maxValue, const QString &unit = "", const QString &comment = "");

    /**
     * 使用示例:
     * @code
     * ModbusVariables vars;
     * vars.addVariable("battery", "%MW200", VAR_UINT, 0, 100, "%");
     * auto all = vars.getAllVariables();
     * @endcode
     */

private:
    QMap<QString, ModbusVariable> m_variables;
};

#endif // MODBUSVARIABLES_H
