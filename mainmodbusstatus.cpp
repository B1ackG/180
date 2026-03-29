#include "mainmodbusstatus.h"

#include <QDateTime>
#include <QLabel>
#include <QStatusBar>

#include "operationrecorder.h"

namespace {
QString stateToOperation(MainModbusState state)
{
    switch (state) {
    case MainModbusState::Connected:
        return QStringLiteral("connected");
    case MainModbusState::Disconnected:
        return QStringLiteral("disconnected");
    case MainModbusState::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("unknown");
}

QString stateToNewValue(MainModbusState state, const QString &error)
{
    switch (state) {
    case MainModbusState::Connected:
        return QStringLiteral("连接成功");
    case MainModbusState::Disconnected:
        return QStringLiteral("连接断开");
    case MainModbusState::Error:
        return error;
    }
    return QString();
}
} // namespace

void MainModbusStatus::applyUiState(QStatusBar *statusBar,
                                    MainModbusState state,
                                    const QString &error)
{
    if (!statusBar) {
        return;
    }

    QString message;
    QString style;
    QString tooltip;
    switch (state) {
    case MainModbusState::Connected:
        message = QStringLiteral("Modbus连接成功");
        style = QStringLiteral("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
        tooltip = QStringLiteral("主设备 Modbus连接正常");
        break;
    case MainModbusState::Disconnected:
        message = QStringLiteral("Modbus设备断开连接");
        style = QStringLiteral("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
        tooltip = QStringLiteral("主设备 Modbus已断开");
        break;
    case MainModbusState::Error:
        message = QStringLiteral("Modbus错误: %1").arg(error);
        style = QStringLiteral("color: #ffaa00; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
        tooltip = QStringLiteral("主设备 Modbus错误: %1").arg(error);
        break;
    }

    statusBar->showMessage(message, state == MainModbusState::Error ? 5000 : 3000);
    QLabel *mainIndicator = statusBar->findChild<QLabel *>(QStringLiteral("mainModbusStatusIndicator"));
    if (mainIndicator) {
        mainIndicator->setStyleSheet(style);
        mainIndicator->setToolTip(tooltip);
    }
}

void MainModbusStatus::appendOperationRecord(OperationRecorder *recorder,
                                             MainModbusState state,
                                             const QString &error)
{
    if (!recorder) {
        return;
    }

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = QStringLiteral("系统");
    record.controlName = QStringLiteral("Modbus连接");
    record.controlType = QStringLiteral("ModbusTCP");
    record.operation = stateToOperation(state);
    record.oldValue = QString();
    record.newValue = stateToNewValue(state, error);
    recorder->addRecord(record);
}
