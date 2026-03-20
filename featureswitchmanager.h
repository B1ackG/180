#ifndef FEATURESWITCHMANAGER_H
#define FEATURESWITCHMANAGER_H

#include <QObject>
#include <QSet>
#include <QString>

/**
 * @brief 功能开关管理器
 *
 * 使用单例模式管理应用的功能开关（分为“大功能”和“小功能”两层）。
 * - 大功能（BigFeatures）：用于粗粒度的功能分组，关闭后会影响该分组下的所有小功能。
 * - 小功能（SmallFeatures）：用于细粒度控制，通常依赖于对应的大功能必须开启。
 *
 * 配置通过应用目录下的 `feature_switches.ini`（INI 格式）持久化，类提供 `reload()` 和 `save()`
 * 接口用于从文件重新加载/写回当前状态。
 *
 * 设计要点：
 * - 内存中保存的是“被禁用项”的集合（m_disabledBigFeatures / m_disabledSmallFeatures），
 *   查询时通过判断集合是否包含 key 来决定是否关闭功能；这样更方便实现“一键全关/全开”。
 * - 对于未在已知列表中的 key，查询时默认返回允许（避免阻塞新增或未列出的功能）。
 *
 * 示例：
 * @code{.cpp}
 * // 获取单例并检查/修改开关
 * FeatureSwitchManager *m = FeatureSwitchManager::instance();
 * // 检查主开关
 * bool master = m->masterEnabled();
 * // 检查某个大功能和小功能
 * if (m->isFeatureEnabled("modbus_main", "modbus_main.polling")) {
 *     // 启动 modbus 轮询
 * }
 * // 一键全关并保存
 * m->setAllEnabled(false);
 * m->save();
 * // 恢复并重新加载配置
 * m->reload();
 * @endcode
 */
class FeatureSwitchManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 全局唯一的 FeatureSwitchManager 指针
     */
    static FeatureSwitchManager* instance();

    /** @brief 主开关，若为 false 则所有功能均不可用 */
    bool masterEnabled() const;

    /**
     * @brief 检查某个大功能是否可用
     * @param bigKey 大功能的键名称（如 "modbus_main"）
     * @return 若主开关为 false 返回 false；若 bigKey 未知则返回 true；否则根据配置返回状态
     */
    bool isBigFeatureEnabled(const QString &bigKey) const;

    /**
     * @brief 检查某个小功能是否可用
     * @param smallKey 小功能键名称（如 "modbus_main.polling"）
     * @return 行为与 isBigFeatureEnabled 类似，但针对小功能集合进行判断
     */
    bool isSmallFeatureEnabled(const QString &smallKey) const;

    /**
     * @brief 组合检查：先检查所属大功能，再检查小功能（若提供）
     * @param bigKey 大功能键
     * @param smallKey 可选的小功能键（默认为空，仅检查大功能）
     */
    bool isFeatureEnabled(const QString &bigKey, const QString &smallKey = QString()) const;

    /** @brief 设置主开关（仅修改内存状态，需要调用 save() 持久化） */
    void setMasterEnabled(bool enabled);

    /** @brief 设置指定大功能是否启用（仅影响内存中的禁用集合） */
    void setBigFeatureEnabled(const QString &bigKey, bool enabled);

    /** @brief 设置指定小功能是否启用（仅影响内存中的禁用集合） */
    void setSmallFeatureEnabled(const QString &smallKey, bool enabled);

    /** @brief 一键设置所有功能的开启/关闭（修改内存状态） */
    void setAllEnabled(bool enabled);

    /** @brief 一键设置所有大功能的开启/关闭 */
    void setAllBigEnabled(bool enabled);

    /** @brief 一键设置所有小功能的开启/关闭 */
    void setAllSmallEnabled(bool enabled);

    /** @brief 返回当前用于持久化的配置文件路径 */
    QString configFilePath() const;

    /** @brief 从配置文件重新加载开关状态（覆盖内存状态） */
    void reload();

    /** @brief 将当前内存状态保存到配置文件 */
    void save();

    /** @brief 获取所有已知的大功能键集合（只读引用） */
    const QSet<QString>& allBigFeatures() const { return m_allBigFeatures; }

    /** @brief 获取所有已知的小功能键集合（只读引用） */
    const QSet<QString>& allSmallFeatures() const { return m_allSmallFeatures; }

private:
    explicit FeatureSwitchManager(QObject *parent = nullptr);

    /** @brief 初始化已知的大/小功能键集合（内置默认项） */
    void initializeFeatureSets();

    /** @brief 如果配置文件不存在，创建一个包含所有功能默认开启的 INI 文件 */
    void ensureConfigExists();

private:
    static FeatureSwitchManager *s_instance;

    bool m_masterEnabled = true;
    QSet<QString> m_allBigFeatures;
    QSet<QString> m_allSmallFeatures;
    // 内存中的“禁用集合”：若某键存在于这些集合中表示已被禁用
    QSet<QString> m_disabledBigFeatures;
    QSet<QString> m_disabledSmallFeatures;
};

#endif // FEATURESWITCHMANAGER_H
