#ifndef TECHSLIDERMANAGER_H
/**
 * @file techslidermanager.h
 * @brief 滑动控件管理器的声明，负责管理多个滑块控件的状态与交互。
 *
 * 详细说明: 该文件声明用于协调和管理 `TechSlider` 系列控件的类或工具函数。
 *
 * 使用示例:
 * @code
 * #include "techslidermanager.h"
 * TechSliderManager mgr;
 * mgr.registerSlider(slider);
 * @endcode
 */
#define TECHSLIDERMANAGER_H

#include <QObject>
#include <QLabel>
#include <QHash>
#include <QWidget>
#include <QMap>

struct TechSliderConfig {
    int modbusAddress;
    QString displayText;
    QString unit;
    QString styleSheet;
    int minWidth;
    int minHeight;

    TechSliderConfig() : modbusAddress(0), minWidth(120), minHeight(40) {}
    TechSliderConfig(int addr, const QString& text, const QString& u = "",
                     const QString& style = "", int w = 120, int h = 40)
        : modbusAddress(addr), displayText(text), unit(u),
        styleSheet(style), minWidth(w), minHeight(h) {}
};

class TechSliderManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造 TechSliderManager
     *
     * 管理页面中多个 `TechSliderLabel` / `TechSliderEdit` 的配置、缓存与批量更新。
     * 典型用法是从配置文件中加载 `TechSliderConfig`，然后调用 `setupPageUI()`
     * 在目标页面创建并缓存标签对象，随后通过 `updateValues()` 批量刷新显示。
     *
     * @param parent 父对象（一般传 `qApp` 或主窗口指针）。
     * @since 1.0.0
     *
     * @note 本管理器维护标签对象指针缓存（`m_labelCache`），请在页面关闭时
     *       清理或确保页面生命周期与本管理器相匹配以避免悬挂指针。
     */
    explicit TechSliderManager(QObject *parent = nullptr);

    /**
     * 使用示例:
     * @code
     * TechSliderManager *mgr = new TechSliderManager(qApp);
     * TechSliderConfig cfg(1001, "Battery", "%");
     * mgr->addConfig("mainPage", "battery", cfg);
     * mgr->setupPageUI("mainPage");
     * // 批量更新来自 Modbus 的数据
     * QMap<int,float> data; data[1001] = 78.5f;
     * mgr->updateValues(data);
     * @endcode
     */

    /**
     * @brief 初始化默认配置
     *
     * 加载或创建默认 `TechSliderConfig`，不修改已存在的页面实例。
     */
    void initializeConfig();

    /**
     * @brief 为指定页面创建/设置 UI
     *
     * 根据 `m_configs` 中的配置，为 `pageName` 页面创建标签控件并缓存。
     *
     * @param pageName 页面标识字符串。
     * @note 需要在 UI 布局存在父容器时调用，否则标签不会被正确添加到界面。
     */
    void setupPageUI(const QString& pageName);

    /**
     * @brief 为所有已注册页面创建 UI
     */
    void setupAllUI();

    /**
     * @brief 批量更新值（通常来自 Modbus）
     *
     * 将 `modbusData` 中的地址-数值映射应用到缓存的标签控件。
     *
     * @param modbusData 键为地址(int)，值为浮点数表示的读取值。
     * @note 函数内部会根据配置匹配 address 并调用对应控件的更新接口。
     */
    void updateValues(const QMap<int, float>& modbusData);

    /**
     * 使用示例:
     * @code
     * QMap<int,float> data; data[1001] = 78.5f;
     * mgr->updateValues(data);
     * @endcode
     */

    /**
     * @brief 更新单个 label 的值
     *
     * @param pageName 页面名
     * @param labelName 标签名称
     * @param value 要设置的数值
     * @note 若找不到对应的标签，则函数静默返回并在调试模式打印信息。
     */
    void updateLabelValue(const QString& pageName, const QString& labelName, float value);

    /**
     * 使用示例:
     * @code
     * mgr->updateLabelValue("mainPage", "battery", 55.0f);
     * @endcode
     */

    /**
     * @brief 添加或更新某页面的滑块配置
     *
     * @param pageName 页面名
     * @param labelName 标签名
     * @param config 配置项（地址、文本、单位、样式等）
     * @note 调用后若页面已创建 UI，需要调用 `setupPageUI(pageName)` 或特定刷新逻辑。
     */
    void addConfig(const QString& pageName, const QString& labelName,
                   const TechSliderConfig& config);

    /**
     * 使用示例:
     * @code
     * TechSliderConfig cfg(1001, "Battery", "%");
     * mgr->addConfig("mainPage", "battery", cfg);
     * @endcode
     */

private:
    // 设置单个label的UI
    void setupSingleLabelUI(QLabel* label, const TechSliderConfig& config);

    // 查找label（带缓存）
    QLabel* findTechSliderLabel(const QString& pageName, const QString& labelName);

    // 查找所有label并缓存
    void cacheAllLabels(QWidget* parent);

private:
    // 配置存储: 页面名 -> (label名 -> 配置)
    QHash<QString, QHash<QString, TechSliderConfig>> m_configs;

    // label对象缓存: 页面名 -> (label名 -> QLabel指针)
    QHash<QString, QHash<QString, QLabel*>> m_labelCache;

    // 父窗口（主界面）
    QWidget* m_parentWidget;

    // 默认样式
    QString m_defaultStyle;
};

#endif // TECHSLIDERMANAGER_H
