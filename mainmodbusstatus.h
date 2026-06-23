#ifndef MAINMODBUSSTATUS_H
#define MAINMODBUSSTATUS_H

#include <QString>

class QStatusBar;
class OperationRecorder;

/**
 * @brief 主设备 Modbus 的状态枚举。
 */
enum class MainModbusState
{
    Connected,
    Disconnected,
    Error
};

/**
 * @brief 将主设备 Modbus 状态同步到界面和操作记录的辅助类。
 *
 * 该类负责把连接状态转换为状态栏文案、视觉样式以及操作记录，减少主窗口中
 * 重复的状态映射逻辑。
 */
class MainModbusStatus
{
public:
    /**
     * @brief 将状态应用到状态栏 UI。
     * @param statusBar 目标状态栏。
     * @param state 当前连接状态。
     * @param error 发生错误时附带的错误信息。
     *
     * 使用示例:
     * @code
     * MainModbusStatus::applyUiState(ui->statusbar, MainModbusState::Connected);
     * @endcode
     */
    static void applyUiState(QStatusBar *statusBar,
                             MainModbusState state,
                             const QString &error = QString());

    /**
     * @brief 返回状态栏 Modbus 指示器的样式表。
     * @param state 当前连接状态。
     */
    static QString indicatorStyle(MainModbusState state);

    /**
     * @brief 将状态写入操作记录器。
     * @param recorder 操作记录器实例。
     * @param state 当前连接状态。
     * @param error 发生错误时附带的错误信息。
     *
     * 使用示例:
     * @code
     * MainModbusStatus::appendOperationRecord(recorder, MainModbusState::Error, "timeout");
     * @endcode
     */
    static void appendOperationRecord(OperationRecorder *recorder,
                                      MainModbusState state,
                                      const QString &error = QString());
};

#endif // MAINMODBUSSTATUS_H
