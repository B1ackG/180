#include "featureswitchmanager.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

FeatureSwitchManager* FeatureSwitchManager::s_instance = nullptr;

// 单例实例指针的静态定义。
// 使用单例模式全局管理功能开关，确保程序中只有一个管理器实例。
FeatureSwitchManager* FeatureSwitchManager::instance()
{
    // 延迟初始化单例（lazy initialization）。
    if (!s_instance) {
        s_instance = new FeatureSwitchManager();
    }
    return s_instance;
}

FeatureSwitchManager::FeatureSwitchManager(QObject *parent)
    : QObject(parent)
{
    // 构造函数：初始化内部特性集合、确保配置文件存在并从配置加载状态。
    // 1) initializeFeatureSets() 初始化所有已知的大/小功能键集合
    // 2) ensureConfigExists() 确保在应用目录下生成默认的 feature_switches.ini（若不存在）
    // 3) reload() 从配置文件读取当前开关状态并在内存中建立禁用集合
    initializeFeatureSets();
    ensureConfigExists();
    reload();
}

void FeatureSwitchManager::initializeFeatureSets()
{
    // 初始化所有“大功能”键的列表。大功能用于粗粒度的功能开关，关闭大功能将影响该类别下的所有小功能。
    m_allBigFeatures = {
        "startup_checks",
        "ui_navigation",
        "permission_system",
        "operation_records",
        "tcp_transmission",
        "modbus_main",
        "modbus_agv",
        "motion_control",
        "input_devices",
        "alarm_system"
    };

    // 初始化所有“小功能”键的列表。小功能用于更细粒度的控制，依赖于对应的大功能开关。
    m_allSmallFeatures = {
        "startup.clear_servo_alarm",
        "startup.write_registers",
        "startup.log_report",

        "ui.styles",
        "ui.animations",
        "ui.virtual_keyboard",

        "permission.admin_login",

        "records.filter_export",

        "tcp.send_all",

        // 本机 TCP 模拟器模式：开启后将 Modbus 主设备/AGV 指向本机的模拟端口
        "tcp.local_simulator",

        // 远程 TCP 模拟器模式：开启后将 Modbus 主设备/AGV 指向模拟器电脑 (192.168.1.70)
        "tcp.remote_simulator",

        "modbus_main.polling",
        "modbus_main.float_reading",
        "modbus_main.read_enabled",
        "modbus_main.write_enabled",
        "modbus_main.read_logs",
        "modbus_main.write_logs",

        "modbus_agv.read_enabled",
        "modbus_agv.write_enabled",
        "modbus_agv.read_logs",
        "modbus_agv.write_logs",

        "agv.fault_codes",
        "agv.speed_gauge",

        "motion.steering_mode",
        "motion.speed_mode",
        "motion.step_mode",
        "motion.control_mode_switch",
        "motion.agv_oa_switch",
        "motion.agv_park_switch",
        "motion.agv_speed_control",
        "motion.agv_angle_control",

        "input.matrix_key",
        "input.enable_button",

        "alarm.emergency_stop",
        "alarm.steering_switch",
        "alarm.popup",
        "alarm.status_logs",

        // 全局 qDebug 输出开关
        "debug.qdebug"
    };
}

QString FeatureSwitchManager::configFilePath() const
{
    // 返回配置文件的完整路径。优先使用应用程序的运行目录（applicationDirPath），
    // 在某些测试场景下 QCoreApplication::instance() 可能为 nullptr，此时回退到当前工作目录。
    QString baseDir = QDir::currentPath();
    if (QCoreApplication::instance()) {
        baseDir = QCoreApplication::applicationDirPath();
    }

    return QDir(baseDir).absoluteFilePath("feature_switches.ini");
}

void FeatureSwitchManager::ensureConfigExists()
{
    // 确保配置文件存在；若不存在则创建默认配置并开启所有功能。
    // 这样首次运行时会生成一个可编辑的 INI 文件，方便用户修改开关。
    const QString path = configFilePath();
    if (QFileInfo::exists(path)) {
        return;
    }

    QSettings s(path, QSettings::IniFormat);
    // 全局主开关，默认开启
    s.setValue("Master/enabled", true);

    // 写入所有大功能节点，默认值为 true（开启）
    s.beginGroup("BigFeatures");
    for (const QString &key : m_allBigFeatures) {
        s.setValue(key, true);
    }
    s.endGroup();

    // 写入所有小功能节点，默认值为 true（开启）
    s.beginGroup("SmallFeatures");
    for (const QString &key : m_allSmallFeatures) {
        // 特殊处理：模拟器模式默认关闭
        if (key == "tcp.local_simulator" || key == "tcp.remote_simulator") {
            s.setValue(key, false);
        } else {
            s.setValue(key, true);
        }
    }
    s.endGroup();

    s.sync();
}

