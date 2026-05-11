#ifndef MAPPINGCONFIG_H
/**
 * @file mappingconfig.h
 * @brief 映射配置相关的声明，用于键位/动作与内部控制信号的映射定义。
 *
 * 详细说明: 包含读取、保存、应用映射配置的类或结构体声明，便于自定义按键与动作绑定。
 *
 * 使用示例:
 * @code
 * #include "mappingconfig.h"
 * MappingConfig cfg;
 * cfg.loadFromFile("map.json");
 * @endcode
 */
#define MAPPINGCONFIG_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariant>

struct OperationRecord;

class MappingConfig : public QObject
{
    Q_OBJECT
public:
    /**
     * 功能: 构造函数，初始化映射表。
     * 如何使用: 通常通过单例 instance() 获取对象，不直接 new（但也支持显式创建）。
     * 如何修改: 若需从文件或外部资源加载映射，可在构造中或单独初始化方法中添加加载逻辑。
     */
    explicit MappingConfig(QObject *parent = nullptr);

    /**
     * 功能: 获取单例实例（线程非严格安全，按需改造）。
     * 如何使用: 调用 MappingConfig::instance() 获取全局映射配置对象。
     * 如何修改: 若需要线程安全或延迟初始化，可改用 QScopedPointer +互斥锁/静态局部变量。
     */
    static MappingConfig* instance();

    /**
     * 使用示例:
     * @code
     * MappingConfig *cfg = MappingConfig::instance();
     * QString name = cfg->mapControlName("BtnStart");
     * @endcode
     */

    /**
     * 功能: 将控件对象名映射为用户可读显示名。
     * 如何使用: 在显示 UI 文本时调用，例如页面上显示控件标签。
     * 如何修改: 若要支持正则或更复杂的映射规则，可在实现中增加匹配逻辑并保留回退值。
     */
    QString mapControlName(const QString &objectName) const;

    /**
     * 功能: 将内部操作标识映射为显示文本（例如 "start" -> "启动"）。
     * 如何使用: 在操作记录或提示文本生成时调用。
     * 如何修改: 可添加多语言支持，改为根据当前语言选择不同映射表。
     */
    QString mapOperation(const QString &operation) const;

    /**
     * 功能: 将控件类型映射为显示文本（例如 "QPushButton" -> "按钮"）。
     * 如何使用: 用于生成动态 UI 描述或日志信息。
     * 如何修改: 若增加自定义控件类型，调用 addControlTypeMapping 注册新类型。
     */
    QString mapControlType(const QString &controlType) const;

    /**
     * 功能: 将页面键（索引或原始名）映射为显示名称。
     * 如何使用: 在导航条或页面标题显示时调用。
     * 如何修改: 可扩展为支持分组或多层次页面映射。
     */
    QString mapPageName(const QString &pageKey) const;

    /**
     * 功能: 将输入值映射为显示文本（例如 "true" -> "是", "Host unreachable" -> "主机不可达"）。
     * 如何使用: 用于将底层状态或网络错误翻译。
     */
    QString mapValue(const QString &value) const;

    /**
     * 功能: 将一条操作记录中的页面名、控件名、操作类型、控件类型及布尔/常用值统一映射为可读文案。
     * 如何使用: 在写入记录列表、导出或发往 UI 前调用（由 OperationRecorder 集中调用）。
     */
    void normalizeOperationRecord(OperationRecord &record) const;

    /**
     * 功能: 添加或覆盖控件名称映射。
     * 如何使用: 在运行时或配置加载时调用以自定义显示名称。
     * 如何修改: 直接修改 m_controlNameMap 或提供批量加载接口。
     */
    void addControlMapping(const QString &objectName, const QString &displayName);

    /**
     * 功能: 添加或覆盖操作类型映射。
     * 如何使用: 在初始化时注册自定义操作到显示文本的映射。
     * 如何修改: 支持覆盖或按优先级合并多个映射源。
     */
    void addOperationMapping(const QString &operation, const QString &displayText);

    /**
     * 功能: 添加或覆盖控件类型映射。
     * 如何使用: 注册新控件类型的显示文本以便前端显示友好名称。
     * 如何修改: 若控件类型识别规则改变，调整实现并同步映射数据结构。
     */
    void addControlTypeMapping(const QString &controlType, const QString &displayText);

    /**
     * 功能: 初始化默认映射（内置键值对）。
     * 如何使用: 在应用启动时调用以填充默认映射表，随后可被 add* 方法覆盖。
     * 如何修改: 将默认映射提取到配置文件以便维护，或添加多语言支持。
     */
    void initDefaultMappings();

private:
    // 使用QHash存储映射关系，查找效率高
    QHash<QString, QString> m_controlNameMap;    // 控件名映射
    QHash<QString, QString> m_operationMap;      // 操作类型映射
    QHash<QString, QString> m_controlTypeMap;    // 控件类型映射
    QHash<QString, QString> m_pageNameMap;       // 页面名称映射
    QHash<QString, QString> m_valueMap;          // 值映射

    QVariant mapVariantForDisplay(const QVariant &value) const;

    static MappingConfig* s_instance;  // 单例实例
};
#endif // MAPPINGCONFIG_H
