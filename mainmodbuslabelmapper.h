#ifndef MAINMODBUSLABELMAPPER_H
#define MAINMODBUSLABELMAPPER_H

#include <QMap>

class QLabel;
class QWidget;

/**
 * @brief 主设备 Modbus 标签映射工具。
 *
 * 遍历界面树中的标签对象，并根据对象名解析出对应的寄存器键值，
 * 便于后续轮询结果直接更新到界面上。
 */
class MainModbusLabelMapper
{
public:
    /**
     * @brief 扫描根控件并构建寄存器到 QLabel 的映射表。
     * @param rootWidget 根控件，通常为主窗口或页面容器。
     * @return 映射表：
     * - `MWxxxx` 标签使用寄存器地址作为 key
     * - `MXxxxx_x` 标签使用 `address * 1000 + bitPos` 作为 key
     *
     * 使用示例:
     * @code
     * auto labelMap = MainModbusLabelMapper::buildMap(this);
     * @endcode
     */
    static QMap<int, QLabel *> buildMap(QWidget *rootWidget);
};

#endif // MAINMODBUSLABELMAPPER_H