void FeatureSwitchManager::reload()
{
    // 从配置文件读取当前所有开关状态并在内存中建立“禁用集合”。
    // 保存的是被禁用的项集合（m_disabledBigFeatures / m_disabledSmallFeatures），
    // 这样查询时可以通过判断集合是否包含 key 来决定是否关闭功能。
    const QString path = configFilePath();
    QSettings s(path, QSettings::IniFormat);

    m_masterEnabled = s.value("Master/enabled", true).toBool();
    m_disabledBigFeatures.clear();
    m_disabledSmallFeatures.clear();

    s.beginGroup("BigFeatures");
    for (const QString &key : m_allBigFeatures) {
        // 如果配置中该条目为 false，则加入禁用集合
        if (!s.value(key, true).toBool()) {
            m_disabledBigFeatures.insert(key);
        }
    }
    s.endGroup();

    s.beginGroup("SmallFeatures");
    for (const QString &key : m_allSmallFeatures) {
        if (!s.value(key, true).toBool()) {
            m_disabledSmallFeatures.insert(key);
        }
    }
    s.endGroup();
}

void FeatureSwitchManager::save()
{
    // 将当前内存中的状态写回配置文件。注意我们在内存中保存的是“被禁用”的集合，
    // 写回时要把布尔值取反（存在于禁用集合 -> 写 false；否则写 true）。
    const QString path = configFilePath();
    QSettings s(path, QSettings::IniFormat);

    s.setValue("Master/enabled", m_masterEnabled);

    s.beginGroup("BigFeatures");
    for (const QString &key : m_allBigFeatures) {
        s.setValue(key, !m_disabledBigFeatures.contains(key));
    }
    s.endGroup();

    s.beginGroup("SmallFeatures");
    for (const QString &key : m_allSmallFeatures) {
        s.setValue(key, !m_disabledSmallFeatures.contains(key));
    }
    s.endGroup();

    s.sync();
}

bool FeatureSwitchManager::masterEnabled() const
{
    return m_masterEnabled;
}

bool FeatureSwitchManager::isBigFeatureEnabled(const QString &bigKey) const
{
    // 首先检查主开关，如果主开关关闭则所有功能都不可用。
    if (!m_masterEnabled) {
        return false;
    }

    // 如果传入的 key 并不在已知的大功能集合中，默认认为该功能可用（避免阻塞未知/新增功能）。
    if (!m_allBigFeatures.contains(bigKey)) {
        return true;
    }

    // 返回该大功能是否未被标记为禁用。
    return !m_disabledBigFeatures.contains(bigKey);
}

bool FeatureSwitchManager::isSmallFeatureEnabled(const QString &smallKey) const
{
    // 同上：检查主开关
    if (!m_masterEnabled) {
        return false;
    }

    // 若该小功能不是已知列表的一部分，则默认允许（不阻塞未知项）。
    if (!m_allSmallFeatures.contains(smallKey)) {
        return true;
    }

    return !m_disabledSmallFeatures.contains(smallKey);
}

bool FeatureSwitchManager::isFeatureEnabled(const QString &bigKey, const QString &smallKey) const
{
    // 组合查询：先检查所属大功能，若大功能被禁用则直接返回 false。
    if (!isBigFeatureEnabled(bigKey)) {
        return false;
    }

    // 如果没有提供小功能 key，则仅依赖大功能的开关状态。
    if (smallKey.isEmpty()) {
        return true;
    }

    // 否则还需要检查小功能的开关状态。
    return isSmallFeatureEnabled(smallKey);
}

void FeatureSwitchManager::setMasterEnabled(bool enabled)
{
    m_masterEnabled = enabled;
}

void FeatureSwitchManager::setBigFeatureEnabled(const QString &bigKey, bool enabled)
{
    // 只对已知的大功能键生效；未知键忽略以避免误操作。
    if (!m_allBigFeatures.contains(bigKey)) {
        return;
    }

    if (enabled) {
        // 从禁用集合中移除表示启用
        m_disabledBigFeatures.remove(bigKey);
    } else {
        // 插入表示禁用
        m_disabledBigFeatures.insert(bigKey);
    }
}

void FeatureSwitchManager::setSmallFeatureEnabled(const QString &smallKey, bool enabled)
{
    // 与大功能同理：只有已知小功能才会被改变状态。
    if (!m_allSmallFeatures.contains(smallKey)) {
        return;
    }

    if (enabled) {
        m_disabledSmallFeatures.remove(smallKey);
    } else {
        m_disabledSmallFeatures.insert(smallKey);
    }
}

void FeatureSwitchManager::setAllEnabled(bool enabled)
{
    // 全局一键开/关。开启时清空禁用集合，关闭时将所有已知功能加入禁用集合。
    m_masterEnabled = enabled;
    if (enabled) {
        m_disabledBigFeatures.clear();
        m_disabledSmallFeatures.clear();
    } else {
        m_disabledBigFeatures = m_allBigFeatures;
        m_disabledSmallFeatures = m_allSmallFeatures;
    }
}

void FeatureSwitchManager::setAllBigEnabled(bool enabled)
{
    // 一键设置所有“大功能”的开启/关闭，但不影响小功能集合的内容（仅改变大类状态）。
    if (enabled) {
        m_disabledBigFeatures.clear();
    } else {
        m_disabledBigFeatures = m_allBigFeatures;
    }
}

void FeatureSwitchManager::setAllSmallEnabled(bool enabled)
{
    // 一键设置所有“小功能”的开启/关闭。
    if (enabled) {
        m_disabledSmallFeatures.clear();
    } else {
        m_disabledSmallFeatures = m_allSmallFeatures;
    }
}
