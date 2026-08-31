#include "mainwindow.h"
#include "buttonmodbusmapping.h"
#include "ui_mainwindow.h"
#include "batterywidget.h"
#include "featureswitchmanager.h"
#include "featureswitchwidget.h"
#include "maindevicemodbusapi.h"
#include "mainmodbusconnector.h"
#include "mainmodbuslabelmapper.h"
#include "mainmodbuspoller.h"
#include "mainmodbusstatus.h"
#include "modbuswritegate.h"
#include "modbusstringregisters.h"
#include "navigationicon.h"
#include "techchamfertoolbutton.h"
#include <QMovie>
#include <QDateTime>
#include <QDebug>
#include <QLoggingCategory>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QQmlContext>
#include <QQuickItem>
Q_LOGGING_CATEGORY(lcMainWindow, "app.mainwindow")
#include <QPainter>
#ifdef qDebug
#undef qDebug
#endif
#include <QComboBox>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QDialog>
#include <QFileDialog>
#include <QSettings>
#include <QToolTip>
#include <QButtonGroup>
#include <QGuiApplication>
#include <QInputMethod>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QtMath>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <QSocketNotifier>
#include <QIntValidator>
#include <QLineEdit>
#include <QTextEdit>
#include <QVector>
#include <QResizeEvent>
#include <QAbstractButton>

namespace {
constexpr int kRuntimePersistRegister = 8193;
constexpr int kRuntimePersistRegisterHi = 8194;
const QString kWirelessModeWarningText =
    QStringLiteral("遥控器控制互锁，请切换到当前示教器。");
const QString kRobotZeroSpeedHintText =
    QStringLiteral("当前设置的机器人全局速度为0");
const QString kAgvZeroSpeedHintText =
    QStringLiteral("当前设置的全向平台速度为0");
const QString kRobotZeroSpeedHistoryText =
    QStringLiteral("操作者在机器人全局速度为0时操作");
const QString kAgvZeroSpeedHistoryText =
    QStringLiteral("操作者在全向平台速度为0时操作");

bool isSteeringModeIn123Group(SteeringMode mode)
{
    return mode == STEER_FRONT_BACK
        || mode == STEER_FRONT_ONLY
        || mode == STEER_PARALLEL;
}

QString steeringModeStatusText(SteeringMode mode)
{
    switch (mode) {
    case STEER_FRONT_ONLY: return QStringLiteral("前轮转向");
    case STEER_PARALLEL: return QStringLiteral("平移模式");
    case STEER_LATERAL: return QStringLiteral("横移转向");
    case STEER_ROTATE: return QStringLiteral("原地旋转");
    case STEER_FRONT_BACK:
    default: return QStringLiteral("前后转向");
    }
}

SteeringMode steeringModeForReg50Bit10(SteeringMode fallback)
{
    return isSteeringModeIn123Group(fallback) ? fallback : STEER_FRONT_BACK;
}

bool resolveSteeringModeFromStatus50(quint16 value,
                                     SteeringMode *mode,
                                     QString *modeText,
                                     SteeringMode bit10Fallback = STEER_FRONT_BACK)
{
    const bool bit10 = ((value >> 10) & 0x01);
    const bool bit11 = ((value >> 11) & 0x01);
    const bool bit12 = ((value >> 12) & 0x01);

    if (bit11) {
        if (mode) *mode = STEER_LATERAL;
        if (modeText) *modeText = QStringLiteral("横移转向");
        return true;
    }
    if (bit12) {
        if (mode) *mode = STEER_ROTATE;
        if (modeText) *modeText = QStringLiteral("原地旋转");
        return true;
    }
    if (bit10) {
        const SteeringMode resolved = steeringModeForReg50Bit10(bit10Fallback);
        if (mode) *mode = resolved;
        if (modeText) *modeText = steeringModeStatusText(resolved);
        return true;
    }
    return false;
}

void applyTransparentQuickWidgetBackground(QQuickWidget *widget)
{
    if (!widget) {
        return;
    }

    // QQuickWidget 默认/样式表矩形底色会盖住 QML 圆角外侧，表现为直角边框。
    // 与速度表一致：透明清除色 + 半透明背景属性，仅由 QML Rectangle 绘制圆角卡片。
    widget->setStyleSheet(QString());
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    widget->setClearColor(Qt::transparent);

    const auto syncQuickWindowTransparent = [widget]() {
        if (QQuickWindow *qw = widget->quickWindow()) {
            qw->setColor(Qt::transparent);
        }
    };
    syncQuickWindowTransparent();
    QObject::connect(widget, &QQuickWidget::statusChanged, widget,
                     [syncQuickWindowTransparent](QQuickWidget::Status status) {
                         if (status == QQuickWidget::Ready) {
                             syncQuickWindowTransparent();
                         }
                     });
}

std::array<quint16, 4> doubleToRegistersGHEFCDAB(double value)
{
    quint64 raw = 0;
    memcpy(&raw, &value, sizeof(double));

    const quint8 A = static_cast<quint8>((raw >> 56) & 0xFF);
    const quint8 B = static_cast<quint8>((raw >> 48) & 0xFF);
    const quint8 C = static_cast<quint8>((raw >> 40) & 0xFF);
    const quint8 D = static_cast<quint8>((raw >> 32) & 0xFF);
    const quint8 E = static_cast<quint8>((raw >> 24) & 0xFF);
    const quint8 F = static_cast<quint8>((raw >> 16) & 0xFF);
    const quint8 G = static_cast<quint8>((raw >> 8) & 0xFF);
    const quint8 H = static_cast<quint8>(raw & 0xFF);

    return {
        static_cast<quint16>((static_cast<quint16>(G) << 8) | H),
        static_cast<quint16>((static_cast<quint16>(E) << 8) | F),
        static_cast<quint16>((static_cast<quint16>(C) << 8) | D),
        static_cast<quint16>((static_cast<quint16>(A) << 8) | B)
    };
}

    std::array<quint16, 2> floatToRegistersCDAB(float value)
{
    quint32 raw = 0;
    memcpy(&raw, &value, sizeof(float));

    const quint8 A = static_cast<quint8>((raw >> 24) & 0xFF);
    const quint8 B = static_cast<quint8>((raw >> 16) & 0xFF);
    const quint8 C = static_cast<quint8>((raw >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(raw & 0xFF);

    return {
        static_cast<quint16>((static_cast<quint16>(C) << 8) | D),
        static_cast<quint16>((static_cast<quint16>(A) << 8) | B)
    };
}

/** 与 OperationRecorder 规范化后的中文 controlType 或原始英文类名均可匹配 */
bool operationRecordControlTypeMatches(const OperationRecord &record, const QString &englishKey)
{
    MappingConfig *cfg = MappingConfig::instance();
    return record.controlType == englishKey || record.controlType == cfg->mapControlType(englishKey);
}

QString findNearestGroupTitle(const QWidget *widget)
{
    const QWidget *current = widget;
    while (current) {
        const auto *groupBox = qobject_cast<const QGroupBox*>(current);
        if (groupBox) {
            const QString title = groupBox->title().trimmed();
            if (!title.isEmpty()) {
                return title;
            }
        }
        current = current->parentWidget();
    }
    return QString();
}

bool isInsideSteeringModeSelector(const QWidget *widget)
{
    const QWidget *current = widget;
    while (current) {
        if (qobject_cast<const SteeringModeSelector*>(current)) {
            return true;
        }
        current = current->parentWidget();
    }
    return false;
}
}

namespace {
constexpr int kAgvParkOutTriggerLengthRegStart = 5014;
constexpr int kAgvEstimatedWeightReg = 157;
constexpr int kMainCurrentLoadWeightReg = 123;

QPair<int, int> estimatedWeightLimits()
{
    return {0, 500};
}

QPair<int, int> parkOutTriggerLengthLimitsFromSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("SliderLabelLimits"));
    int lo = qRound(settings.value(QStringLiteral("agv_park_out_trigger_length_min"), 100).toDouble());
    int hi = qRound(settings.value(QStringLiteral("agv_park_out_trigger_length_max"), 1100).toDouble());
    settings.endGroup();
    if (hi < lo) {
        qSwap(lo, hi);
    }
    lo = qBound(1, lo, 1000000);
    hi = qBound(1, hi, 1000000);
    if (lo > hi) {
        qSwap(lo, hi);
    }
    return {lo, hi};
}

QPair<int, int> weightOverloadLimitRangeFromSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("SliderLabelLimits"));
    int lo = qRound(settings.value(QStringLiteral("weight_overload_limit_min"), 0).toDouble());
    int hi = qRound(settings.value(QStringLiteral("weight_overload_limit_max"), 350).toDouble());
    settings.endGroup();
    if (hi < lo) {
        qSwap(lo, hi);
    }
    lo = qBound(0, lo, 1000000);
    hi = qBound(0, hi, 1000000);
    if (lo > hi) {
        qSwap(lo, hi);
    }
    return {lo, hi};
}

QPair<int, int> weightLockLimitRangeFromSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("SliderLabelLimits"));
    int lo = qRound(settings.value(QStringLiteral("weight_lock_limit_min"), 0).toDouble());
    int hi = qRound(settings.value(QStringLiteral("weight_lock_limit_max"), 400).toDouble());
    settings.endGroup();
    if (hi < lo) {
        qSwap(lo, hi);
    }
    lo = qBound(0, lo, 1000000);
    hi = qBound(0, hi, 1000000);
    if (lo > hi) {
        qSwap(lo, hi);
    }
    return {lo, hi};
}
} // namespace

// 主控保持寄存器快照（先于使用它的成员函数定义，供 applyCachedMainControlSyncRegistersToUi 等使用）
static QMap<int, quint16> g_registerCache;

bool MainWindow::isBigFeatureEnabled(const QString &key) const
{
    if (!m_featureSwitchManager) {
        return true;
    }
    return m_featureSwitchManager->isBigFeatureEnabled(key);
}

bool MainWindow::isSmallFeatureEnabled(const QString &key) const
{
    if (!m_featureSwitchManager) {
        return true;
    }
    return m_featureSwitchManager->isSmallFeatureEnabled(key);
}

bool MainWindow::isFeatureEnabled(const QString &bigKey, const QString &smallKey) const
{
    if (!m_featureSwitchManager) {
        return true;
    }
    return m_featureSwitchManager->isFeatureEnabled(bigKey, smallKey);
}

/**
 * =============================================================================
 * MainWindow 实现文件
 * =============================================================================
 * 组织结构（对应 mainwindow.h）:
 * 1. 生命周期与核心初始化 (Life Cycle)
 * 2. UI 框架、背景与绘制 (UI Framework)
 * 3. 报警系统 (Alarm System)
 * 4. Modbus & 设备通信 (Modbus)
 * 5. 传感器与周边硬件 (Sensors)
 * 6. 机器人运动控制与配置 (Motion Control)
 * 7. 历史记录与日志 (History & Logging)
 * 8. 信号处理槽函数 (Slot Handlers)
 * 10. 内部辅助函数 (Helper Methods)
 * =============================================================================
 */

// ==========================================

void MainWindow::loadPollingRuntimeSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");

    const bool legacyUiStateSync = settings.value("ui_state_sync_enabled", true).toBool();
    m_mainUiStateSyncEnabled = settings.value("main_ui_state_sync_enabled", legacyUiStateSync).toBool();
    m_agvUiStateSyncEnabled = settings.value("agv_ui_state_sync_enabled", legacyUiStateSync).toBool();
    m_mainModbusPollIntervalMs = settings.value("main_modbus_poll_ms", 500).toInt();
    m_mainUiPollIntervalMs = settings.value("main_ui_poll_ms", 200).toInt();
    m_mainDeviceStatusPollIntervalMs = settings.value("main_device_status_poll_ms", 200).toInt();
    m_mainDeviceStatusStart = settings.value("main_device_status_start", 0).toInt();
    m_mainDeviceStatusCount = settings.value("main_device_status_count", 85).toInt();
    m_mainControlSyncStart = settings.value("main_control_sync_start", 125).toInt();
    m_mainControlSyncCount = settings.value("main_control_sync_count", 6).toInt();
    m_mainReconnectIntervalMs = settings.value("main_reconnect_ms", 5000).toInt();
    m_agvPollIntervalMs = settings.value("agv_poll_ms", 200).toInt();
    m_agvReconnectIntervalMs = settings.value("agv_reconnect_ms", 5000).toInt();

    settings.endGroup();

    settings.beginGroup("Network");
    m_agvHost = settings.value("agv_host", "192.168.1.88").toString();
    m_agvPort = static_cast<quint16>(settings.value("agv_port", 502).toUInt());
    m_tcpServerHost = settings.value("tcp_server_host", "192.168.1.70").toString();
    m_remoteSimulatorHost = settings.value("remote_simulator_host", "192.168.1.70").toString();
    settings.endGroup();

    m_mainModbusPollIntervalMs = qBound(50, m_mainModbusPollIntervalMs, 60000);
    m_mainUiPollIntervalMs = qBound(50, m_mainUiPollIntervalMs, 60000);
    m_mainDeviceStatusPollIntervalMs = qBound(50, m_mainDeviceStatusPollIntervalMs, 60000);
    m_mainDeviceStatusStart = qBound(0, m_mainDeviceStatusStart, 65535);
    m_mainDeviceStatusCount = qBound(1, m_mainDeviceStatusCount, 125);
    m_mainControlSyncStart = qBound(0, m_mainControlSyncStart, 65535);
    m_mainControlSyncCount = qBound(1, m_mainControlSyncCount, 125);
    m_mainReconnectIntervalMs = qBound(500, m_mainReconnectIntervalMs, 120000);
    m_agvPollIntervalMs = qBound(50, m_agvPollIntervalMs, 60000);
    m_agvReconnectIntervalMs = qBound(500, m_agvReconnectIntervalMs, 120000);
    m_agvPort = static_cast<quint16>(qBound(1, static_cast<int>(m_agvPort), 65535));

    if (m_recorder) {
        m_recorder->setTcpServer(m_tcpServerHost, WIN7_PORT);
    }
}

void MainWindow::savePollingRuntimeSettings() const
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    settings.setValue("main_ui_state_sync_enabled", m_mainUiStateSyncEnabled);
    settings.setValue("agv_ui_state_sync_enabled", m_agvUiStateSyncEnabled);
    settings.setValue("main_modbus_poll_ms", m_mainModbusPollIntervalMs);
    settings.setValue("main_ui_poll_ms", m_mainUiPollIntervalMs);
    settings.setValue("main_device_status_poll_ms", m_mainDeviceStatusPollIntervalMs);
    settings.setValue("main_device_status_start", m_mainDeviceStatusStart);
    settings.setValue("main_device_status_count", m_mainDeviceStatusCount);
    settings.setValue("main_control_sync_start", m_mainControlSyncStart);
    settings.setValue("main_control_sync_count", m_mainControlSyncCount);
    settings.setValue("main_reconnect_ms", m_mainReconnectIntervalMs);
    settings.setValue("agv_poll_ms", m_agvPollIntervalMs);
    settings.setValue("agv_reconnect_ms", m_agvReconnectIntervalMs);
    settings.endGroup();

    settings.beginGroup("Network");
    settings.setValue("agv_host", m_agvHost);
    settings.setValue("agv_port", m_agvPort);
    settings.setValue("tcp_server_host", m_tcpServerHost);
    settings.setValue("remote_simulator_host", m_remoteSimulatorHost);
    settings.endGroup();
    settings.sync();
}

void MainWindow::applyPollingRuntimeSettings()
{
    if (m_modbusManager) {
        m_modbusManager->setPollInterval(m_mainModbusPollIntervalMs);
        m_modbusManager->setAutoReconnect(true, m_mainReconnectIntervalMs);
    }

    if (m_modbusPollTimer && m_modbusPollTimer->isActive()) {
        m_modbusPollTimer->setInterval(m_mainUiPollIntervalMs);
    }

    if (m_modbusReadTimer && m_modbusReadTimer->isActive()) {
        m_modbusReadTimer->setInterval(m_mainDeviceStatusPollIntervalMs);
    }

    if (m_mainControlSyncTimer && m_mainControlSyncTimer->isActive()) {
        m_mainControlSyncTimer->setInterval(m_mainUiPollIntervalMs);
    }

    if (m_interlockingSyncTimer && m_interlockingSyncTimer->isActive()) {
        m_interlockingSyncTimer->setInterval(qMax(50, m_mainUiPollIntervalMs));
    }

    m_mainDeviceStatusPollIntervalMs = qBound(50, m_mainDeviceStatusPollIntervalMs, 60000);
    m_mainDeviceStatusStart = qBound(0, m_mainDeviceStatusStart, 65535);
    m_mainDeviceStatusCount = qBound(1, m_mainDeviceStatusCount, 125);
    m_mainControlSyncStart = qBound(0, m_mainControlSyncStart, 65535);
    m_mainControlSyncCount = qBound(1, m_mainControlSyncCount, 125);

    if (m_agvModbusManager) {
        m_agvModbusManager->setPollInterval(m_agvPollIntervalMs);
        m_agvModbusManager->setAutoReconnect(true, m_agvReconnectIntervalMs);
    }
}

void MainWindow::loadSliderLabelRuntimeSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");
    
    for (auto it = m_sliderLabelConfigs.begin(); it != m_sliderLabelConfigs.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());
        
        it.value().minValue = settings.value(keyMin, it.value().minValue).toDouble();
        it.value().maxValue = settings.value(keyMax, it.value().maxValue).toDouble();
    }
    
    settings.endGroup();
}

void MainWindow::saveSliderLabelRuntimeSettings() const
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");
    
    for (auto it = m_sliderLabelConfigs.begin(); it != m_sliderLabelConfigs.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());
        
        settings.setValue(keyMin, it.value().minValue);
        settings.setValue(keyMax, it.value().maxValue);
    }
    
    settings.endGroup();
    settings.sync();
}

void MainWindow::applySliderEditRuntimeSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    const QList<TechSliderEdit*> sliders = findChildren<TechSliderEdit*>();

    settings.beginGroup(QStringLiteral("TechSliderEditLimits"));
    for (TechSliderEdit *slider : sliders) {
        if (!slider) {
            continue;
        }
        const QString name = slider->objectName();
        if (name.isEmpty()) {
            continue;
        }

        const auto readBound = [&](const QString &suffix, double fallback) -> double {
            const QString key = name + suffix;
            return settings.contains(key) ? settings.value(key).toDouble() : fallback;
        };

        const double displayMin = readBound(QStringLiteral("_display_min"),
            readBound(QStringLiteral("_value_min"), slider->displayRangeMinimum()));
        const double displayMax = readBound(QStringLiteral("_display_max"),
            readBound(QStringLiteral("_value_max"), slider->displayRangeMaximum()));

        if (displayMax > displayMin) {
            slider->setDisplayRange(displayMin, displayMax);
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (TechSliderEdit *slider : sliders) {
        if (!slider) {
            continue;
        }
        const QString name = slider->objectName();
        if (name.isEmpty()) {
            continue;
        }
        if (settings.contains(name)) {
            slider->setVisible(settings.value(name, true).toBool());
        }
    }
    settings.endGroup();
}

void MainWindow::applySliderLabelRuntimeSettings()
{
    // 应用到所有缓存的实例
    for (auto it = m_sliderLabelConfigs.begin(); it != m_sliderLabelConfigs.end(); ++it) {
        const QString& name = it.key();
        const SliderLabelConfig& config = it.value();
        if (config.maxValue <= config.minValue) {
            continue;
        }
        
        // 首页实例
        if (m_sliderLabelInstances.contains(name)) {
            m_sliderLabelInstances[name]->setRange(config.minValue, config.maxValue);
        }

        // 环形仪表实例（widget_test1~4）
        if (m_arcGauges.contains(name)) {
            m_arcGauges[name]->setRange(config.minValue, config.maxValue);
        }
        
        // 副本页实例（在 m_pageSliders 中）
        for (auto pageIt = m_pageSliders.begin(); pageIt != m_pageSliders.end(); ++pageIt) {
            for (TechSliderLabel* slider : pageIt.value()) {
                if (slider->objectName().contains(name.split("_").last())) {
                     slider->setRange(config.minValue, config.maxValue);
                }
            }
        }
    }
}

void MainWindow::applyEstimatedWeightRuntimeSettings()
{
    QLineEdit *ed = ui ? ui->LEdit_AGV_EstimatedWeight : nullptr;
    if (!ed) {
        return;
    }
    const QPair<int, int> lim = estimatedWeightLimits();
    if (!m_estimatedWeightValidator) {
        m_estimatedWeightValidator = new QIntValidator(this);
        ed->setValidator(m_estimatedWeightValidator);
    }
    m_estimatedWeightValidator->setRange(lim.first, lim.second);

    const QString text = ed->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    bool ok = false;
    const int cur = text.toInt(&ok);
    if (!ok) {
        ed->clear();
        return;
    }
    const int clamped = qBound(lim.first, cur, lim.second);
    if (clamped != cur) {
        ed->setText(QString::number(clamped));
    }
}

void MainWindow::applyParkOutTriggerLengthRuntimeSettings()
{
    QLineEdit *ed = m_parkingLegAbnormalLengthEdit;
    if (!ed) {
        return;
    }
    const QPair<int, int> lim = parkOutTriggerLengthLimitsFromSettings();
    if (!m_parkOutTriggerLengthValidator) {
        m_parkOutTriggerLengthValidator = new QIntValidator(this);
        ed->setValidator(m_parkOutTriggerLengthValidator);
    }
    m_parkOutTriggerLengthValidator->setRange(lim.first, lim.second);

    bool ok = false;
    const int cur = ed->text().trimmed().toInt(&ok);
    if (!ok) {
        ed->setText(QStringLiteral("1100"));
        return;
    }
    const int clamped = qBound(lim.first, cur, lim.second);
    if (clamped != cur) {
        ed->setText(QString::number(clamped));
    }
}

void MainWindow::applyWeightThresholdRuntimeSettings()
{
    const QPair<int, int> overloadLim = weightOverloadLimitRangeFromSettings();
    const QPair<int, int> lockLim = weightLockLimitRangeFromSettings();

    if (m_weightOverloadLimitEdit) {
        if (!m_weightOverloadLimitValidator) {
            m_weightOverloadLimitValidator = new QIntValidator(this);
            m_weightOverloadLimitEdit->setValidator(m_weightOverloadLimitValidator);
        }
        m_weightOverloadLimitValidator->setRange(overloadLim.first, overloadLim.second);

        bool ok = false;
        const int cur = m_weightOverloadLimitEdit->text().trimmed().toInt(&ok);
        if (ok) {
            const int clamped = qBound(overloadLim.first, cur, overloadLim.second);
            if (clamped != cur) {
                const QSignalBlocker blocker(m_weightOverloadLimitEdit);
                m_weightOverloadLimitEdit->setText(QString::number(clamped));
            }
        }
    }

    if (m_weightLockLimitEdit) {
        if (!m_weightLockLimitValidator) {
            m_weightLockLimitValidator = new QIntValidator(this);
            m_weightLockLimitEdit->setValidator(m_weightLockLimitValidator);
        }
        m_weightLockLimitValidator->setRange(lockLim.first, lockLim.second);

        bool ok = false;
        const int cur = m_weightLockLimitEdit->text().trimmed().toInt(&ok);
        if (ok) {
            const int clamped = qBound(lockLim.first, cur, lockLim.second);
            if (clamped != cur) {
                const QSignalBlocker blocker(m_weightLockLimitEdit);
                m_weightLockLimitEdit->setText(QString::number(clamped));
            }
        }
    }
}

bool MainWindow::writeAgvHoldingRegisterBlock(int startAddress, const QVector<quint16> &words)
{
    if (!isFeatureEnabled("modbus_agv", "modbus_agv.write_enabled")) {
        showModbusWriteDisabledToast();
        return false;
    }
    if (!m_agvModbusManager || words.isEmpty()) {
        return false;
    }
    if (!m_agvModbusManager->writeMultipleRegisters(startAddress, words)) {
        return false;
    }
    for (int i = 0; i < words.size(); ++i) {
        m_agvRegisterShadow[startAddress + i] = words.at(i);
    }
    return true;
}

namespace {
bool isDescendantOfWidget(const QWidget *widget, const QWidget *ancestor)
{
    if (!widget || !ancestor) {
        return false;
    }
    for (const QWidget *current = widget; current; current = current->parentWidget()) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

QString buttonDisplayText(const QAbstractButton *btn)
{
    if (!btn) {
        return {};
    }
    const QString text = btn->text().trimmed();
    if (!text.isEmpty()) {
        return text;
    }
    const QString toolTip = btn->toolTip().trimmed();
    if (!toolTip.isEmpty()) {
        return toolTip;
    }
    return {};
}

/** Modbus 写入时按 TechSliderEdit 当前显示/数值范围取整钳位，避免硬编码 0~100 等旧限制 */
int clampTechSliderEditToInt(TechSliderEdit *slider, double value)
{
    if (!slider) {
        return qRound(value);
    }
    const int lo = qRound(qMin(slider->minimum(), slider->maximum()));
    const int hi = qRound(qMax(slider->minimum(), slider->maximum()));
    return qBound(lo, qRound(value), hi);
}

bool spareButtonSecondStateDarkeningEnabled(const QString &objectName)
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ButtonSecondStateDarkening"));
    const bool enabled = settings.value(objectName, true).toBool();
    settings.endGroup();
    return enabled;
}

void initChamferButtonTheme(TechChamferToolButton *button)
{
    if (!button) {
        return;
    }
    button->setFillColor(QColor(8, 18, 32, 173));
    button->setBorderColor(QColor(0, 220, 255, 180));
    button->setCheckedFillColor(QColor(0, 130, 200, 224));
    button->setCheckedBorderColor(QColor(120, 240, 255, 255));
}

void applyChamferStateColors(TechChamferToolButton *btn,
                             const QColor &fill, const QColor &border)
{
    if (!btn) {
        return;
    }
    btn->setColors(fill, border);
}

void applyTwoStateButtonStyle(TechPushButton *button,
                              bool secondState,
                              bool dimSecondState,
                              const QString &firstStateText,
                              const QString &secondStateText)
{
    if (!button) {
        return;
    }
    const bool useDark = secondState && dimSecondState;
    button->setText(secondState ? secondStateText : firstStateText);
    button->setPrimaryColor(useDark ? QColor("#7F8C8D") : QColor("#00C8FF"));
    button->setGlowColor(useDark ? QColor(127, 140, 141, 100) : QColor(0, 200, 255, 180));
}

QString pickStateValue(const MainWindow::ModbusRegisterSpec &spec, int stateIndex)
{
    if (stateIndex <= 1) {
        return spec.value1.trimmed();
    }
    if (stateIndex == 2) {
        return spec.value2.trimmed().isEmpty() ? spec.value1.trimmed() : spec.value2.trimmed();
    }
    return spec.value3.trimmed().isEmpty() ? spec.value1.trimmed() : spec.value3.trimmed();
}

bool isParkLengthWriteSpec(const MainWindow::ModbusRegisterSpec &spec)
{
    if (!spec.isConfigured() || !spec.bit.trimmed().isEmpty()) {
        return false;
    }
    const QString marker = spec.value1.trimmed();
    return marker == QStringLiteral("驻车长度参数") || marker.contains(QStringLiteral("驻车"));
}

ButtonModbusMapping::Binding modbusBindingForControllable(const QString &objectName,
                                                          const TechSliderLabel *sliderLabel)
{
    const ButtonModbusMapping::Binding known = ButtonModbusMapping::defaultBinding(objectName);
    if (!known.reads.isEmpty() || !known.writes.isEmpty()
        || objectName == QStringLiteral("techBtn_spare_1")
        || objectName == QStringLiteral("techBtn_spare_2")) {
        return known;
    }

    if (sliderLabel && sliderLabel->modbusAddress() >= 0) {
        ButtonModbusMapping::Binding binding;
        ButtonModbusMapping::RegisterSpec spec;
        spec.device = QStringLiteral("主控");
        spec.address = QString::number(sliderLabel->modbusAddress());
        binding.reads.append(spec);
        binding.readForUiSync = true;
        return binding;
    }

    return {};
}
} // namespace

QList<MainWindow::ControllableButtonInfo> MainWindow::controllableButtons() const
{
    struct Entry {
        QString displayText;
        QString widgetKind;
        QList<ModbusRegisterSpec> defaultReads;
        QList<ModbusRegisterSpec> defaultWrites;
        bool readForUiSync = false;
    };
    QMap<QString, Entry> byObjectName;
    const FeatureSwitchWidget *featureSwitch = findChild<FeatureSwitchWidget*>();

    const auto tryInsert = [&](const QWidget *widget, const QString &widgetKind) {
        if (!widget || widget->window() != this) {
            return;
        }
        // 转向模式组内的子按钮不单独暴露到控制台，统一由 steeringModeSelector 本体控制显示。
        if (qobject_cast<const QAbstractButton*>(widget) && isInsideSteeringModeSelector(widget)) {
            return;
        }
        if (isDescendantOfWidget(widget, featureSwitch)) {
            return;
        }
        const QString objectName = widget->objectName();
        if (objectName.isEmpty()) {
            return;
        }

        QString displayText;
        const TechSliderLabel *sliderLabel = qobject_cast<const TechSliderLabel*>(widget);
        if (const auto *btn = qobject_cast<const QAbstractButton*>(widget)) {
            displayText = buttonDisplayText(btn);
        } else if (sliderLabel) {
            displayText = sliderLabel->labelText().trimmed();
        }

        const ButtonModbusMapping::Binding binding = modbusBindingForControllable(objectName, sliderLabel);

        auto it = byObjectName.find(objectName);
        if (it == byObjectName.end()) {
            byObjectName.insert(objectName, {
                displayText,
                widgetKind,
                binding.reads,
                binding.writes,
                binding.readForUiSync
            });
            return;
        }
        if (it->displayText.isEmpty() && !displayText.isEmpty()) {
            it->displayText = displayText;
        }
        if (it->widgetKind.isEmpty() && !widgetKind.isEmpty()) {
            it->widgetKind = widgetKind;
        }
        if (it->defaultReads.isEmpty() && !binding.reads.isEmpty()) {
            it->defaultReads = binding.reads;
            it->readForUiSync = binding.readForUiSync;
        }
        if (it->defaultWrites.isEmpty() && !binding.writes.isEmpty()) {
            it->defaultWrites = binding.writes;
        }
    };

    for (const QAbstractButton *btn : findChildren<QAbstractButton*>()) {
        tryInsert(btn, QStringLiteral("按钮"));
    }
    for (const TechSliderLabel *sliderLabel : findChildren<TechSliderLabel*>()) {
        tryInsert(sliderLabel, QStringLiteral("滑块标签"));
    }
    if (const SteeringModeSelector *selector = findChild<SteeringModeSelector*>()) {
        tryInsert(selector, QStringLiteral("转向模式"));
    }

    QList<ControllableButtonInfo> result;
    result.reserve(byObjectName.size());
    for (auto it = byObjectName.constBegin(); it != byObjectName.constEnd(); ++it) {
        result.append({
            it.key(),
            it.value().displayText,
            it.value().widgetKind,
            it.value().defaultReads,
            it.value().defaultWrites,
            it.value().readForUiSync
        });
    }
    std::sort(result.begin(), result.end(), [](const ControllableButtonInfo &a, const ControllableButtonInfo &b) {
        return a.objectName < b.objectName;
    });
    return result;
}

void MainWindow::applyPermissionPageLoginState()
{
    const bool loggedIn = m_currentUserRole != UserRole::Operator;
    const bool isManufacturer = m_currentUserRole == UserRole::Manufacturer;

    const auto setVisibleByName = [this](const QString &name, bool visible) {
        if (QWidget *widget = findChild<QWidget*>(name)) {
            widget->setVisible(visible);
        }
    };

    setVisibleByName(QStringLiteral("loginButton"), !loggedIn);
    setVisibleByName(QStringLiteral("logoutButton"), loggedIn);
    setVisibleByName(QStringLiteral("featureButton"), loggedIn && isManufacturer);
    setVisibleByName(QStringLiteral("roleComboBox"), !loggedIn);
    setVisibleByName(QStringLiteral("passwordEdit"), !loggedIn);
    setVisibleByName(QStringLiteral("passwordHint"), !loggedIn);

    if (!loggedIn) {
        applyPermissionLoginFormForSelectedRole();
    }
}

bool MainWindow::isPermissionSelectionPending() const
{
    return !m_roleSelected && isFeatureEnabled("permission_system", "permission.admin_login");
}

void MainWindow::applyPermissionNavigationGate()
{
    if (!ui) {
        return;
    }

    const bool unlocked = !isPermissionSelectionPending();
    const auto setEnabledIf = [unlocked](QWidget *widget) {
        if (widget) {
            widget->setEnabled(unlocked);
        }
    };

    setEnabledIf(ui->TBtn_HomePage);
    setEnabledIf(ui->TBtn_HistoryRecord);
    setEnabledIf(ui->TBtn_SixAxies);
    setEnabledIf(ui->TBtn_Stepmove);
    setEnabledIf(ui->TBtn_MoveMode);
    setEnabledIf(ui->TBtn_Interlocking);
    setEnabledIf(ui->TBtn_ControlMode);
    setEnabledIf(ui->TBtn_RemoveWarning);
    if (ui->TBtn_PermissionPage) {
        ui->TBtn_PermissionPage->setEnabled(true);
    }
}

void MainWindow::applyPermissionLoginFormForSelectedRole()
{
    QComboBox *roleComboBox = findChild<QComboBox*>(QStringLiteral("roleComboBox"));
    QLineEdit *passwordEdit = findChild<QLineEdit*>(QStringLiteral("passwordEdit"));
    QLabel *hintLabel = findChild<QLabel*>(QStringLiteral("passwordHint"));
    QPushButton *loginButton = findChild<QPushButton*>(QStringLiteral("loginButton"));
    if (!roleComboBox) {
        return;
    }

    const bool isOperator =
        static_cast<UserRole>(roleComboBox->currentData().toInt()) == UserRole::Operator;
    if (passwordEdit) {
        passwordEdit->setVisible(!isOperator);
        if (isOperator) {
            passwordEdit->clear();
        }
    }
    if (hintLabel) {
        hintLabel->setVisible(!isOperator);
    }
    if (loginButton) {
        loginButton->setText(isOperator ? QStringLiteral("进入") : QStringLiteral("登录"));
    }
}

void MainWindow::applyButtonVisibilityRuntimeSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ButtonVisibility"));

    const FeatureSwitchWidget *featureSwitch = findChild<FeatureSwitchWidget*>();

    const auto applyIfControllable = [&](QWidget *widget) {
        if (!widget || widget->window() != this) {
            return;
        }
        if (isDescendantOfWidget(widget, featureSwitch)) {
            return;
        }
        const QString objectName = widget->objectName();
        if (objectName.isEmpty()) {
            return;
        }
        const bool isTarget = qobject_cast<QAbstractButton*>(widget)
            || qobject_cast<TechSliderLabel*>(widget)
            || qobject_cast<TechArcGauge*>(widget)
            || qobject_cast<SteeringModeSelector*>(widget);
        if (!isTarget) {
            return;
        }
        widget->setVisible(settings.value(objectName, true).toBool());
    };

    for (QAbstractButton *btn : findChildren<QAbstractButton*>()) {
        applyIfControllable(btn);
    }
    for (TechSliderLabel *sliderLabel : findChildren<TechSliderLabel*>()) {
        applyIfControllable(sliderLabel);
    }
    for (TechArcGauge *gauge : findChildren<TechArcGauge*>()) {
        applyIfControllable(gauge);
    }
    if (SteeringModeSelector *selector = findChild<SteeringModeSelector*>()) {
        applyIfControllable(selector);
    }

    settings.endGroup();
    reloadButtonModbusBindings();
    applyPermissionPageLoginState();
    applyPermissionNavigationGate();
    loadSpareButtonNameRegisterSettings();
    syncSpareButtonNamesFromRegisters();
    applySpareButtonRuntimeSettings();
}

void MainWindow::reloadButtonModbusBindings()
{
    static const QStringList kKnownButtons = {
        QStringLiteral("TBtn_Interlocking"),
        QStringLiteral("TBtn_ControlMode"),
        QStringLiteral("techBtn_AGV_OA"),
        QStringLiteral("techBtn_AGV_Park"),
        QStringLiteral("techBtn_resetSixAxies"),
        QStringLiteral("techBtn_spare_1"),
        QStringLiteral("techBtn_spare_2"),
        QStringLiteral("TBtn_MoveMode"),
        QStringLiteral("TBtn_RemoveWarning"),
        QStringLiteral("btn_ForceControl"),
        QStringLiteral("Btn_bigForceControl"),
        QStringLiteral("Btn_smallForceControl"),
        QStringLiteral("steeringModeSelector")
    };

    m_buttonModbusBindings.clear();
    for (const QString &name : kKnownButtons) {
        m_buttonModbusBindings.insert(name, ButtonModbusMapping::resolvedBinding(name));
    }

    const ButtonModbusMapping::Binding interlock = buttonModbusBinding(QStringLiteral("TBtn_Interlocking"));
    int interlockAddr = 8192;
    if (!interlock.writes.isEmpty()) {
        interlockAddr = ButtonModbusMapping::addressOr(interlock.writes.first(), 8192);
    } else if (!interlock.reads.isEmpty()) {
        interlockAddr = ButtonModbusMapping::addressOr(interlock.reads.first(), 8192);
    }
    ModbusWriteGate::setInterlockRegisterAddress(interlockAddr);
}

ButtonModbusMapping::Binding MainWindow::buttonModbusBinding(const QString &objectName) const
{
    const auto it = m_buttonModbusBindings.find(objectName);
    if (it != m_buttonModbusBindings.end()) {
        return it.value();
    }
    return ButtonModbusMapping::resolvedBinding(objectName);
}

void MainWindow::applyModbusAccessSwitches()
{
    if (!m_agvModbusManager) {
        return;
    }
    const bool agvReadEnabled = isFeatureEnabled("modbus_agv", "modbus_agv.read_enabled");
    const bool agvWriteEnabled = isFeatureEnabled("modbus_agv", "modbus_agv.write_enabled");
    m_agvModbusManager->setReadsEnabled(agvReadEnabled);
    m_agvModbusManager->setWritesEnabled(agvWriteEnabled);
}

void MainWindow::applyInclinometerDisplayRuntimeSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Inclinometer"));
    const double tx = qBound(0.01, settings.value(QStringLiteral("display_threshold_x_deg"), 1.0).toDouble(), 90.0);
    const double ty = qBound(0.01, settings.value(QStringLiteral("display_threshold_y_deg"), 1.0).toDouble(), 90.0);
    settings.endGroup();

    const auto formatThreshold = [](double deg) {
        return QStringLiteral("阈值：%1°").arg(deg, 0, 'f', 2);
    };

    if (m_inclinometerXQml && m_inclinometerXQml->rootObject()) {
        m_inclinometerXQml->rootObject()->setProperty("thresholdText", formatThreshold(tx));
    }
    if (m_inclinometerYQml && m_inclinometerYQml->rootObject()) {
        m_inclinometerYQml->rootObject()->setProperty("thresholdText", formatThreshold(ty));
    }
}

void MainWindow::applyPlaneHeightOffsetRuntimeSettings()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("PlaneHeight"));
    m_planeHeightOffsetMm = qBound(
        -100000.0,
        settings.value(QStringLiteral("offset_mm"), 1900.0).toDouble(),
        100000.0);
    settings.endGroup();

    if (!ui || !ui->label_PlaneHeightValue) {
        return;
    }

    const double j2Height = m_hasLastJ2Height ? m_lastJ2HeightMm : 0.0;
    const double planeHeight = j2Height - m_planeHeightOffsetMm;
    ui->label_PlaneHeightValue->setText(
        QStringLiteral("%1\nmm").arg(planeHeight, 0, 'f', 0));
}

// 1. 生命周期与核心初始化 (Life Cycle)
// ==========================================

// ==========================================
// 2. UI 框架、背景与绘制 (UI Framework)
// ==========================================

/**
 * @brief 设置主要信号与槽的连接
 *
 * 将 UI 按钮、记录器、网络及其他模块的信号与本窗口的槽绑定，
 * 保证用户交互和模块事件能驱动界面更新与相关逻辑处理。
 */
// 信号槽连接
void MainWindow::setupConnections()
{
    setupNavigationConnections();
    setupRecordAndPermissionConnections();
    setupControlConnections();
    setupSubsystemConnections();
}

void MainWindow::setupNavigationConnections()
{
    if (!isBigFeatureEnabled("ui_navigation")) {
        qCDebug(lcMainWindow) << "UI导航功能已关闭，跳过导航连接";
        return;
    }

    // 导航按钮互斥：同一时刻仅保留一个页面入口为激活态。
    const QList<QToolButton*> navButtons = {
        ui->TBtn_HomePage,
        ui->TBtn_PermissionPage,
        ui->TBtn_HistoryRecord,
        ui->TBtn_SixAxies
    };
    for (QToolButton *btn : navButtons) {
        if (!btn) {
            continue;
        }
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
    }
    if (isFeatureEnabled("permission_system", "permission.admin_login") && ui->TBtn_PermissionPage) {
        ui->TBtn_PermissionPage->setChecked(true);
    } else if (ui->TBtn_HomePage) {
        ui->TBtn_HomePage->setChecked(true);
    }

    connect(ui->TBtn_HomePage, &QPushButton::clicked, [=]() {
        if (isPermissionSelectionPending()) {
            showNotification(QStringLiteral("请先选择权限"));
            if (ui->page_Permission) {
                ui->StackedWidget->setCurrentWidget(ui->page_Permission);
            }
            if (ui->TBtn_PermissionPage) {
                ui->TBtn_PermissionPage->setChecked(true);
            }
            return;
        }
        ui->StackedWidget->setCurrentIndex(0);
        ui->TBtn_HomePage->setChecked(true);
        updateNavButtonStyles(nullptr);
    });

    connect(ui->TBtn_SixAxies, &QPushButton::clicked, [this]() {
        if (isPermissionSelectionPending()) {
            showNotification(QStringLiteral("请先选择权限"));
            if (ui->page_Permission) {
                ui->StackedWidget->setCurrentWidget(ui->page_Permission);
            }
            if (ui->TBtn_PermissionPage) {
                ui->TBtn_PermissionPage->setChecked(true);
            }
            return;
        }
        if (ui->page_SixAxies) {
            ui->StackedWidget->setCurrentWidget(ui->page_SixAxies);
        }
        ui->TBtn_SixAxies->setChecked(true);
    });

    // 旧模板按钮 Btn_Switch* 已移除，页面切换统一由左侧工具按钮负责。
}

void MainWindow::setupRecordAndPermissionConnections()
{
    if (!isBigFeatureEnabled("operation_records") && !isBigFeatureEnabled("permission_system")) {
        qCDebug(lcMainWindow) << "记录与权限功能均关闭，跳过相关连接";
        return;
    }

    connect(ui->TBtn_HistoryRecord, &QPushButton::clicked, this, [this]() {
        if (isPermissionSelectionPending()) {
            showNotification(QStringLiteral("请先选择权限"));
            if (ui->page_Permission) {
                ui->StackedWidget->setCurrentWidget(ui->page_Permission);
            }
            if (ui->TBtn_PermissionPage) {
                ui->TBtn_PermissionPage->setChecked(true);
            }
            return;
        }
        if (m_currentUserRole >= UserRole::Admin) {
            if (ui->page_HistoryRecord) {
                ui->StackedWidget->setCurrentWidget(ui->page_HistoryRecord);
            }
            ui->TBtn_HistoryRecord->setChecked(true);
            updateRecordDisplay();
            showNotification("已进入操作记录页面");
        } else {
            if (ui->TBtn_HomePage) {
                ui->TBtn_HomePage->setChecked(true);
            }
            const QString tip = "权限不足：查看历史记录需要管理员权限";
            showToast(tip, ToastKind::Warning);
            updateStatusTip(tip);
        }
    });

    ui->TBtn_MoveMode->setText("未选择模式");

    connect(ui->TBtn_MoveMode, &QPushButton::clicked, [=]() {
        if (m_moveModeUnknown) {
            m_moveModeUnknown = false;
            m_isJointMode = true; // 首次默认进入关节模式
        } else {
            m_isJointMode = !m_isJointMode;
        }

        const ButtonModbusMapping::Binding moveBinding = buttonModbusBinding(QStringLiteral("TBtn_MoveMode"));
        const ModbusRegisterSpec writeSpec = moveBinding.writes.isEmpty()
            ? ModbusRegisterSpec{}
            : moveBinding.writes.first();
        const int writeAddr = ButtonModbusMapping::addressOr(writeSpec, 525);
        const int jointValue = ButtonModbusMapping::stateValueOr(writeSpec, 1, 2);
        const int coordValue = ButtonModbusMapping::stateValueOr(writeSpec, 2, 1);

        if (m_isJointMode) {
            if (writeSpec.device == QStringLiteral("AGV")) {
                writeToAGVDevice(writeAddr, jointValue, true);
            } else {
                writeToMainDevice(writeAddr, jointValue);
            }
            ui->TBtn_MoveMode->setText("关节模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("关节模式");
                moveModeLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
            }
            showNotification("已切换至关节模式");
        } else {
            if (writeSpec.device == QStringLiteral("AGV")) {
                writeToAGVDevice(writeAddr, coordValue, true);
            } else {
                writeToMainDevice(writeAddr, coordValue);
            }
            ui->TBtn_MoveMode->setText("坐标模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("坐标模式");
                moveModeLabel->setStyleSheet("color: #ffaa00; font-weight: bold; font-size: 11px;");
            }
            showNotification("已切换至坐标模式");
        }

        updateFunctionSwitchVisuals();
        updateStepTargetButtonsState();
    });
    connect(ui->TBtn_PermissionPage, &QPushButton::clicked, [this]() {
        if (ui->page_Permission) {
            ui->StackedWidget->setCurrentWidget(ui->page_Permission);
        }
        ui->TBtn_PermissionPage->setChecked(true);
    });

    connect(m_recorder, &OperationRecorder::recordAdded, this, [this](const OperationRecord &record) {
        updateRecordDisplay();
        
        // 使用 mapping 转换显示内容
        if (m_mappingConfig && record.controlType != "Login" && record.controlType != "Logout") {
            QString mappedPage = m_mappingConfig->mapPageName(record.pageName);
            QString mappedControl = m_mappingConfig->mapControlName(record.controlName);
            QString mappedOp = m_mappingConfig->mapOperation(record.operation);
            QString mappedValue = m_mappingConfig->mapValue(record.newValue.toString());
            
            QString tip = QString("[%1] %2 -> %3 -> %4")
                .arg(record.timestamp.toString("hh:mm:ss"))
                .arg(mappedControl.isEmpty() ? record.controlName : mappedControl)
                .arg(mappedOp.isEmpty() ? record.operation : mappedOp)
                .arg(mappedValue.isEmpty() ? record.newValue.toString() : mappedValue);
            
            updateStatusTip(tip);
        }
    });
    connect(m_recorder, &OperationRecorder::recordsCleared,
            this, &MainWindow::updateRecordDisplay);

    if (ui->StackedWidget) {
        connect(ui->StackedWidget, &QStackedWidget::currentChanged,
                this, [this](int) {
            if (!isPermissionSelectionPending() || !ui->page_Permission) {
                return;
            }
            if (ui->StackedWidget->currentWidget() != ui->page_Permission) {
                ui->StackedWidget->setCurrentWidget(ui->page_Permission);
                if (ui->TBtn_PermissionPage) {
                    ui->TBtn_PermissionPage->setChecked(true);
                }
            }
        });
    }
}

void MainWindow::setupControlConnections()
{
    if (!isBigFeatureEnabled("motion_control")) {
        qCDebug(lcMainWindow) << "运动控制功能已关闭，跳过控制连接";
        return;
    }

    connect(ui->StackedWidget, &QStackedWidget::currentChanged,
            this, [this](int index) {
                const QString pageName = m_pageNames.value(index, "未知");
                qCDebug(lcMainWindow) << "切换到页面:" << pageName;

                if (m_pageSliders.contains(pageName)) {
                    qCDebug(lcMainWindow) << "当前页面有" << m_pageSliders[pageName].size()
                             << "个TechSliderLabel控件";
                }

                syncStepModeUiByCurrentPage();
                updateStepTargetButtonsState();
            });

    connect(ui->TBtn_Stepmove, &QToolButton::clicked,
            this, &MainWindow::onStepMoveButtonClicked);

    m_controlModeBtn = findChild<QToolButton*>("TBtn_ControlMode");
    if (m_controlModeBtn) {
        connect(m_controlModeBtn, &QToolButton::clicked,
                this, &MainWindow::onControlModeClicked);
    } else {
        qWarning() << "未找到TBtn_ControlMode按钮";
    }

    // 六自由度页：主控 192.168.1.13 保持寄存器 615 的 bit1 置 1（读改写，保留其它位）
    if (TechPushButton *resetSixBtn = findChild<TechPushButton*>(QStringLiteral("techBtn_resetSixAxies"))) {
        connect(resetSixBtn, &TechPushButton::clicked, this, [this]() {
            if (!isFeatureEnabled("modbus_main", "modbus_main.read_enabled")) {
                showNotification(QStringLiteral("Main Modbus 读功能已关闭"));
                return;
            }
            if (!isFeatureEnabled("modbus_main", "modbus_main.write_enabled")) {
                showModbusWriteDisabledToast();
                return;
            }
            const ButtonModbusMapping::Binding resetBinding = buttonModbusBinding(QStringLiteral("techBtn_resetSixAxies"));
            const ModbusRegisterSpec spec = !resetBinding.writes.isEmpty()
                ? resetBinding.writes.first()
                : (!resetBinding.reads.isEmpty() ? resetBinding.reads.first() : ModbusRegisterSpec{});
            const int addr = ButtonModbusMapping::addressOr(spec, 615);
            const int bitIndex = ButtonModbusMapping::bitOr(spec, 1);
            if (spec.device == QStringLiteral("AGV")) {
                writeAGVRegisterBits(addr, {qMakePair(bitIndex, true)}, QStringLiteral("六轴复位"));
                return;
            }
            if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
                showNotification(QStringLiteral("主控 Modbus 未连接"));
                return;
            }
            quint16 cur = 0;
            if (!m_modbusManager->readSingleRegister(addr, cur)) {
                qWarning() << "[六轴复位] 同步读取寄存器失败, addr=" << addr;
                showNotification(QStringLiteral("读取复位寄存器失败"));
                return;
            }
            const quint16 next = static_cast<quint16>(cur | (static_cast<quint16>(1u) << bitIndex));
            writeToMainDevice(addr, next);
            qCDebug(lcMainWindow) << "[六轴复位]" << addr << ": 原值" << cur << "→ 写入" << next << "(bit" << bitIndex << "=1)";
        });
    } else {
        qWarning() << "未找到 techBtn_resetSixAxies 按钮";
    }
}

void MainWindow::setupSubsystemConnections()
{
    if (isBigFeatureEnabled("operation_records") && m_recorder) {
        connect(m_recorder, &OperationRecorder::tcpConnectionStatusChanged,
                this, &MainWindow::onTcpConnectionStatusChanged);
        connect(m_recorder, &OperationRecorder::tcpTransmissionComplete,
                this, &MainWindow::onTcpTransmissionComplete);
        connect(m_recorder, &OperationRecorder::tcpTransmissionError,
                this, &MainWindow::onTcpTransmissionError);
    }
}


// 样式设置
void MainWindow::setupStyles()
{
    if (!isFeatureEnabled("ui_navigation", "ui.styles")) {
        qCDebug(lcMainWindow) << "UI样式功能已关闭，跳过样式设置";
        return;
    }

    //设定所有普通按钮的样式


    //方向TBtn
    QList<QToolButton *>CommonTBtns = this->findChildren<QToolButton*>();
    //普通LineEdit
    // QList<QLineEdit *>StatusLEdit = {
    //     ui->lineEdit_46,ui->lineEdit_47
    // };
    //所有LEdit
    QList<QLineEdit*>AllLEdits = this->findChildren<QLineEdit*>();


    // 应用样式
    applyToolButtonStyles(CommonTBtns);
    applyLineEditStyles(AllLEdits);

    setupBottomBarButtonIcons();

    // 顶部分组与按钮基础风格统一由 .ui 样式表维护，便于 Qt Creator 中可视化调整。
    if (ui) {
        if (ui->TBtn_Stepmove) ui->TBtn_Stepmove->setCheckable(false);
        if (ui->TBtn_MoveMode) ui->TBtn_MoveMode->setCheckable(false);
        if (ui->TBtn_ControlMode) ui->TBtn_ControlMode->setCheckable(false);
        updateFunctionSwitchVisuals();
    }

    //record
    // 设置蓝色背景
    ui->page_HistoryRecord->setStyleSheet(
        "QWidget#recordPage {"
        "    background-color: #1a5fb4;"  // 蓝色背景
        "}"

        "QLabel#recordTitle {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 20px;"
        "    font-weight: bold;"
        "}"

        "QLabel#timeLabel {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 12px;"
        "}"

        "QWidget#controlPanel {"
        "    background-color: rgba(30, 60, 120, 0.7);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "}"

        "QPushButton {"
        "    background-color: #2d7fda;"  // 按钮蓝色
        "    color: #ffffff;"  // 按钮文字白色
        "    border: 1px solid #4a9eff;"
        "    border-radius: 5px;"
        "    padding: 6px 12px;"
        "    font-size: 12px;"
        "}"

        "QPushButton:hover {"
        "    background-color: #4a9eff;"  // 悬停时浅蓝色
        "}"

        "QPushButton:pressed {"
        "    background-color: #1a5fb4;"  // 按下时深蓝色
        "}"

        "QComboBox {"
        "    background-color: #2d7fda;"  // 下拉框蓝色
        "    color: #ffffff;"  // 下拉框文字白色
        "    border: 1px solid #4a9eff;"
        "    border-radius: 5px;"
        "    padding: 4px 8px;"
        "    min-width: 100px;"
        "}"

        "QComboBox::drop-down {"
        "    border: none;"
        "}"

        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 1px solid #4a9eff;"
        "    padding-left: 8px;"
        "}"

        "QComboBox QAbstractItemView {"
        "    background-color: #2d7fda;"  // 下拉列表蓝色
        "    color: #ffffff;"  // 下拉列表文字白色
        "    selection-background-color: #4a9eff;"  // 选中项浅蓝色
        "}"

        "QTextEdit#recordDisplay {"
        "    background-color: rgba(30, 60, 120, 0.5);"  // 半透明深蓝色
        "    color: #ffffff;"  // 文字白色
        "    border: 1px solid #2d7fda;"
        "    border-radius: 8px;"
        "    font-family: Consolas, Courier New, monospace;"
        "    font-size: 12px;"
        "    padding: 10px;"
        "}"

        "QWidget#statsPanel {"
        "    background-color: rgba(30, 60, 120, 0.7);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "}"

        "QLabel {"
        "    color: #ffffff;"  // 所有标签文字白色
        "    font-size: 12px;"
        "}"

        "QLabel[objectName^='stats'] {"
        "    color: #ffffff;"  // 统计标签白色
        "    font-weight: bold;"
        "}"
        );
}

void MainWindow::setupBottomBarButtonIcons()
{
    if (!ui) {
        return;
    }

    constexpr QColor kCyan(0, 200, 255);
    const auto setNavIcon = [&](TechChamferToolButton *btn, NavIconKind kind, int px = 22) {
        if (!btn) {
            return;
        }
        btn->setIcon(navigationIcon(kind, QSize(px, px), kCyan));
        btn->setIconSize(QSize(px, px));
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setMinimumHeight(46);
        initChamferButtonTheme(btn);
    };

    setNavIcon(ui->TBtn_HomePage, NavIconKind::Home);
    setNavIcon(ui->TBtn_PermissionPage, NavIconKind::Permission);
    setNavIcon(ui->TBtn_HistoryRecord, NavIconKind::History);
    setNavIcon(ui->TBtn_SixAxies, NavIconKind::SixAxis);
    setNavIcon(ui->TBtn_Stepmove, NavIconKind::StepMove);
    setNavIcon(ui->TBtn_MoveMode, NavIconKind::JointMode);
    setNavIcon(ui->TBtn_Interlocking, NavIconKind::ControlMenu);
    setNavIcon(ui->TBtn_ControlMode, NavIconKind::WiredControl);

    if (ui->TBtn_RemoveWarning) {
        constexpr QColor kClearAlarmIcon(255, 244, 244);
        ui->TBtn_RemoveWarning->setIcon(
            navigationIcon(NavIconKind::ClearAlarm, QSize(24, 24), kClearAlarmIcon));
        ui->TBtn_RemoveWarning->setIconSize(QSize(24, 24));
        ui->TBtn_RemoveWarning->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        ui->TBtn_RemoveWarning->setMinimumHeight(46);
        ui->TBtn_RemoveWarning->setFillColor(QColor(128, 24, 24, 230));
        ui->TBtn_RemoveWarning->setBorderColor(QColor(0xff, 0x9a, 0x9a));
    }
}

void MainWindow::updateFunctionSwitchVisuals()
{
    if (!ui) {
        return;
    }

    const QColor unknownFill(96, 102, 114, 219);
    const QColor unknownBorder(0x8f, 0x99, 0xa8);

    if (m_stepModeUnknown) {
        applyChamferStateColors(ui->TBtn_Stepmove, unknownFill, unknownBorder);
    } else if (m_stepModeEnabled) {
        applyChamferStateColors(ui->TBtn_Stepmove,
                                QColor(30, 148, 84, 230),
                                QColor(0xa9, 0xff, 0xd0));
    } else {
        applyChamferStateColors(ui->TBtn_Stepmove,
                                QColor(172, 108, 26, 230),
                                QColor(0xff, 0xd7, 0xa1));
    }

    if (m_moveModeUnknown) {
        applyChamferStateColors(ui->TBtn_MoveMode, unknownFill, unknownBorder);
    } else if (m_isJointMode) {
        applyChamferStateColors(ui->TBtn_MoveMode,
                                QColor(32, 140, 86, 224),
                                QColor(0x9d, 0xff, 0xd3));
    } else {
        applyChamferStateColors(ui->TBtn_MoveMode,
                                QColor(166, 104, 24, 224),
                                QColor(0xff, 0xd2, 0x9a));
    }

    if (m_controlMode == WIRED_MODE) {
        applyChamferStateColors(ui->TBtn_ControlMode,
                                QColor(30, 126, 150, 230),
                                QColor(0xa8, 0xf0, 0xff));
    } else {
        applyChamferStateColors(ui->TBtn_ControlMode,
                                QColor(158, 122, 16, 224),
                                QColor(0xff, 0xe2, 0x8f));
    }
}

void MainWindow::applyPushButtonStyles(const QList<QPushButton*> &buttons)
{
    QString style = TransparentWidgetStyle("QPushButton");

    for(QPushButton *btn : buttons) {
        if(btn) {
            btn->setStyleSheet(style);
        } else {
            qCDebug(lcMainWindow) << "发现空的QPushButton指针";
        }
    }
}


void MainWindow::applyToolButtonStyles(const QList<QToolButton*> &buttons)
{
    QString style = BlueWidgetStyle("QToolButton");

    for(QToolButton *btn : buttons) {
        if(btn) {
            const QString name = btn->objectName();
            if (name == "TBtn_Stepmove" || name == "TBtn_MoveMode" ||
                name == "TBtn_ControlMode" || name == "TBtn_RemoveWarning" ||
                name == "TBtn_Interlocking" ||
                name == "TBtn_HomePage" || name == "TBtn_PermissionPage" ||
                name == "TBtn_HistoryRecord" || name == "TBtn_SixAxies") {
                continue;
            }
            btn->setStyleSheet(style);
        } else {
            qCDebug(lcMainWindow) << "发现空的QToolButton指针";
        }
    }
}

QString MainWindow::BlueWidgetStyle(const QString &WidgetType )
{
    return QString(
               "%1 {"
               "    background-color: transparent    ;"
               "    color: white                 ;"
               "    border: none ;                "
               "    border-radius: 6px;           "
               "    padding: 1px 1px;             "
               "    font-weight: bold;"
               "    font-family: 'Microsoft YaHei', 'Segoe UI';"
               "   font-size: 14px;"
               "}"
               "%1:hover {"
               "    background-color: #2980B9;"
               "}"
               "%1:pressed {"
               "    background-color: #21618C;"
               "}"
               "%1:disabled {"
               "    background-color: #BDC3C7;"
               "    color: #7F8C8D;"
               "}"
               ).arg(WidgetType);
}
/**
 * @brief 返回一个透明风格的样式表模板
 *
 * 该函数根据传入的 `WidgetType` 返回一个最小样式表，用于将控件背景设为透明并移除边框，
 * 常用于工具按钮和导航按钮等需要融合背景的场景。
 *
 * @param WidgetType 要应用样式的控件类型（例如 `QPushButton`, `QToolButton`）
 * @return QString 用于 `setStyleSheet()` 的样式表字符串
 */
QString MainWindow::TransparentWidgetStyle(const QString &WidgetType )
{
    return QString(
               "%1 { background: transparent; border: none; }"
               ).arg(WidgetType);
}


// 动画设置
void MainWindow::setupAnimations()
{
    if (!isFeatureEnabled("ui_navigation", "ui.animations")) {
        qCDebug(lcMainWindow) << "UI动画功能已关闭，跳过动画设置";
        return;
    }

    /**
     * @brief 配置并启动界面动画（如果有）
     *
     * 该函数集中管理界面上可复用的 QMovie/QPropertyAnimation 等动画对象，
     * 并将其与相应控件绑定。例如速度指示器的闪烁、报警图标的动画等。
     * 在当前实现中，动画内容可能为空（保留扩展点）。
     */

}
// 历史记录页面初始化
void MainWindow::initializeHistoryPage()
{
    /**
     * @brief 初始化历史记录页面 UI 元素
     *
     * 包括设置记录列表、过滤器、导出/清除按钮的初始状态与连接回调。
     * 该函数确保历史记录页面在首次显示前完成必需的控件配置。
     */

}
//显示背景
void MainWindow::loadBackgroundImage()
{
    /**
     * @brief 加载并缓存主窗口背景图片
     *
     * 优先从资源文件加载图片并缓存到 `m_backgroundPixmap`，以便在 `paintEvent`
     * 中直接绘制，避免每次重绘都进行磁盘/资源读取造成性能开销。
     */
    // 只在初始化时加载一次图片
    QPixmap pix;
    if(pix.load(":/Picture/background9.png"))
    {
        // 缩放到窗口大小并缓存
        m_backgroundPixmap = pix.scaled(this->size(),
                                        Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
        m_backgroundLoaded = true;
        qCDebug(lcMainWindow) << "背景图片预加载成功，尺寸:" << m_backgroundPixmap.size();
    }
    else
    {
        qCDebug(lcMainWindow) << "无法加载背景图片";
        m_backgroundLoaded = false;
    }
}

// 修改 paintEvent 函数
void MainWindow::paintEvent(QPaintEvent *)
{
    // 如果已经预加载了图片，直接绘制缓存的图片
    if (m_backgroundLoaded)
    {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_backgroundPixmap);
        return;
    }

    // 备用方案：如果预加载失败，使用简单背景色
    QPainter painter(this);
    painter.fillRect(this->rect(), QColor(25, 148, 225));
}

// 修改事件过滤器(虚拟键盘)
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_parkingLegAbnormalLengthEdit
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            return true;
        }
    }

    // 处理LineEdit点击事件
    if (event->type() == QEvent::MouseButtonPress) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(obj);
        const bool isMainWindowLineEdit = lineEdit && this->isAncestorOf(lineEdit);
        const bool isTiltLockPasswordEdit = lineEdit && lineEdit == m_inclinometerTiltLockPasswordEdit;
        const bool isParkingLegAbnormalLengthEdit = lineEdit && lineEdit == m_parkingLegAbnormalLengthEdit;
        if (lineEdit && lineEdit->isEnabled()
            && (isMainWindowLineEdit || isTiltLockPasswordEdit || isParkingLegAbnormalLengthEdit)) {
            // // 检查是否为管理员页面的密码输入框
            // if (lineEdit->objectName() == "passwordEdit") {
            //     // 对于密码框，我们可能需要特殊处理
            //     // 或者直接返回false让系统处理
            //     return false;
            // }

            // 显示虚拟键盘
            if (m_virtualKeyboard) {
                m_virtualKeyboard->setTargetLineEdit(lineEdit);
                m_virtualKeyboard->showAtWidget(lineEdit);
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}


// 修改applyLineEditStyles，移除只读设置
void MainWindow::applyLineEditStyles(const QList<QLineEdit*> &lineEdits)
{
    for(QLineEdit *edit : lineEdits) {
        if(edit) {
            // 仅在未配置验证器时设置默认数字验证器，避免覆盖业务控件（如TechSliderEdit）的负数范围校验。
            if (!edit->validator()) {
                QRegularExpression regExp("[0-9]*\\.?[0-9]*");
                QRegularExpressionValidator *validator = new QRegularExpressionValidator(regExp, this);
                edit->setValidator(validator);
            }

            // 设置样式
            edit->setStyleSheet(
                "QLineEdit {"
                "    background-color: white;"
                "    border: 2px solid #3498DB;"
                "    border-radius: 6px;"
                "    padding: 8px 12px;"
                "    font-size: 12px;"
                "    color: #2C3E50;"
                "    selection-background-color: #3498DB;"
                "}"
                "QLineEdit:focus {"
                "    border-color: #2980B9;"
                "    background-color: #F8F9FA;"
                "}"
                );
        } else {
            qCDebug(lcMainWindow) << "发现空的QLineEdit指针";
        }
    }
}



//科技感按钮
void MainWindow::initTechButtons() {
    // 1. 清空列表
    m_techButtons.clear();

    // 2. 查找所有 TechPushButton 类型的子对象
    // 此方法会将所有已提升的按钮加入到 m_techButtons 列表中
    QList<TechPushButton*> foundButtons = this->findChildren<TechPushButton*>();

    // 3. 遍历列表，应用统一的科技感配置
    for (TechPushButton* btn : foundButtons) {
        // 启用基本效果
        // btn->enableHoverAnimation(true);
        // btn->enableClickAnimation(true);
        // btn->setTextGlow(true);

        // 根据不同需求选择样式，以全息和能量风格为例：
        // a) 全息样式 (Holographic)
        btn->setButtonStyle(TechPushButton::StyleHolographic);
        btn->setPrimaryColor(QColor(0, 200, 255, 150)); // 半透明蓝
        btn->setSecondaryColor(QColor(255, 0, 255, 150)); // 半透明紫
        // btn->enableScanLine(true); // 启用扫描线动画

        // b) 能量样式 (Energy)
        // btn->setButtonStyle(TechPushButton::StyleEnergy);
        // btn->setPrimaryColor(QColor(255, 100, 0)); // 橙色
        // btn->setSecondaryColor(QColor(255, 220, 0)); // 黄色
        // btn->enablePulseEffect(true); // 启用脉冲动画

        // 4. 连接到信号槽（例如，统一连接到处理函数）
        // connect(btn, &TechPushButton::clicked, this, &MainWindow::onTechButtonClicked);

        // 5. 存储到成员变量列表以备后用
        m_techButtons.append(btn);
    }



}





void MainWindow::initSpeedGaugeUI()
{
    // 初始化环形仪表 (TechArcGauge)
    // 映射关系：占位控件 -> 运行时创建的 TechArcGauge
    struct ArcConfig {
        QWidget* placeholder;
        QString name;
        QString label;
        QString secondLabel;
        QString suffix;
        double min;
        double max;
        int precision;
    };

    QList<ArcConfig> configs = {
        {ui->widget_test1, "robot_ArcGauge_J1Angle", "悬臂角度", QString(), "°", -170, 170, 1},
        {ui->widget_test2, "robot_ArcGauge_J2Height", "升降高度", QString(), "mm", 4400, 7200, 0},
        {ui->widget_test3, "robot_ArcGauge_J3Length", "总伸展长度", QString(), "mm", 3765, 6765, 0},
        {ui->widget_test4, "robot_ArcGauge_J4Angle", "末端角度", QString(), "°", -45, 45, 1},
        {ui->widget_SixAxies_1, "robot_ArcGauge_SixAxis1", "RX", QString(), "°", -15, 15, 2},
        {ui->widget_SixAxies_2, "robot_ArcGauge_SixAxis2", "RY", QString(), "°", -15, 15, 2},
        {ui->widget_SixAxies_3, "robot_ArcGauge_SixAxis3", "RZ", QString(), "°", -12, 12, 2},
        {ui->widget_SixAxies_4, "robot_ArcGauge_SixAxis4", "X", QString(), "mm", -110, 110, 2},
        {ui->widget_SixAxies_5, "robot_ArcGauge_SixAxis5", "Y", QString(), "mm", -110, 110, 2},
        {ui->widget_SixAxies_6, "robot_ArcGauge_SixAxis6", "Z", QString(), "mm", -90, 90, 2}
    };

    for (auto &cfg : configs) {
        if (m_sliderLabelConfigs.contains(cfg.name)) {
            const SliderLabelConfig &rangeCfg = m_sliderLabelConfigs[cfg.name];
            if (rangeCfg.maxValue > rangeCfg.minValue) {
                cfg.min = rangeCfg.minValue;
                cfg.max = rangeCfg.maxValue;
            }
            if (!rangeCfg.labelText.isEmpty()) {
                cfg.label = rangeCfg.labelText;
            }
            if (!rangeCfg.secondLabelText.isEmpty()) {
                cfg.secondLabel = rangeCfg.secondLabelText;
            }
            if (!rangeCfg.suffix.isEmpty()) {
                cfg.suffix = rangeCfg.suffix;
            }
            cfg.precision = rangeCfg.precision;
        }
    }

    for (const auto& cfg : configs) {
        if (cfg.placeholder) {
            TechArcGauge *arcGauge = new TechArcGauge(cfg.placeholder->parentWidget());
            arcGauge->setGeometry(cfg.placeholder->geometry());
            arcGauge->setObjectName(cfg.name);
            arcGauge->setRange(cfg.min, cfg.max);
            arcGauge->setValue(0);
            
            // 如果是 J3，设置速度显示相关参数
            if (cfg.name == "robot_ArcGauge_J3Length") {
                arcGauge->setSecondMaximum(40.0); // J3 速度范围 0~40 mm/s
                arcGauge->setSecondSuffix("mm/s");
                arcGauge->setSecondValue(0.0);
            } else if (cfg.name == "robot_ArcGauge_J1Angle") {
                arcGauge->setSecondMaximum(2.0);  // J1 速度范围 0~2 °/s
                arcGauge->setSecondSuffix("°/s");
            } else if (cfg.name == "robot_ArcGauge_J2Height") {
                arcGauge->setSecondMaximum(15.0); // J2 速度范围 0~15 mm/s
                arcGauge->setSecondSuffix("mm/s");
            } else if (cfg.name == "robot_ArcGauge_J4Angle") {
                arcGauge->setSecondMaximum(2.0);  // J4 速度范围 0~2 °/s
                arcGauge->setSecondSuffix("°/s");
            }
            
            arcGauge->setLabelText(cfg.label);
            arcGauge->setSecondLabelText(cfg.secondLabel);
            arcGauge->setSuffix(cfg.suffix);
            arcGauge->setPrecision(cfg.precision);

            cfg.placeholder->hide();
            arcGauge->show();

            // 存入映射表，Key 使用规范化后的名称
            m_arcGauges[cfg.name] = arcGauge;
        }
    }

    applyPlaneHeightOffsetRuntimeSettings();

    // 1. 创建 QQuickWidget 来承载 QML
    m_speedGaugeQml = new QQuickWidget(this);
    m_speedGaugeQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_speedGaugeQml->setSource(QUrl("qrc:/TechSpeedGauge.qml"));
    m_speedGaugeQml->setClearColor(Qt::transparent);

    // 2. 将原本放置 TechSpeedGauge 的布局或位置替换掉
    if (ui->AGV_speedGauge) {
        QWidget *parent = ui->AGV_speedGauge->parentWidget();
        m_speedGaugeQml->setParent(parent);
        m_speedGaugeQml->setGeometry(ui->AGV_speedGauge->geometry());
        ui->AGV_speedGauge->hide(); // 隐藏旧的 C++ 控件
        m_speedGaugeQml->show();
        m_speedGaugeQml->raise();
    }

    // 3. 初始化 QML 属性
    QQuickItem *rootItem = m_speedGaugeQml->rootObject();
    if (rootItem) {
        rootItem->setProperty("maxValue", 900);
        rootItem->setProperty("title", "行驶速度");
        rootItem->setProperty("unit", "mm/s");
    }

    qCDebug(lcMainWindow) << "QML 速度仪表初始化完成 - 量程: 0-900 mm/s";
}

// 示例：更新速度值
void MainWindow::updateSpeed(qreal newSpeed)
{
    if (m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
        QQuickItem *root = m_speedGaugeQml->rootObject();
        if (!qFuzzyCompare(root->property("currentValue").toReal() + 1.0, newSpeed + 1.0)) {
            root->setProperty("currentValue", newSpeed);
        }
    }
}

void MainWindow::initInclinometerAndRobotPowerStrip()
{
    QWidget *host = findChild<QWidget*>(QStringLiteral("widget_InclinometerPowerStrip"));
    if (!host) {
        qCWarning(lcMainWindow) << "未找到 widget_InclinometerPowerStrip，跳过倾角+总功率条初始化";
        return;
    }
    m_inclinometerPowerStripWidget = host;

    if (QLayout *oldLayout = host->layout()) {
        QLayoutItem *item = nullptr;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete oldLayout;
    }

    // Independent cards provide their own shells; host only carries warn/alarm border.
    host->setStyleSheet(QStringLiteral(
        "background: transparent;"
        "border: 1px solid transparent;"
        "border-radius: 14px;"));

    auto createInclinometerQml = [this, host](const QString &axisTitle, QQuickWidget *&out) {
        out = new QQuickWidget(host);
        out->setMinimumHeight(96);
        out->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        out->setResizeMode(QQuickWidget::SizeRootObjectToView);
        applyTransparentQuickWidgetBackground(out);
        connect(out, &QQuickWidget::statusChanged, this,
                [this, out, axisTitle](QQuickWidget::Status status) {
                    if (status == QQuickWidget::Error && out) {
                        const auto errs = out->errors();
                        for (const auto &err : errs) {
                            qWarning() << "InclinometerCard QML error (" << axisTitle << "):" << err.toString();
                        }
                        return;
                    }
                    if (status == QQuickWidget::Ready && out && out->rootObject()) {
                        out->rootObject()->setProperty("axisLabel", axisTitle);
                        applyInclinometerDisplayRuntimeSettings();
                    }
                }, Qt::UniqueConnection);
        out->setSource(QUrl(QStringLiteral("qrc:/InclinometerCard.qml")));
        if (QQuickItem *root = out->rootObject()) {
            root->setProperty("axisLabel", axisTitle);
            root->setProperty("tiltValue", 0.0);
            root->setProperty("thresholdText", QString());
        }
    };

    createInclinometerQml(QStringLiteral("X轴倾角"), m_inclinometerXQml);
    createInclinometerQml(QStringLiteral("Y轴倾角"), m_inclinometerYQml);

    m_robotTotalPowerQml = new QQuickWidget(host);
    m_robotTotalPowerQml->setMinimumHeight(96);
    m_robotTotalPowerQml->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_robotTotalPowerQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    applyTransparentQuickWidgetBackground(m_robotTotalPowerQml);
    connect(m_robotTotalPowerQml, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status status) {
                if (status == QQuickWidget::Error && m_robotTotalPowerQml) {
                    const auto errs = m_robotTotalPowerQml->errors();
                    for (const auto &err : errs) {
                        qWarning() << "RobotTotalPower QML error:" << err.toString();
                    }
                }
            }, Qt::UniqueConnection);
    m_robotTotalPowerQml->setSource(QUrl("qrc:/RobotTotalPowerCard.qml"));

    if (QQuickItem *root = m_robotTotalPowerQml->rootObject()) {
        root->setProperty("title", QStringLiteral("总功率"));
        root->setProperty("unit", QStringLiteral("W"));
        root->setProperty("currentPower", 0.0);
        root->setProperty("showCardBackground", true);
    }

    auto *column = new QVBoxLayout(host);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(6);
    column->addWidget(m_robotTotalPowerQml, 0);

    auto *tiltRow = new QHBoxLayout();
    tiltRow->setContentsMargins(0, 0, 0, 0);
    tiltRow->setSpacing(6);
    tiltRow->addWidget(m_inclinometerXQml, 1);
    tiltRow->addWidget(m_inclinometerYQml, 1);
    column->addLayout(tiltRow, 1);

    applyInclinometerDisplayRuntimeSettings();
}

void MainWindow::initWeightCard()
{
    QWidget *host = findChild<QWidget*>(QStringLiteral("widget_Weight"));
    if (!host) {
        qCWarning(lcMainWindow) << "未找到 widget_Weight，跳过当前负载重量卡片初始化";
        return;
    }

    if (QLayout *oldLayout = host->layout()) {
        QLayoutItem *item = nullptr;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete oldLayout;
    }

    host->setStyleSheet(QStringLiteral("background: transparent;"));

    m_weightCardQml = new QQuickWidget(host);
    m_weightCardQml->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_weightCardQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    applyTransparentQuickWidgetBackground(m_weightCardQml);
    connect(m_weightCardQml, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status status) {
                if (status == QQuickWidget::Error && m_weightCardQml) {
                    const auto errs = m_weightCardQml->errors();
                    for (const auto &err : errs) {
                        qWarning() << "WeightCard QML error:" << err.toString();
                    }
                }
            }, Qt::UniqueConnection);
    m_weightCardQml->setSource(QUrl(QStringLiteral("qrc:/WeightCard.qml")));

    if (QQuickItem *root = m_weightCardQml->rootObject()) {
        root->setProperty("title", QStringLiteral("当前负载"));
        root->setProperty("unit", QStringLiteral("KG"));
        root->setProperty("weightValue", 0.0);
        root->setProperty("dataValid", false);
    }

    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_weightCardQml);
}

void MainWindow::initDeviceCoordPanel()
{
    m_deviceCoordPanelQml = findChild<QQuickWidget*>(QStringLiteral("quickWidget_DeviceCoordPanel"));
    if (!m_deviceCoordPanelQml) {
        qCWarning(lcMainWindow) << "未找到 quickWidget_DeviceCoordPanel，跳过坐标面板初始化";
        return;
    }

    m_deviceCoordPanelQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    applyTransparentQuickWidgetBackground(m_deviceCoordPanelQml);
    connect(m_deviceCoordPanelQml, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status status) {
                if (status == QQuickWidget::Error && m_deviceCoordPanelQml) {
                    const auto errs = m_deviceCoordPanelQml->errors();
                    for (const auto &err : errs) {
                        qWarning() << "DeviceCoordPanel QML error:" << err.toString();
                    }
                }
            }, Qt::UniqueConnection);
    m_deviceCoordPanelQml->setSource(QUrl(QStringLiteral("qrc:/DeviceCoordPanel.qml")));

    if (QQuickItem *root = m_deviceCoordPanelQml->rootObject()) {
        root->setProperty("coordX", 0.0);
        root->setProperty("coordY", 0.0);
        root->setProperty("coordZ", 0.0);
        root->setProperty("coordAr", 0.0);
    }
}

void MainWindow::updateDeviceCoordPanelFromCache()
{
    if (!(m_deviceCoordPanelQml && m_deviceCoordPanelQml->rootObject())) {
        return;
    }

    constexpr int kStart = 103;
    constexpr int kEnd = 118;
    for (int a = kStart; a <= kEnd; ++a) {
        if (!g_registerCache.contains(a)) {
            return;
        }
    }

    const double cx = registersToDoubleDCBAFEHG(
        g_registerCache[103], g_registerCache[104], g_registerCache[105], g_registerCache[106]);
    const double cy = registersToDoubleDCBAFEHG(
        g_registerCache[107], g_registerCache[108], g_registerCache[109], g_registerCache[110]);
    const double cz = registersToDoubleDCBAFEHG(
        g_registerCache[111], g_registerCache[112], g_registerCache[113], g_registerCache[114]);
    const double car = registersToDoubleDCBAFEHG(
        g_registerCache[115], g_registerCache[116], g_registerCache[117], g_registerCache[118]);

    QQuickItem *root = m_deviceCoordPanelQml->rootObject();
    const auto setRealIfChanged = [root](const char *name, double value) {
        if (!qFuzzyCompare(root->property(name).toDouble() + 1.0, value + 1.0)) {
            root->setProperty(name, value);
        }
    };
    setRealIfChanged("coordX", cx);
    setRealIfChanged("coordY", cy);
    setRealIfChanged("coordZ", cz);
    setRealIfChanged("coordAr", car);
}

void MainWindow::updateRobotTotalPower(quint16 powerValue)
{
    if (!(m_robotTotalPowerQml && m_robotTotalPowerQml->rootObject())) {
        return;
    }

    QQuickItem *root = m_robotTotalPowerQml->rootObject();
    const qreal numericPower = static_cast<qreal>(powerValue);
    if (!qFuzzyCompare(root->property("currentPower").toReal() + 1.0, numericPower + 1.0)) {
        // QML 的 onCurrentPowerChanged 是趋势图唯一采样入口，避免一次数据双重重绘。
        root->setProperty("currentPower", numericPower);
    }
}

void MainWindow::updateCurrentLoadWeight(quint16 rawValue)
{
    if (!(m_weightCardQml && m_weightCardQml->rootObject())) {
        return;
    }

    QQuickItem *root = m_weightCardQml->rootObject();
    const qreal kg = static_cast<qreal>(rawValue);
    if (!qFuzzyCompare(root->property("weightValue").toReal() + 1.0, kg + 1.0)) {
        root->setProperty("weightValue", kg);
    }
    if (!root->property("dataValid").toBool()) {
        root->setProperty("dataValid", true);
    }
}

void MainWindow::updateInclinometerValue(bool isXAxis, quint16 rawValue)
{
    QQuickWidget *target = isXAxis ? m_inclinometerXQml : m_inclinometerYQml;
    if (!(target && target->rootObject())) {
        return;
    }

    const qint16 signedRaw = static_cast<qint16>(rawValue);
    const qreal degree = static_cast<qreal>(signedRaw) / 100.0;
    QQuickItem *root = target->rootObject();
    if (!qFuzzyCompare(root->property("tiltValue").toReal() + 1.0, degree + 1.0)) {
        root->setProperty("tiltValue", degree);
    }

    if (isXAxis) {
        m_inclinometerXDegree = degree;
    } else {
        m_inclinometerYDegree = degree;
    }
    refreshInclinometerTiltPresentation();
}

namespace {
bool isInclinometerTiltRiskWarningDegree(qreal degree)
{
    const qreal absDeg = qAbs(degree);
    return absDeg > 0.8 && absDeg <= 1.0;
}

bool isInclinometerTiltLockDegree(qreal degree)
{
    return qAbs(degree) > 1.0;
}

QString inclinometerPowerStripNormalStyleSheet()
{
    return QStringLiteral(
        "background: transparent;"
        "border: 1px solid transparent;"
        "border-radius: 14px;");
}

QString inclinometerPowerStripWarningStyleSheet()
{
    return QStringLiteral(
        "background: transparent;"
        "border: 2px solid #FFD966;"
        "border-radius: 14px;");
}

QString inclinometerPowerStripAlarmStyleSheet()
{
    return QStringLiteral(
        "background: transparent;"
        "border: 2px solid #FF6666;"
        "border-radius: 14px;");
}

void positionFloatingPopupCenter(QWidget *widget)
{
    if (!widget) {
        return;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    const QRect area = screen->availableGeometry();
    const int x = area.left() + (area.width() - widget->width()) / 2;
    const int y = area.top() + (area.height() - widget->height()) / 2;
    widget->move(x, y);
}

void positionFloatingPopupBottomRight(QWidget *widget, int bottomMarginPx, QWidget *avoidWidget = nullptr)
{
    if (!widget) {
        return;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    const QRect area = screen->availableGeometry();
    const int x = area.right() - widget->width() - 50;
    int y = area.bottom() - widget->height() - bottomMarginPx;
    if (avoidWidget && avoidWidget->isVisible()) {
        y = qMin(y, avoidWidget->y() - widget->height() - 12);
    }
    y = qBound(area.top() + 20, y, area.bottom() - widget->height() - 20);
    widget->move(x, y);
}
} // namespace

void MainWindow::refreshInclinometerTiltPresentation()
{
    const bool inLockZone = isInclinometerTiltLockDegree(m_inclinometerXDegree)
                         || isInclinometerTiltLockDegree(m_inclinometerYDegree);
    const bool inWarnZone = !inLockZone
                         && (isInclinometerTiltRiskWarningDegree(m_inclinometerXDegree)
                             || isInclinometerTiltRiskWarningDegree(m_inclinometerYDegree));

    if (m_inclinometerPowerStripWidget) {
        if (inLockZone) {
            m_inclinometerPowerStripWidget->setStyleSheet(inclinometerPowerStripAlarmStyleSheet());
        } else if (inWarnZone) {
            m_inclinometerPowerStripWidget->setStyleSheet(inclinometerPowerStripWarningStyleSheet());
        } else {
            m_inclinometerPowerStripWidget->setStyleSheet(inclinometerPowerStripNormalStyleSheet());
        }
    }

    if (inLockZone != m_inclinometerTiltLockInZone) {
        if (inLockZone) {
            m_inclinometerTiltLockUnlocked = false;
            m_inclinometerTiltRiskInZone = false;
            m_inclinometerTiltRiskAcked = false;
            hideInclinometerTiltRiskDialog();
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = QStringLiteral("报警系统");
                record.controlName = QStringLiteral("高倾覆风险锁定");
                record.controlType = QStringLiteral("提示窗口");
                record.operation = QStringLiteral("报警触发");
                record.oldValue = QString();
                record.newValue = QStringLiteral("高倾覆风险报警！！！设备倾角过大锁定。");
                m_recorder->addRecord(record);
            }
            showInclinometerTiltLockDialog();
        } else {
            m_inclinometerTiltLockUnlocked = false;
            hideInclinometerTiltLockDialog();
        }
        m_inclinometerTiltLockInZone = inLockZone;
    } else if (inLockZone) {
        if (m_inclinometerTiltLockUnlocked) {
            if (m_inclinometerTiltLockDialog && !m_inclinometerTiltLockDialog->isVisible()) {
                presentInclinometerTiltLockUnlocked();
            }
        } else {
            showInclinometerTiltLockDialog();
        }
    }

    if (inLockZone) {
        return;
    }

    if (inWarnZone != m_inclinometerTiltRiskInZone) {
        if (inWarnZone) {
            m_inclinometerTiltRiskAcked = false;
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = QStringLiteral("报警系统");
                record.controlName = QStringLiteral("倾覆风险提示");
                record.controlType = QStringLiteral("提示窗口");
                record.operation = QStringLiteral("报警触发");
                record.oldValue = QString();
                record.newValue = QStringLiteral("倾覆风险提示！设备倾角过大。");
                m_recorder->addRecord(record);
            }
            showInclinometerTiltRiskDialog();
        } else {
            m_inclinometerTiltRiskAcked = false;
            hideInclinometerTiltRiskDialog();
        }
        m_inclinometerTiltRiskInZone = inWarnZone;
        return;
    }

    if (inWarnZone && !m_inclinometerTiltRiskAcked) {
        showInclinometerTiltRiskDialog();
    }
}

//模拟速度
void MainWindow::setupDataSimulation()
{
    // 创建定时器（用于测试，实际使用时注释掉）
    m_dataSimulator = new QTimer(this);
    connect(m_dataSimulator, &QTimer::timeout, this, [this]() {
        if (!(m_speedGaugeQml && m_speedGaugeQml->rootObject())) {
            qCDebug(lcMainWindow) << "错误：m_speedGaugeQml 为空指针！";
            return;
        }
        // 生成随机速度波动（测试用）
        qreal current = m_speedGaugeQml->rootObject()->property("currentValue").toReal();
        qreal change = (QRandomGenerator::global()->bounded(100) / 10.0) - 5.0;
        qreal newSpeed = qBound(0.0, current + change, 900.0);

        updateSpeed(newSpeed);
    });

    // 临时测试：使用 pushButton_5 (工艺页) 手动触发力控报警
    // 因为 Btn_Test 在当前UI中不存在
    // if (ui->pushButton_5) {
    //     ui->pushButton_5->setText("测试报警");
    //     connect(ui->pushButton_5, &QPushButton::clicked, this, [this]() {
    //         qCDebug(lcMainWindow) << "测试按钮点击：手动触发力控超限报警";
    //         showAlarm("力控超限警报触发\n请点击下方按钮清除报警\n请手动移出超限位置", "#ff8800");
    //     });
    // }
    
    // 原有的 Btn_Test 代码已删除
}

void MainWindow::initSliderEditUI()
{
    QList<TechSliderEdit*> sliders = this->findChildren<TechSliderEdit*>();
    qCDebug(lcMainWindow) << "找到" << sliders.size() << "个TechSliderEdit控件";

    // 存储所有非AGV的TechSliderEdit，用于联动更新
    QList<TechSliderEdit*> nonAGVSliders;

    for (TechSliderEdit *slider : sliders) {
        QString objName = slider->objectName();

        // 根据控件对象名设置特定的值范围
        if (objName == "TechSliderEdit_HoriSupSec_RotationSpeed") {
            // 水平支撑段旋转速度：0-5 °/s，支持一位小数
            slider->setLabelText("旋转速度");
            slider->setRange(0, 5);
            slider->setValue(1); // 默认值设为中间值
            slider->setSuffix("°/s");
            slider->setPrecision(1); // 可以输入一位小数
            nonAGVSliders.append(slider);

            qCDebug(lcMainWindow) << "初始化: TechSliderEdit_HoriSupSec_RotationSpeed, 范围:0-5 °/s, 默认值:1 °/s, 精度:1";
        }
        else if (objName == "TechSliderEdit_HoriSupSec_MoveSpeed") {
            // 水平支撑段移动速度：0-20 mm/s，只能输入整数
            slider->setLabelText("伸缩速度");
            slider->setRange(0, 20);
            slider->setValue(10); // 默认值设为中间值
            slider->setSuffix("mm/s");
            slider->setPrecision(0); // 只能输入整数
            nonAGVSliders.append(slider);

            qCDebug(lcMainWindow) << "初始化: TechSliderEdit_HoriSupSec_MoveSpeed, 范围:0-20 mm/s, 默认值:10 mm/s";
        }
        else if (objName == "TechSliderEdit_VeSupSec_MoveSpeed") {
            // 垂直支撑段移动速度：0-30 mm/s，只能输入整数
            slider->setLabelText("升降速度");
            slider->setRange(0, 35);
            slider->setValue(15); // 默认值设为中间值
            slider->setSuffix("mm/s");
            slider->setPrecision(0); // 只能输入整数
            nonAGVSliders.append(slider);

            qCDebug(lcMainWindow) << "初始化: TechSliderEdit_VeSupSec_MoveSpeed, 范围:0-30 mm/s, 默认值:15 mm/s";
        }
        else if (objName == "TechSliderEdit_EOAT_RotationSpeed") {
            // EOAT旋转速度：0-5 °/s，支持一位小数
            slider->setLabelText("全局速度");
            slider->setRange(0, 100);
            slider->setValue(3); // 默认值设为整数
            slider->setSuffix("%");
            slider->setPrecision(0); // 可以输入一位小数
            nonAGVSliders.append(slider);

            qCDebug(lcMainWindow) << "初始化: TechSliderEdit_EOAT_RotationSpeed, 范围:0-5 °/s, 默认值:3 °/s, 精度:1";
        }
        else if (objName == "SEdit_AGV_MoveSpeed") {
            // 六自由度平台速度：0-100 mm/s
            slider->setLabelText("六自由度平台速度");
            slider->setRange(0, 100);
            slider->setValue(0);
            slider->setSuffix("mm/s");
            slider->setPrecision(0);

            qCDebug(lcMainWindow) << "初始化: SEdit_AGV_MoveSpeed, 范围:0-100 mm/s, 默认值:0 mm/s";
        }
        else if (objName == "SEdit_AGV_Angle") {
            // AGV转向角度：-25~25 °
            slider->setLabelText("六自由度平台转向角度");
            slider->setRange(-25, 25);
            slider->setValue(0);
            slider->setSuffix("°");
            slider->setPrecision(0);

            qCDebug(lcMainWindow) << "初始化: SEdit_AGV_Angle, 范围:-25~25 °, 默认值:0 °";
        }
        else if (objName == "TechSliderEdit_Robot_RobotSpeed") {
            slider->setLabelText("机器人全局速度");
            slider->setRange(0, 100);
            slider->setValue(0);
            slider->setSuffix("%");
            slider->setPrecision(0);

            qCDebug(lcMainWindow) << "初始化: TechSliderEdit_Robot_RobotSpeed, 范围:0-100 %, 默认值:0 %";
        }
        else {
            // 如果有其他未处理的TechSliderEdit控件，输出警告
            qWarning() << "未处理的TechSliderEdit控件:" << objName;
        }

        // 记录到slider列表
        m_sliders.append(slider);
    }

    // 为非AGV的TechSliderEdit连接联动更新信号
    for (TechSliderEdit *slider : nonAGVSliders) {
        connect(slider, &TechSliderEdit::valueChangedWithRecord,
                this, [this, slider, nonAGVSliders](double oldValue, double newValue) {
                    Q_UNUSED(oldValue);
                    // 只处理非AGV的TechSliderEdit
                    QString objName = slider->objectName();
                    if (objName != "SEdit_AGV_MoveSpeed" && objName != "SEdit_AGV_Angle") {
                        onNonAGVSliderEditChanged(slider, newValue, nonAGVSliders);
                    }
                });
    }

    TechSliderEdit *robotSpeedSlider = findChild<TechSliderEdit*>("TechSliderEdit_Robot_RobotSpeed");
    if (robotSpeedSlider) {
        robotSpeedSlider->setLabelText("机器人全局速度");
        robotSpeedSlider->setRange(0, 100);
        robotSpeedSlider->setSuffix("%");
        robotSpeedSlider->setPrecision(0);

        connect(robotSpeedSlider, &TechSliderEdit::valueChangedWithRecord,
                this, [this, robotSpeedSlider](double /*oldValue*/, double newValue) {
                    const int speedValue = clampTechSliderEditToInt(robotSpeedSlider, newValue);
                    writeToMainDevice(5001, speedValue);

                    qCDebug(lcMainWindow) << "RobotSpeed(%)直写地址5001, 值:" << speedValue
                             << "范围:" << robotSpeedSlider->minimum() << "~" << robotSpeedSlider->maximum();
                });
    } else {
        qWarning() << "未找到控件: TechSliderEdit_Robot_RobotSpeed";
    }
}

/**
 * @brief 处理非 AGV 的滑块编辑值变化并实现联动
 *
 * 当页面中非 AGV 的 `TechSliderEdit` 值发生变化时，调用此函数完成：
 * - 更新相关联的滑块标签或其他 UI 显示
 * - 将变化记录到 `OperationRecorder`（如需要）
 * - 执行必要的边界校验与同步写入（如果配置了 Modbus 写入）
 *
 * @param changedSlider 发生变化的滑块控件指针
 * @param newValue 新值
 * @param allNonAGVSliders 页面内所有非 AGV 的滑块列表（用于联动计算）
 */
void MainWindow::onNonAGVSliderEditChanged(TechSliderEdit *changedSlider, double newValue,
                                           const QList<TechSliderEdit*> &allNonAGVSliders)
{
    qCDebug(lcMainWindow) << "非AGV TechSliderEdit值变化：" << changedSlider->objectName()
             << "新值:" << newValue;

    // 1. 计算百分比：值 / 最大值 × 100，取整
    double maxValue = changedSlider->maximum();
    double percentage = (newValue / maxValue) * 100.0;
    int percentageInt = qRound(percentage); // 四舍五入取整

    qCDebug(lcMainWindow) << "计算百分比: (" << newValue << " / " << maxValue << ") × 100 = "
             << percentage << "% → 取整后:" << percentageInt << "%";

    // 2. 写入192.168.1.13设备的79地址
    writeToMainDevice(79, percentageInt);

    // 3. 更新所有其他非AGV TechSliderEdit的值
    for (TechSliderEdit *slider : allNonAGVSliders) {
        if (slider != changedSlider) {
            double sliderMax = slider->maximum();
            double newSliderValue = sliderMax * percentageInt / 100.0;

            // 四舍五入到整数（因为非AGV控件只能输入整数）
            newSliderValue = qRound(newSliderValue);

            // 确保值在范围内
            if (newSliderValue < slider->minimum()) newSliderValue = slider->minimum();
            if (newSliderValue > sliderMax) newSliderValue = sliderMax;

            qCDebug(lcMainWindow) << "更新" << slider->objectName() << ": "
                     << sliderMax << " × " << percentageInt << "% / 100 = "
                     << newSliderValue;

            // 设置新值（会触发记录信号）
            slider->setValue(newSliderValue);
        }
    }
}

void MainWindow::initSliderLabelUI()
{
    // 清空现有记录
    m_sliderLabels.clear();
    m_sliderLabelInstances.clear();
    m_pageSliders.clear();

    // 获取首页的所有TechSliderLabel
    QWidget* mainPage = ui->StackedWidget->widget(0);  // 假设首页是索引0
    if (!mainPage) return;

    // 查找首页中的所有TechSliderLabel
    QList<TechSliderLabel*> mainPageSliders = mainPage->findChildren<TechSliderLabel*>();

    for (TechSliderLabel* sliderLabel : mainPageSliders) {
        QString objName = sliderLabel->objectName();

        // 检查是否在配置中
        if (m_sliderLabelConfigs.contains(objName)) {
            const SliderLabelConfig& config = m_sliderLabelConfigs[objName];

            // 设置UI属性
            sliderLabel->setLabelText(config.labelText);
            sliderLabel->setRange(config.minValue, config.maxValue);
            sliderLabel->setValue(config.defaultValue);
            sliderLabel->setSuffix(config.suffix);
            sliderLabel->setPrecision(config.precision); // 设置精度

            // 设置样式
            sliderLabel->setTechBlueStyle();

            // 添加到管理列表
            m_sliderLabelInstances[objName] = sliderLabel;
            m_sliderLabels.append(sliderLabel);

            // 添加到页面映射
            m_pageSliders["软件参数"].append(sliderLabel);  // 首页名称

            qCDebug(lcMainWindow) << "初始化TechSliderLabel:" << objName
                     << "文本:" << config.labelText
                     << "范围:" << config.minValue << "-" << config.maxValue;
        } else {
            qWarning() << "未配置的TechSliderLabel:" << objName;
        }
    }
}














void MainWindow::on_TBtn_VeSupSec_Rise_released()
{
    qCDebug(lcMainWindow) << "按钮释放，尝试停止动图...";

    if (m_verticalMovie && m_verticalMovie->state() == QMovie::Running) {
        m_verticalMovie->stop();
        qCDebug(lcMainWindow) << "动图已停止。";
    }

}
/**
 * @brief 初始化并安装虚拟键盘
 *
 * 创建 `TechVirtualKeyboard` 并为所有 `QLineEdit` 安装事件过滤器，使得点击
 * 输入框时能弹出虚拟键盘进行输入。虚拟键盘通过 `eventFilter` 捕获点击事件，
 * 并展示在目标控件附近。
 */
void MainWindow::setupVirtualKeyboard()
{
    if (!isFeatureEnabled("ui_navigation", "ui.virtual_keyboard")) {
        qCDebug(lcMainWindow) << "虚拟键盘功能已关闭，跳过初始化";
        return;
    }

    // 创建虚拟键盘
    m_virtualKeyboard = new TechVirtualKeyboard(this);

    // 为所有LineEdit安装事件过滤器
    QList<QLineEdit*> allLineEdits = this->findChildren<QLineEdit*>();
    for (QLineEdit *edit : allLineEdits) {
        edit->installEventFilter(this);
    }
}



/*********************************历史记录**********************/

void MainWindow::connectRecordSignals()
{
    /**
     * @brief 连接界面控件到操作记录器的记录信号
     *
     * 遍历页面中的滑块、按钮、工具按钮等控件，连接其产生的用户操作信号
     * 到 `OperationRecorder` 的记录函数，从而实现全局操作记录与审计。
     */
    // 连接所有TechSliderEdit的记录信号
    QList<TechSliderEdit*> allSliders = this->findChildren<TechSliderEdit*>();
    for (TechSliderEdit* slider : allSliders) {
        // 获取控件所在的页面
        QWidget *page = qobject_cast<QWidget*>(slider->parent());
        int pageIndex = -1;

        // 查找控件属于哪个StackedWidget页面
        while (page && page != this) {
            QStackedWidget *stack = qobject_cast<QStackedWidget*>(page->parent());
            if (stack) {
                pageIndex = stack->indexOf(page);
                break;
            }
            page = page->parentWidget();
        }

        if (pageIndex != -1) {
            // 使用lambda捕获页面信息
            connect(slider, &TechSliderEdit::valueChangedWithRecord,
                    this, [this, slider, pageIndex](double /*oldValue*/, double newValue) {
                        OperationRecord record;
                        const QString labelTextRaw = slider->labelText().trimmed();
                        const QString sliderLabelText = labelTextRaw.isEmpty()
                            ? MappingConfig::instance()->mapControlName(slider->objectName())
                            : labelTextRaw;
                        const QString changeSource = slider->lastChangeSource().trimmed();
                        const bool isPresetClick = (changeSource == QStringLiteral("低速")
                                                 || changeSource == QStringLiteral("中速")
                                                 || changeSource == QStringLiteral("高速"));
                        const QString changedTo = isPresetClick
                            ? changeSource
                            : QString::number(newValue, 'f', slider->precision());

                        record.timestamp = QDateTime::currentDateTime();
                        record.pageName = MappingConfig::instance()->mapPageName(QString::number(pageIndex));
                        record.controlName = QStringLiteral("“%1”被设置为%2").arg(sliderLabelText, changedTo);
                        record.controlType = MappingConfig::instance()->mapControlType("TechSliderEdit");
                        record.operation = "";
                        record.oldValue = "";
                        record.newValue = "";

                        m_recorder->addRecord(record);
                    });
        }
    }
    // 连接所有TechPushButton的记录信号
    QList<TechPushButton*> allButtons = this->findChildren<TechPushButton*>();
    for (TechPushButton* button : allButtons) {
        // 驻车按钮：成功/超时由 onAGVParkBtnClicked 单独写入简短中文，避免与全局点击记录重复。
        if (button->objectName() == QStringLiteral("techBtn_AGV_Park")) {
            continue;
        }
        connect(button, &TechPushButton::clicked,
                this, [this, button]() {
                    // 获取按钮所在页面
                    QWidget *page = qobject_cast<QWidget*>(button->parent());
                    int pageIndex = -1;

                    while (page && page != this) {
                        QStackedWidget *stack = qobject_cast<QStackedWidget*>(page->parent());
                        if (stack) {
                            pageIndex = stack->indexOf(page);
                            break;
                        }
                        page = page->parentWidget();
                    }

                    const QString buttonText = button->text().trimmed();
                    const QString detailText = buttonText.isEmpty()
                        ? button->objectName()
                        : buttonText;
                    const QString groupTitle = findNearestGroupTitle(button);

                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = MappingConfig::instance()->mapPageName(QString::number(pageIndex));
                    if (isInsideSteeringModeSelector(button)) {
                        record.controlName = QStringLiteral("转向模式切换为：“%1”").arg(detailText);
                    } else {
                        record.controlName = groupTitle.isEmpty()
                            ? QStringLiteral("切换到“%1”").arg(detailText)
                            : QStringLiteral("“%1”切换到了“%2”").arg(groupTitle, detailText);
                    }
                    record.controlType = MappingConfig::instance()->mapControlType("TechPushButton");
                    record.operation = MappingConfig::instance()->mapOperation("clicked");
                    record.oldValue = "";
                    record.newValue = MappingConfig::instance()->mapValue(button->text());

                    m_recorder->addRecord(record);
                });
    }
    // 连接所有QToolButton的记录信号
    QList<QToolButton*> allToolButtons = this->findChildren<QToolButton*>();
    for (QToolButton* toolButton : allToolButtons) {
        // 跳过记录页面和管理员页面的按钮，避免记录操作记录的操作
        if (toolButton->objectName().contains("record", Qt::CaseInsensitive) ||
            toolButton->objectName().contains("admin", Qt::CaseInsensitive)) {
            continue;
        }

        // 记录点击事件
        connect(toolButton, &QToolButton::clicked,
                this, [this, toolButton]() {
                    // 获取按钮所在页面
                    QString pageName = getControlPageName(toolButton);

                    const QString buttonText = toolButton->text().trimmed();
                    const QString tooltipText = toolButton->toolTip().trimmed();
                    const QString detailText = !buttonText.isEmpty()
                        ? buttonText
                        : (!tooltipText.isEmpty() ? tooltipText : toolButton->objectName());
                    const QString groupTitle = findNearestGroupTitle(toolButton);

                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = MappingConfig::instance()->mapPageName(pageName);
                    record.controlName = groupTitle.isEmpty()
                        ? QStringLiteral("切换到“%1”").arg(detailText)
                        : QStringLiteral("“%1”切换到了“%2”").arg(groupTitle, detailText);
                    record.controlType = MappingConfig::instance()->mapControlType("QToolButton");
                    record.operation = MappingConfig::instance()->mapOperation("clicked");
                    record.oldValue = "";
                    record.newValue = MappingConfig::instance()->mapValue(toolButton->text().isEmpty() ? toolButton->toolTip() : toolButton->text());

                    m_recorder->addRecord(record);

                    // 在状态栏显示通知
                    showNotification(QString("工具按钮点击: %1").arg(record.controlName));
                });

    }
}

void MainWindow::setupRecordUI()
{
    if (!isBigFeatureEnabled("operation_records")) {
        qCDebug(lcMainWindow) << "操作记录功能已关闭，跳过记录UI初始化";
        return;
    }

    QWidget *recordPage = ui->page_HistoryRecord;
    if (!recordPage) {
        qCWarning(lcMainWindow) << "未找到 page_HistoryRecord，跳过记录页面初始化";
        return;
    }

    QQuickWidget *historyQuick = ui->quickWidget_HistoryList;
    if (!historyQuick) {
        historyQuick = new QQuickWidget(recordPage);
        historyQuick->setObjectName("quickWidget_HistoryList");
        historyQuick->setGeometry(20, 12, 1180, 618);
    }

    m_historyListQml = historyQuick;
    m_historyListQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    connect(m_historyListQml, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
        if (status == QQuickWidget::Ready) {
            updateHistoryListRuntimeDisplay();
        }
        if (status == QQuickWidget::Error && m_historyListQml) {
            const auto errs = m_historyListQml->errors();
            for (const auto &err : errs) {
                qWarning() << "HistoryList QML error:" << err.toString();
            }
        }
    });
    m_historyListQml->setSource(QUrl("qrc:/HistoryList.qml"));
    m_historyListQml->setClearColor(Qt::transparent);

    // 连接信号
    connect(m_recorder, &OperationRecorder::recordAdded, this, [this](const OperationRecord &record) {
        if (m_historyListQml && m_historyListQml->rootObject()) {
            QMetaObject::invokeMethod(m_historyListQml->rootObject(), "addRecord",
                Q_ARG(QVariant, record.timestamp.toString("hh:mm:ss")),
                Q_ARG(QVariant, record.pageName),
                Q_ARG(QVariant, record.controlName),
                Q_ARG(QVariant, record.operation),
                Q_ARG(QVariant, record.oldValue.toString()),
                Q_ARG(QVariant, record.newValue.toString()),
                Q_ARG(QVariant, record.controlType));
        }
    });

    connect(m_recorder, &OperationRecorder::recordsCleared, this, [this]() {
        if (m_historyListQml && m_historyListQml->rootObject()) {
            QMetaObject::invokeMethod(m_historyListQml->rootObject(), "clearRecords");
        }
    });

    qCDebug(lcMainWindow) << "QML 操作记录列表初始化完成";
}

QString MainWindow::formatUptimeSeconds(qint64 sec)
{
    if (sec < 0) {
        sec = 0;
    }
    const qint64 days = sec / 86400;
    const int h = static_cast<int>((sec % 86400) / 3600);
    const int m = static_cast<int>((sec % 3600) / 60);
    const int s = static_cast<int>(sec % 60);
    if (days > 0) {
        return QStringLiteral("%1天 %2:%3:%4")
            .arg(days)
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

void MainWindow::loadPersistedDeviceTotalRuntime()
{
    m_persistedTotalRuntimeSec = 0;
    m_lastSavedTotalRuntimeSec = -1;
    m_runtimeBaselineReady = false;
}

void MainWindow::savePersistedDeviceTotalRuntime()
{
    if (!m_appSessionUptimeTimer.isValid() || !m_runtimeBaselineReady) {
        return;
    }
    const qint64 sessionSec = m_appSessionUptimeTimer.elapsed() / 1000;
    const qint64 totalSecRaw = m_persistedTotalRuntimeSec + sessionSec;
    const qint64 totalSec = qBound<qint64>(0, totalSecRaw, 0xFFFFFFFFLL);
    if (totalSec == m_lastSavedTotalRuntimeSec) {
        return;
    }
    if (!m_modbusManager || !m_modbusManager->isConnected()) {
        return;
    }

    const quint32 packed = static_cast<quint32>(totalSec);
    QVector<quint16> words;
    words.reserve(2);
    words.append(static_cast<quint16>(packed & 0xFFFF));         // 8193: low word
    words.append(static_cast<quint16>((packed >> 16) & 0xFFFF)); // 8194: high word
    if (m_modbusManager->writeMultipleRegisters(kRuntimePersistRegister, words)) {
        m_lastSavedTotalRuntimeSec = totalSec;
    }
}

void MainWindow::updateHistoryListRuntimeDisplay()
{
    if (!m_appSessionUptimeTimer.isValid()) {
        return;
    }
    const qint64 sessionSec = m_appSessionUptimeTimer.elapsed() / 1000;
    const qint64 totalSec = m_persistedTotalRuntimeSec + sessionSec;

    if (m_historyListQml && m_historyListQml->rootObject()) {
        const QString sessionText = formatUptimeSeconds(sessionSec);
        const QString totalText = formatUptimeSeconds(totalSec);
        QMetaObject::invokeMethod(m_historyListQml->rootObject(), "setRuntimeTexts",
                                  Q_ARG(QVariant, sessionText),
                                  Q_ARG(QVariant, totalText));
    }

    savePersistedDeviceTotalRuntime();
}

void MainWindow::updateRecordDisplay()
{
    // QML 列表会自动通过信号更新，无需主循环刷新
}

void MainWindow::onClearRecords()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认", "确定要清空所有操作记录吗？",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_recorder->clear();
    }
}

void MainWindow::onSaveRecords()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "保存操作记录", "operation_history.json", "JSON文件 (*.json)");

    if (!filename.isEmpty()) {
        if (m_recorder->saveToFile(filename)) {
            showToast(QStringLiteral("操作记录已保存"), ToastKind::Success);
        } else {
            showToast(QStringLiteral("保存失败"), ToastKind::Warning);
        }
    }
}

void MainWindow::onExportRecords()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "导出操作报告", "operation_report.txt", "文本文件 (*.txt)");

    if (!filename.isEmpty()) {
        if (m_recorder->exportToText(filename)) {
            showToast(QStringLiteral("操作报告已导出"), ToastKind::Success);
        } else {
            showToast(QStringLiteral("导出失败"), ToastKind::Warning);
        }
    }
}

void MainWindow::onFilterRecords()
{
    if (!isFeatureEnabled("operation_records", "records.filter_export")) {
        showNotification("记录筛选功能已关闭");
        return;
    }

    QComboBox *filterCombo = findChild<QComboBox*>();
    if (!filterCombo) return;

    QString filter = filterCombo->currentText();
    QPlainTextEdit *display = findChild<QPlainTextEdit*>("recordDisplay");

    if (!display) return;

    display->clear();
    const QList<OperationRecord> &records = m_recorder->records();

    if (filter == "显示全部") {
        for (const auto &record : records) {
            display->appendPlainText(record.toString());
        }
    } else if (m_pageNames.values().contains(filter)) {
        QList<OperationRecord> pageRecords = m_recorder->getPageRecords(filter);
        for (const auto &record : pageRecords) {
            display->appendPlainText(record.toString());
        }
    } else if (filter == "TechSliderEdit操作") {
        const QString sliderTypeDisplay = MappingConfig::instance()->mapControlType(QStringLiteral("TechSliderEdit"));
        for (const auto &record : records) {
            if (record.controlType == QStringLiteral("TechSliderEdit") || record.controlType == sliderTypeDisplay) {
                display->appendPlainText(record.toString());
            }
        }
    } else if (filter == "TechPushButton操作") {
        const QString pushTypeDisplay = MappingConfig::instance()->mapControlType(QStringLiteral("TechPushButton"));
        for (const auto &record : records) {
            if (record.controlType == QStringLiteral("TechPushButton") || record.controlType == pushTypeDisplay) {
                display->appendPlainText(record.toString());
            }
        }
    }
}
// 辅助函数：获取控件所在的页面名称
QString MainWindow::getControlPageName(QWidget *widget)
{
    QWidget *parent = widget->parentWidget();

    while (parent && parent != this) {
        QStackedWidget *stack = qobject_cast<QStackedWidget*>(parent);
        if (stack) {
            int pageIndex = stack->currentIndex();
            return m_pageNames.value(pageIndex, QString("页面%1").arg(pageIndex + 1));
        }

        // 如果父控件是StackedWidget的页面
        if (parent->parentWidget()) {
            QStackedWidget *stack2 = qobject_cast<QStackedWidget*>(parent->parentWidget());
            if (stack2) {
                int pageIndex = stack2->indexOf(parent);
                return m_pageNames.value(pageIndex, QString("页面%1").arg(pageIndex + 1));
            }
        }

        parent = parent->parentWidget();
    }

    return "未知页面";
}
/***************************************管理员界面**********************************/
void MainWindow::setupAdminPasswordPage()
{
    // 权限验证页面：使用 .ui 中页面，便于 Qt Creator 可视化编辑
    QWidget *adminPage = ui->page_Permission;
    if (!adminPage) {
        qCWarning(lcMainWindow) << "未找到 page_Permission，跳过权限页面初始化";
        return;
    }

    QWidget *contentHost = adminPage->findChild<QWidget*>("permissionContentHost");
    QWidget *targetParent = contentHost ? contentHost : adminPage;

    // 隐藏权限页中非宿主控件，避免与动态内容重叠
    const auto directChildren = adminPage->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : directChildren) {
        if (child && child != targetParent) {
            child->setVisible(false);
        }
    }

    // 清理旧布局，支持重复初始化
    if (targetParent->layout()) {
        QLayoutItem *item;
        while ((item = targetParent->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete targetParent->layout();
    }

    // 创建科技感背景
    QVBoxLayout *mainLayout = new QVBoxLayout(targetParent);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 背景容器
    QWidget *container = new QWidget(targetParent);
    container->setObjectName("adminContainer");
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setAlignment(Qt::AlignCenter);

    // 标题
    QLabel *titleLabel = new QLabel("请先选择权限", container);
    titleLabel->setObjectName("adminTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    // 角色选择下拉框
    QComboBox *roleComboBox = new QComboBox(container);
    roleComboBox->setObjectName("roleComboBox");
    roleComboBox->addItem("操作员 (Operator)", QVariant::fromValue(static_cast<int>(UserRole::Operator)));
    roleComboBox->addItem("工程师 (Engineer)", QVariant::fromValue(static_cast<int>(UserRole::Engineer)));
    roleComboBox->addItem("管理员 (Admin)", QVariant::fromValue(static_cast<int>(UserRole::Admin)));
    roleComboBox->addItem("厂家 (Manufacturer)", QVariant::fromValue(static_cast<int>(UserRole::Manufacturer)));
    // QComboBox 没有 setAlignment 方法，移除该行

    // 密码输入框
    QLineEdit *passwordEdit = new QLineEdit(container);
    passwordEdit->setObjectName("passwordEdit");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText("请输入密码");
    passwordEdit->setAlignment(Qt::AlignCenter);

    // 密码提示
    QLabel *hintLabel = new QLabel("工程师: 456 | 管理员: 123 | 厂家: 8888", container);
    hintLabel->setObjectName("passwordHint");
    hintLabel->setAlignment(Qt::AlignCenter);

    // 登录按钮
    QPushButton *loginButton = new QPushButton("登录", container);
    loginButton->setObjectName("loginButton");

    // 功能开关管理按钮 (仅厂家)
    QPushButton *featureButton = new QPushButton("功能开关管理", container);
    featureButton->setObjectName("featureButton");
    featureButton->setStyleSheet(
        "background-color: #55007f; color: #ffaa00; font-weight: bold; border: 2px solid #ffaa00;"
    );
    featureButton->setVisible(false);

    // 注销按钮
    QPushButton *logoutButton = new QPushButton("注销 (返回操作员)", container);
    logoutButton->setObjectName("logoutButton");
    logoutButton->setVisible(false); // 默认隐藏，登录后显示

    // 负载阈值配置（管理员登录后可见）
    QWidget *weightThresholdSection = new QWidget(container);
    weightThresholdSection->setObjectName(QStringLiteral("weightThresholdSection"));
    QVBoxLayout *weightMainLayout = new QVBoxLayout(weightThresholdSection);
    weightMainLayout->setContentsMargins(0, 0, 0, 0);
    weightMainLayout->setSpacing(10);

    const QString weightLabelStyle = QStringLiteral(
        "color: #00ffff; font-family: 'Microsoft YaHei UI'; font-size: 14px;");
    const QString weightEditStyle = QStringLiteral(
        "QLineEdit { background: rgba(0, 0, 0, 100); border: 1px solid #00c8ff;"
        " color: #ffaa00; border-radius: 4px; padding: 6px 10px; min-width: 200px; }");

    QLabel *overloadLimitLabel = new QLabel(QStringLiteral("负载超限阈值"), weightThresholdSection);
    overloadLimitLabel->setStyleSheet(weightLabelStyle);
    QLineEdit *weightOverloadLimitEdit = new QLineEdit(weightThresholdSection);
    weightOverloadLimitEdit->setObjectName(QStringLiteral("weightOverloadLimitEdit"));
    weightOverloadLimitEdit->setAlignment(Qt::AlignCenter);
    weightOverloadLimitEdit->setStyleSheet(weightEditStyle);
    m_weightOverloadLimitEdit = weightOverloadLimitEdit;

    QLabel *lockLimitLabel = new QLabel(QStringLiteral("负载超重阈值"), weightThresholdSection);
    lockLimitLabel->setStyleSheet(weightLabelStyle);
    QLineEdit *weightLockLimitEdit = new QLineEdit(weightThresholdSection);
    weightLockLimitEdit->setObjectName(QStringLiteral("weightLockLimitEdit"));
    weightLockLimitEdit->setAlignment(Qt::AlignCenter);
    weightLockLimitEdit->setStyleSheet(weightEditStyle);
    m_weightLockLimitEdit = weightLockLimitEdit;

    weightMainLayout->addWidget(overloadLimitLabel);
    weightMainLayout->addWidget(weightOverloadLimitEdit);
    weightMainLayout->addSpacing(8);
    weightMainLayout->addWidget(lockLimitLabel);
    weightMainLayout->addWidget(weightLockLimitEdit);
    weightThresholdSection->setVisible(false);

    // 错误提示
    QLabel *errorLabel = new QLabel("", container);
    errorLabel->setObjectName("errorLabel");
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setVisible(false);

    // 添加控件到容器
    containerLayout->addStretch(2);
    containerLayout->addWidget(titleLabel);
    containerLayout->addSpacing(30);
    containerLayout->addWidget(roleComboBox);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(passwordEdit);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(hintLabel);
    containerLayout->addSpacing(30);
    containerLayout->addWidget(loginButton);
    containerLayout->addWidget(featureButton);
    containerLayout->addWidget(weightThresholdSection); // 管理员登录后可见
    containerLayout->addWidget(logoutButton);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(errorLabel);
    containerLayout->addStretch(3);

    // 设置容器大小和居中
    container->setFixedSize(480, 480);

    // 添加容器到主布局
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->addStretch();
    centerLayout->addWidget(container);
    centerLayout->addStretch();

    mainLayout->addLayout(centerLayout);

    // 设置样式
    QString style = QString(
        "#page_Permission {"
        "    background-color: #0a0a1a;"
        "}"
        "#adminContainer {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 rgba(20, 20, 40, 220),"
        "        stop:1 rgba(40, 20, 60, 200));"
        "    border: 2px solid #00c8ff;"
        "    border-radius: 15px;"
        "    padding: 30px;"
        "}"
        "#adminTitle {"
        "    font-family: 'Microsoft YaHei UI';"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    color: #00ffff;"
        "    padding: 10px;"
        "}"
        "#passwordEdit {"
        "    background-color: rgba(10, 10, 30, 180);"
        "    border: 2px solid #00c8ff;"
        "    border-radius: 8px;"
        "    padding: 12px 20px;"
        "    font-size: 16px;"
        "    color: #00ffff;"
        "    selection-background-color: #00c8ff;"
        "    min-width: 250px;"
        "}"
        "#roleComboBox {"
        "    background-color: rgba(10, 10, 30, 180);"
        "    border: 2px solid #00c8ff;"
        "    border-radius: 8px;"
        "    padding: 8px 15px;"
        "    font-size: 16px;"
        "    color: #00ffff;"
        "    min-width: 250px;"
        "}"
        "#roleComboBox::drop-down {"
        "    border: none;"
        "}"
        "#roleComboBox QAbstractItemView {"
        "    background-color: rgba(20, 20, 40, 240);"
        "    color: #00ffff;"
        "    selection-background-color: #0088ff;"
        "}"
        "#passwordEdit:focus {"
        "    border-color: #00ffff;"
        "}"
        "#passwordHint {"
        "    color: #8888ff;"
        "    font-size: 12px;"
        "    font-style: italic;"
        "}"
        "#loginButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #00c8ff,"
        "        stop:1 #0088ff);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 30px;"
        "    color: white;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    min-width: 150px;"
        "}"
        "#loginButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #00ffff,"
        "        stop:1 #00aaff);"
        "}"
        "#loginButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #0088cc,"
        "        stop:1 #0066aa);"
        "}"
        "#logoutButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #ff5555,"
        "        stop:1 #cc0000);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 30px;"
        "    color: white;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    min-width: 150px;"
        "}"
        "#logoutButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #ff7777,"
        "        stop:1 #ee2222);"
        "}"
        "#logoutButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #cc0000,"
        "        stop:1 #aa0000);"
        "}"
        "#errorLabel {"
        "    color: #ff5555;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    background-color: rgba(255, 50, 50, 0.1);"
        "    padding: 8px 15px;"
        "    border-radius: 5px;"
        "    border: 1px solid #ff5555;"
        "}"
        );

    // 使整个 adminPage 背景透明（仅 container 保持可见样式），并应用样式表
    adminPage->setAttribute(Qt::WA_TranslucentBackground);
    adminPage->setStyleSheet(style);

    applyWeightThresholdRuntimeSettings();

    connect(weightOverloadLimitEdit, &QLineEdit::editingFinished, this,
            [this, weightOverloadLimitEdit, weightLockLimitEdit]() {
        bool ok = false;
        const QPair<int, int> overloadLim = weightOverloadLimitRangeFromSettings();
        int value = weightOverloadLimitEdit->text().trimmed().toInt(&ok);
        if (!ok) {
            if (ui && ui->statusBar) {
                ui->statusBar->showMessage(QStringLiteral("负载超限阈值无效，请输入整数"), 3000);
            }
            if (g_registerCache.contains(5004)) {
                const QSignalBlocker blocker(weightOverloadLimitEdit);
                weightOverloadLimitEdit->setText(QString::number(g_registerCache.value(5004)));
            }
            return;
        }
        const int rawOverload = value;
        value = qBound(overloadLim.first, value, overloadLim.second);
        if (value != rawOverload) {
            const QSignalBlocker blocker(weightOverloadLimitEdit);
            weightOverloadLimitEdit->setText(QString::number(value));
        }
        bool lockOk = false;
        int lockValue = weightLockLimitEdit->text().trimmed().toInt(&lockOk);
        if (!lockOk && g_registerCache.contains(5005)) {
            lockValue = static_cast<int>(g_registerCache.value(5005));
            lockOk = true;
        }
        if (lockOk && value > lockValue) {
            if (g_registerCache.contains(5004)) {
                const QSignalBlocker blocker(weightOverloadLimitEdit);
                weightOverloadLimitEdit->setText(QString::number(g_registerCache.value(5004)));
            }
            showNotification(QStringLiteral("负载超限阈值不能大于负载超重阈值"));
            return;
        }
        writeToMainDevice(5004, value);
        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = QStringLiteral("权限验证");
            record.controlName = QStringLiteral("负载超限阈值");
            record.controlType = QStringLiteral("AdminConfig");
            record.operation = QStringLiteral("write_register");
            record.oldValue = QString();
            record.newValue = QStringLiteral("已向主控寄存器5004写入%1").arg(value);
            m_recorder->addRecord(record);
        }
        showNotification(QStringLiteral("负载超限阈值已写入主控5004: %1").arg(value));
    });

    connect(weightLockLimitEdit, &QLineEdit::editingFinished, this,
            [this, weightOverloadLimitEdit, weightLockLimitEdit]() {
        bool ok = false;
        const QPair<int, int> lockLim = weightLockLimitRangeFromSettings();
        int value = weightLockLimitEdit->text().trimmed().toInt(&ok);
        if (!ok) {
            if (ui && ui->statusBar) {
                ui->statusBar->showMessage(QStringLiteral("负载超重阈值无效，请输入整数"), 3000);
            }
            if (g_registerCache.contains(5005)) {
                const QSignalBlocker blocker(weightLockLimitEdit);
                weightLockLimitEdit->setText(QString::number(g_registerCache.value(5005)));
            }
            return;
        }
        const int rawLock = value;
        value = qBound(lockLim.first, value, lockLim.second);
        if (value != rawLock) {
            const QSignalBlocker blocker(weightLockLimitEdit);
            weightLockLimitEdit->setText(QString::number(value));
        }
        bool overloadOk = false;
        int overloadValue = weightOverloadLimitEdit->text().trimmed().toInt(&overloadOk);
        if (!overloadOk && g_registerCache.contains(5004)) {
            overloadValue = static_cast<int>(g_registerCache.value(5004));
            overloadOk = true;
        }
        if (overloadOk && value < overloadValue) {
            if (g_registerCache.contains(5005)) {
                const QSignalBlocker blocker(weightLockLimitEdit);
                weightLockLimitEdit->setText(QString::number(g_registerCache.value(5005)));
            }
            showNotification(QStringLiteral("负载超限阈值不能大于负载超重阈值"));
            return;
        }
        writeToMainDevice(5005, value);
        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = QStringLiteral("权限验证");
            record.controlName = QStringLiteral("负载超重阈值");
            record.controlType = QStringLiteral("AdminConfig");
            record.operation = QStringLiteral("write_register");
            record.oldValue = QString();
            record.newValue = QStringLiteral("已向主控寄存器5005写入%1").arg(value);
            m_recorder->addRecord(record);
        }
        showNotification(QStringLiteral("负载超重阈值已写入主控5005: %1").arg(value));
    });

    // 连接登录按钮
    connect(loginButton, &QPushButton::clicked, this, [this, roleComboBox, passwordEdit, errorLabel, titleLabel, loginButton, logoutButton, hintLabel, featureButton, weightThresholdSection]() {
        QString password = passwordEdit->text();
        UserRole selectedRole = static_cast<UserRole>(roleComboBox->currentData().toInt());
        QString roleName = roleComboBox->currentText();

        // 记录登录尝试
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "权限验证";
        record.controlName = "passwordEdit";
        record.controlType = "LoginAttempt";
        record.operation = "login_attempt";
        record.oldValue = "";
        record.newValue = QString("尝试登录 %1: %2").arg(roleName).arg(password.isEmpty() ? "(空)" : "******");
        m_recorder->addRecord(record);

        // 验证明文口令；操作员无需密码
        bool loginSuccess = false;
        if (selectedRole == UserRole::Operator) {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Admin && password == "123") {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Engineer && password == "456") {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Manufacturer && password == "8888") {
            loginSuccess = true;
        }

        if (loginSuccess) {
            m_currentUserRole = selectedRole;
            m_roleSelected = true;
            applyPermissionNavigationGate();

            OperationRecord successRecord;
            successRecord.timestamp = QDateTime::currentDateTime();
            successRecord.pageName = "权限验证";
            successRecord.controlName = "loginButton";
            successRecord.controlType = "LoginSuccess";
            successRecord.operation = "login_success";
            successRecord.oldValue = "";
            successRecord.newValue = selectedRole == UserRole::Operator
                ? QStringLiteral("已选择操作员权限")
                : QString("登录成功: %1").arg(roleName);
            m_recorder->addRecord(successRecord);

            errorLabel->setVisible(false);
            passwordEdit->clear();

            if (selectedRole == UserRole::Operator) {
                titleLabel->setText("权限验证");
                applyPermissionPageLoginState();
                if (ui->StackedWidget) {
                    ui->StackedWidget->setCurrentIndex(0);
                }
                if (ui->TBtn_HomePage) {
                    ui->TBtn_HomePage->setChecked(true);
                }
                updateStatusBarTime();
                showNotification(QStringLiteral("已选择操作员权限"));
                return;
            }

            titleLabel->setText(QString("当前权限: %1").arg(roleName));
            roleComboBox->setVisible(false);
            passwordEdit->setVisible(false);
            hintLabel->setVisible(false);
            loginButton->setVisible(false);
            logoutButton->setVisible(true);
            featureButton->setVisible(m_currentUserRole == UserRole::Manufacturer);
            weightThresholdSection->setVisible(m_currentUserRole == UserRole::Admin);

            updateStatusBarTime();
            showNotification(QString("%1 登录成功").arg(roleName));
        } else {
            errorLabel->setText("密码错误！请重试。");
            errorLabel->setVisible(true);

            OperationRecord failRecord;
            failRecord.timestamp = QDateTime::currentDateTime();
            failRecord.pageName = "权限验证";
            failRecord.controlName = "loginButton";
            failRecord.controlType = "LoginFail";
            failRecord.operation = "login_fail";
            failRecord.oldValue = "";
            failRecord.newValue = QString("登录失败: %1").arg(password.isEmpty() ? "(空)" : "******");
            m_recorder->addRecord(failRecord);

            passwordEdit->clear();
            passwordEdit->setFocus();
        }
    });

    // 连接注销按钮
    connect(logoutButton, &QPushButton::clicked, this, [this, titleLabel, errorLabel, weightThresholdSection]() {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "权限验证";
        record.controlName = "logoutButton";
        record.controlType = "Logout";
        record.operation = "logout";
        record.oldValue = "";
        record.newValue = "注销，返回操作员权限";
        m_recorder->addRecord(record);

        m_currentUserRole = UserRole::Operator;

        titleLabel->setText("权限验证");
        weightThresholdSection->setVisible(false);
        errorLabel->setVisible(false);
        applyPermissionPageLoginState();
        updateStatusBarTime();

        showNotification("已注销，当前为操作员权限");
    });

    // 回车键登录
    connect(passwordEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);

    connect(roleComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (m_currentUserRole == UserRole::Operator) {
            applyPermissionLoginFormForSelectedRole();
        }
    });
    applyPermissionLoginFormForSelectedRole();


    // 当输入时隐藏错误提示
    connect(passwordEdit, &QLineEdit::textChanged, errorLabel, &QLabel::hide);

    // 连接功能开关按钮
    connect(featureButton, &QPushButton::clicked, this, [this]() {
        if (m_currentUserRole == UserRole::Manufacturer) {
            if (!m_featureSwitchWidget) {
                m_featureSwitchWidget = new FeatureSwitchWidget(this);
                connect(m_featureSwitchWidget, &FeatureSwitchWidget::runtimeSettingsChanged,
                        this, [this]() {
                    loadPollingRuntimeSettings();
                    applyPollingRuntimeSettings();
                    applyNetworkRuntimeSettings();
                    loadSliderLabelRuntimeSettings();
                    applySliderLabelRuntimeSettings();
                    applyEstimatedWeightRuntimeSettings();
                    applyModbusAccessSwitches();
                    applySliderEditRuntimeSettings();
                    applyParkOutTriggerLengthRuntimeSettings();
                    applyWeightThresholdRuntimeSettings();
                    applyInclinometerDisplayRuntimeSettings();
                    applyPlaneHeightOffsetRuntimeSettings();
                    applyButtonVisibilityRuntimeSettings();
                    loadSpareButtonNameRegisterSettings();
                    syncSpareButtonNamesFromRegisters();
                    applySpareButtonRuntimeSettings();
                    refreshInterlockingButtonText();
                });
            }
            m_featureSwitchWidget->show();
            m_featureSwitchWidget->raise();
            m_featureSwitchWidget->activateWindow();
        }
    });
}
void MainWindow::showNotification(const QString &message)
{
    if (!userPopupsAllowed()) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (message == m_lastNotificationMessage && (nowMs - m_lastNotificationMs) < 800) {
        return;
    }
    m_lastNotificationMessage = message;
    m_lastNotificationMs = nowMs;

    // 在状态栏显示通知
    if (ui && ui->statusBar) {
        ui->statusBar->showMessage(message, 3000);
    }

    // Wayland下避免触发 requestActivate 警告与高频弹框开销
    const QString platform = QGuiApplication::platformName().toLower();
    if (!platform.contains("wayland")) {
        QToolTip::showText(QCursor::pos(), message, nullptr, QRect(), 2000);
    }
}

void MainWindow::showModbusWriteDisabledToast()
{
    showToast(QStringLiteral("当前操作在目前设备上不被允许"), ToastKind::Warning);
}

void MainWindow::showToast(const QString &message,
                           ToastKind kind,
                           int durationMs,
                           const std::function<void()> &onDismissed)
{
    if (!userPopupsAllowed()) {
        return;
    }
    const QString text = message.trimmed();
    if (text.isEmpty()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (text == m_lastToastMessage && (nowMs - m_lastToastMs) < kToastDuplicateWindowMs) {
        return;
    }
    for (int i = 0; i < m_toasts.size(); ++i) {
        if (m_toasts.at(i).message == text) {
            return;
        }
    }
    m_lastToastMessage = text;
    m_lastToastMs = nowMs;

    const int toastDurationMs = durationMs > 0
                                    ? durationMs
                                    : (kind == ToastKind::Warning
                                           ? kToastWarningDurationMs
                                           : kToastDefaultDurationMs);
    if (ui && ui->statusBar) {
        ui->statusBar->showMessage(text, toastDurationMs);
    }

    while (m_toasts.size() >= kToastMaxCount) {
        dismissToast(m_toasts.first().widget);
    }

    auto *toast = new QWidget(this);
    toast->setObjectName(QStringLiteral("toastWidget"));
    toast->setAttribute(Qt::WA_StyledBackground);
    toast->setFocusPolicy(Qt::NoFocus);
    toast->setStyleSheet(toastStyleSheet(kind));

    auto *layout = new QHBoxLayout(toast);
    layout->setContentsMargins(14, 10, 12, 10);
    layout->setSpacing(12);

    auto *label = new QLabel(text, toast);
    label->setObjectName(QStringLiteral("toastLabel"));
    label->setFocusPolicy(Qt::NoFocus);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(label, 1);

    auto *confirmBtn = new QPushButton(QStringLiteral("确认"), toast);
    confirmBtn->setObjectName(QStringLiteral("toastConfirmBtn"));
    confirmBtn->setAutoDefault(false);
    confirmBtn->setDefault(false);
    confirmBtn->setFocusPolicy(Qt::NoFocus);
    confirmBtn->setFixedSize(76, 34);
    layout->addWidget(confirmBtn, 0, Qt::AlignVCenter);

    const int lineCount = qMax(1, text.count(QLatin1Char('\n')) + 1);
    const int height = qBound(kToastMinHeight, 38 + lineCount * 24, kToastMaxHeight);
    toast->setFixedSize(kToastWidth, height);

    connect(confirmBtn, &QPushButton::clicked, this, [this, toast, onDismissed]() {
        dismissToast(toast);
        if (onDismissed) {
            onDismissed();
        }
    });

    m_toasts.append({toast, nullptr, text});
    updateToastHostVisibility();
}

void MainWindow::dismissToast(QWidget *toast)
{
    if (!toast) {
        return;
    }

    for (int i = 0; i < m_toasts.size(); ++i) {
        if (m_toasts.at(i).widget == toast) {
            if (m_toasts.at(i).timer) {
                m_toasts.at(i).timer->stop();
                m_toasts.at(i).timer->deleteLater();
            }
            m_toasts.removeAt(i);
            break;
        }
    }

    toast->deleteLater();
    updateToastHostVisibility();
}

void MainWindow::dismissToastByMessage(const QString &message)
{
    const QString text = message.trimmed();
    if (text.isEmpty()) {
        return;
    }

    for (int i = m_toasts.size() - 1; i >= 0; --i) {
        if (m_toasts.at(i).message == text) {
            dismissToast(m_toasts.at(i).widget);
        }
    }
}

void MainWindow::dismissOperationHintToasts()
{
    dismissToastByMessage(kRobotZeroSpeedHintText);
    dismissToastByMessage(kAgvZeroSpeedHintText);
    dismissToastByMessage(QStringLiteral("当前未设置步进值"));
    dismissToastByMessage(QStringLiteral("外部按键与当前选中的步进目标不匹配"));
    dismissToastByMessage(QStringLiteral("未选择步进或者点动模式，将自动选择点动模式"));
    dismissToastByMessage(QStringLiteral("未选择坐标或者关节模式，将自动选择关节模式"));
    dismissToastByMessage(ModbusWriteGate::teachingGateUserDialogMessage());
    dismissToastByMessage(kWirelessModeWarningText);
    dismissToastByMessage(QStringLiteral("重心偏高安全风险警告！！！请将立柱高度调整至1000mm以内。"));
    dismissToastByMessage(QStringLiteral("高倾覆风险报警！！！请将伸缩臂长度调整至1000mm以内。"));
}

void MainWindow::ensureToastHost()
{
    // Toasts are positioned individually as MainWindow children so empty stack
    // gaps never block clicks on the underlying UI.
}

void MainWindow::repositionToastHost()
{
    if (m_toasts.isEmpty()) {
        return;
    }

    const QRect area = rect();
    const int x = qMax(area.left(), area.right() - kToastWidth - kToastRightMargin + 1);
    int bottom = area.bottom() - kToastBottomMargin + 1;

    // Newest toast is last in m_toasts and sits closest to the bottom-right.
    for (int i = m_toasts.size() - 1; i >= 0; --i) {
        QWidget *toast = m_toasts.at(i).widget;
        if (!toast) {
            continue;
        }
        const int y = qMax(area.top(), bottom - toast->height());
        toast->move(x, y);
        toast->show();
        toast->raise();
        bottom = y - kToastSpacing;
    }
}

void MainWindow::updateToastHostVisibility()
{
    repositionToastHost();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionToastHost();
}

QString MainWindow::toastStyleSheet(ToastKind kind) const
{
    QString borderColor = QStringLiteral("#4da3ff");
    QString textColor = QStringLiteral("#e8f3ff");
    QString backgroundColor = QStringLiteral("rgba(18, 28, 42, 218)");
    QString buttonTextColor = QStringLiteral("#102030");

    switch (kind) {
    case ToastKind::Success:
        borderColor = QStringLiteral("#5ed98a");
        textColor = QStringLiteral("#e9fff0");
        backgroundColor = QStringLiteral("rgba(18, 52, 30, 218)");
        buttonTextColor = QStringLiteral("#102818");
        break;
    case ToastKind::Warning:
        borderColor = QStringLiteral("#ffb84d");
        textColor = QStringLiteral("#fff3d1");
        backgroundColor = QStringLiteral("rgba(62, 42, 6, 222)");
        buttonTextColor = QStringLiteral("#2f1f00");
        break;
    case ToastKind::Info:
        break;
    }

    return QStringLiteral(
               "#toastWidget {"
               "  background-color: %1;"
               "  border: 1px solid %2;"
               "  border-radius: 9px;"
               "}"
               "#toastLabel {"
               "  color: %3;"
               "  font-size: 16px;"
               "  font-weight: 600;"
               "  line-height: 22px;"
               "  background-color: transparent;"
               "  padding-right: 4px;"
               "}"
               "#toastConfirmBtn {"
               "  background-color: %2;"
               "  color: %4;"
               "  border: none;"
               "  border-radius: 6px;"
               "  padding: 0;"
               "  font-size: 14px;"
               "  font-weight: 600;"
               "  min-width: 76px;"
               "  min-height: 34px;"
               "}"
               "#toastConfirmBtn:pressed {"
               "  background-color: %3;"
               "  color: %4;"
               "}")
        .arg(backgroundColor, borderColor, textColor, buttonTextColor);
}

// 辅助函数：根据记录类型获取颜色
QString MainWindow::getRecordColor(const OperationRecord &record)
{
    if (operationRecordControlTypeMatches(record, QStringLiteral("TechSliderEdit"))) {
        return "#a9d4ff";  // 浅蓝色
    }
    if (operationRecordControlTypeMatches(record, QStringLiteral("TechPushButton"))) {
        return "#ffffff";  // 白色
    }
    if (operationRecordControlTypeMatches(record, QStringLiteral("QToolButton"))) {
        return "#a9d4ff";  // 浅蓝色
    }
    return "#ffffff";  // 默认白色
}

// 辅助函数：检查记录是否应该显示
bool MainWindow::shouldDisplayRecord(const OperationRecord &record, const QString &filter)
{
    if (filter.isEmpty() || filter.contains("显示全部记录")) {
        return true;
    }

    if (filter.contains(QStringLiteral("滑块操作")) && operationRecordControlTypeMatches(record, QStringLiteral("TechSliderEdit"))) {
        return true;
    }

    if (filter.contains(QStringLiteral("按钮操作")) && operationRecordControlTypeMatches(record, QStringLiteral("TechPushButton"))) {
        return true;
    }

    if (filter.contains(QStringLiteral("工具按钮")) && operationRecordControlTypeMatches(record, QStringLiteral("QToolButton"))) {
        return true;
    }

    if (filter.contains(QStringLiteral("登录记录")) &&
        (record.controlType.contains(QStringLiteral("Login"), Qt::CaseInsensitive) ||
         record.controlType.contains(QStringLiteral("登录")) ||
         record.operation.contains(QStringLiteral("login"), Qt::CaseInsensitive) ||
         record.operation.contains(QStringLiteral("登录")))) {
        return true;
    }

    // 检查页面筛选
    for (int i = 0; i < 5; i++) {
        if (m_pageNames.contains(i) && filter.contains(m_pageNames[i])) {
            const QString mappedPage = MappingConfig::instance()->mapPageName(m_pageNames[i]);
            return record.pageName == m_pageNames[i] || record.pageName == mappedPage;
        }
    }

    return false;
}

// 辅助函数：应用样式表
void MainWindow::applyRecordPageStyle(QWidget *recordPage)
{
    QString style = QString(
        "#recordPage {"
        "    background-color: #1a5fb4;"  // 蓝色背景
        "    border: none;"  // 去掉边框
        "}"

        "#recordTitleWidget {"
        "    background: rgba(30, 60, 120, 0.7);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    padding: 10px;"
        "    margin-bottom: 5px;"
        "    border: 1px solid #2d7fda;"
        "}"

        "#recordTitle {"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "    color: #ffffff;"  // 白色文字
        "}"

        "#timeLabel {"
        "    color: #a9d4ff;"  // 浅蓝色文字
        "    font-size: 14px;"
        "}"

        "#controlPanel {"
        "    background: rgba(30, 60, 120, 0.8);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "    padding: 5px;"
        "    margin-bottom: 5px;"
        "}"

        // 按钮样式
        "QPushButton {"
        "    background-color: #2d7fda;"  // 蓝色
        "    color: #ffffff;"  // 白色文字
        "    border: 1px solid #4a9eff;"
        "    border-radius: 4px;"
        "    padding: 6px 12px;"
        "    font-size: 12px;"
        "    min-width: 80px;"
        "}"

        "QPushButton:hover {"
        "    background-color: #4a9eff;"  // 浅蓝色
        "    border-color: #7fbfff;"
        "}"

        "QPushButton:pressed {"
        "    background-color: #1a5fb4;"  // 深蓝色
        "}"

        // 下拉框样式
        "QComboBox {"
        "    background-color: #2d7fda;"  // 蓝色
        "    color: #ffffff;"  // 白色文字
        "    border: 1px solid #4a9eff;"
        "    border-radius: 4px;"
        "    padding: 4px 8px;"
        "    min-width: 120px;"
        "}"

        "QComboBox:hover {"
        "    border-color: #7fbfff;"
        "}"

        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"

        "QComboBox::down-arrow {"
        "    width: 12px;"
        "    height: 12px;"
        "    border-left: 6px solid transparent;"
        "    border-right: 6px solid transparent;"
        "    border-top: 6px solid #ffffff;"
        "}"

        "QComboBox QAbstractItemView {"
        "    background-color: #2d7fda;"
        "    color: #ffffff;"
        "    selection-background-color: #4a9eff;"
        "    border: 1px solid #4a9eff;"
        "}"

        // 标签样式
        "QLabel {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 12px;"
        "}"

        // 记录显示区域
        "#recordDisplay {"
        "    background: rgba(30, 60, 120, 0.6);"  // 半透明深蓝色
        "    color: #ffffff;"  // 白色文字
        "    border: 1px solid #2d7fda;"
        "    border-radius: 8px;"
        "    font-family: 'Segoe UI', Arial, sans-serif;"
        "    font-size: 12px;"
        "    padding: 10px;"
        "    selection-background-color: #4a9eff;"  // 选中文字背景
        "}"

        "#statsPanel {"
        "    background: rgba(30, 60, 120, 0.8);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "    padding: 5px;"
        "    margin-top: 5px;"
        "}"

        // 统计标签样式
        "#totalStats, #todayStats, #sliderStats, #buttonStats, #toolButtonStats {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    padding: 4px 8px;"
        "    background: rgba(45, 127, 218, 0.5);"  // 半透明蓝色
        "    border-radius: 4px;"
        "    border: 1px solid #4a9eff;"
        "}"
        );

    recordPage->setStyleSheet(style);
}

// 辅助函数：根据控件类型获取图标
QString MainWindow::getControlIcon(const QString &controlType)
{
    Q_UNUSED(controlType);
    // if (controlType == "TechSliderEdit") return "🎚️";
    // if (controlType == "TechPushButton") return "🔄";
    // if (controlType == "QToolButton") return "🔧";
    // if (controlType == "QLineEdit") return "⌨️";
    // if (controlType.contains("Login")) return "🔐";
    return "";
}

// mainwindow.cpp 中添加以下函数

// 获取TechSliderLabel的值
// 获取TechSliderLabel的值
double MainWindow::getSliderLabelValue(const QString &labelName)
{
    static const QMap<QString, QString> aliasToArcGauge = {
        {"label_Value1", "robot_ArcGauge_J1Angle"},
        {"label_Value2", "robot_ArcGauge_J2Height"},
        {"label_Value3", "robot_ArcGauge_J3Length"},
        {"label_Value4", "robot_ArcGauge_J4Angle"}
    };

    const QString gaugeKey = aliasToArcGauge.value(labelName, labelName);
    if (m_arcGauges.contains(gaugeKey) && m_arcGauges[gaugeKey]) {
        return m_arcGauges[gaugeKey]->value();
    }

    // 从首页查找对应的label
    if (m_sliderLabelInstances.contains(labelName)) {
        return m_sliderLabelInstances[labelName]->value();  // 修改这里：使用 value() 而不是 currentValue()
    }

    // 如果没有找到，尝试在首页中查找
    QWidget* mainPage = ui->StackedWidget->widget(0);
    if (mainPage) {
        TechSliderLabel* label = mainPage->findChild<TechSliderLabel*>(labelName);
        if (label) {
            return label->value();  // 修改这里：使用 value() 而不是 currentValue()
        }
    }

    return 0.0;
}

// 获取TechSliderEdit的值
double MainWindow::getSliderEditValue(const QString &sliderName)
{
    TechSliderEdit* slider = findChild<TechSliderEdit*>(sliderName);
    if (slider) {
        return slider->value();
    }
    return 0.0;
}

double MainWindow::getAxisCurrentValue(int axisIndex) const
{
    const QString gaugeName = QStringLiteral("robot_ArcGauge_J%1%2")
                                  .arg(axisIndex)
                                  .arg(axisIndex == 2 ? QStringLiteral("Height")
                                                      : (axisIndex == 3 ? QStringLiteral("Length")
                                                                        : QStringLiteral("Angle")));

    if (TechArcGauge *gauge = m_arcGauges.value(gaugeName, nullptr)) {
        return gauge->value();
    }

    switch (axisIndex) {
    case 1: return const_cast<MainWindow *>(this)->getSliderLabelValue("label_Value1");
    case 2: return const_cast<MainWindow *>(this)->getSliderLabelValue("label_Value2");
    case 3: return const_cast<MainWindow *>(this)->getSliderLabelValue("label_Value3");
    case 4: return const_cast<MainWindow *>(this)->getSliderLabelValue("label_Value4");
    default: return 0.0;
    }
}

QString MainWindow::getAxisHistoryName(int axisIndex) const
{
    switch (axisIndex) {
    case 1: return QStringLiteral("悬臂组件(J1)");
    case 2: return QStringLiteral("升降组件(J2)");
    case 3: return QStringLiteral("伸缩臂(J3)");
    case 4: return QStringLiteral("柔顺组件(J4)");
    default: return QStringLiteral("未知轴");
    }
}

QString MainWindow::getAxisHistoryUnit(int axisIndex) const
{
    switch (axisIndex) {
    case 2:
    case 3:
        return QStringLiteral("mm");
    case 1:
    case 4:
        return QStringLiteral("°");
    default:
        return QString();
    }
}

// 记录回转升降页面的操作
void MainWindow::recordVerticalSupportAction(int keyNumber, bool pressed)
{
    QString pageName = "回转升降";

    // 获取当前高度（J2）
    double currentHeight = getAxisCurrentValue(2);
    // 获取移动速度
    double moveSpeed = getSliderEditValue("TechSliderEdit_VeSupSec_MoveSpeed");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = QString("按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "pressed" : "released";
    record.oldValue = "";

    if (keyNumber == 1) {  // ○1 升降下降
        if (pressed) {
            record.newValue = QString("立柱升降当前高度为%1mm，当前以%2mm/s速度下降")
                                  .arg(currentHeight, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("立柱升降当前高度为%1mm，下降完成")
                                  .arg(currentHeight, 0, 'f', 1);
        }
    } else if (keyNumber == 2) {  // ○2 升降上升
        if (pressed) {
            record.newValue = QString("立柱升降当前高度为%1mm，当前以%2mm/s速度上升")
                                  .arg(currentHeight, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("立柱升降当前高度为%1mm，上升完成")
                                  .arg(currentHeight, 0, 'f', 1);
        }
    }

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

// 记录伸缩臂页面的操作
void MainWindow::recordHorizontalSupportAction(int keyNumber, bool pressed)
{
    QString pageName = "伸缩臂";

    // 获取当前角度（J1）
    double currentAngle = getAxisCurrentValue(1);
    // 获取旋转速度
    double rotationSpeed = getSliderEditValue("TechSliderEdit_HoriSupSec_RotationSpeed");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = QString("按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "pressed" : "released";
    record.oldValue = "";

    if (keyNumber == 3) {  // ○3 负方向旋转
        if (pressed) {
            record.newValue = QString("立柱旋转当前角度为%1°，当前以%2°/s速度负方向旋转")
                                  .arg(currentAngle, 0, 'f', 1)
                                  .arg(rotationSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("立柱旋转当前角度为%1°，旋转完成")
                                  .arg(currentAngle, 0, 'f', 1);
        }
    } else if (keyNumber == 4) {  // ○4 正方向旋转
        if (pressed) {
            record.newValue = QString("立柱旋转当前角度为%1°，当前以%2°/s速度正方向旋转")
                                  .arg(currentAngle, 0, 'f', 1)
                                  .arg(rotationSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("立柱旋转当前角度为%1°，旋转完成")
                                  .arg(currentAngle, 0, 'f', 1);
        }
    }

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

void MainWindow::recordHorizontalSupportMoveAction(int keyNumber, bool pressed)
{
    QString pageName = "伸缩臂";

    // 获取当前长度（J3）
    double currentLength = getAxisCurrentValue(3);
    // 获取移动速度
    double moveSpeed = getSliderEditValue("TechSliderEdit_HoriSupSec_MoveSpeed");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = QString("按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "pressed" : "released";
    record.oldValue = "";

    if (keyNumber == 1) {  // ○1 缩短
        if (pressed) {
            record.newValue = QString("伸缩平衡臂当前长度为%1mm，当前以%2mm/s速度缩短")
                                  .arg(currentLength, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("伸缩平衡臂当前长度为%1mm，缩短完成")
                                  .arg(currentLength, 0, 'f', 1);
        }
    } else if (keyNumber == 2) {  // ○2 伸长
        if (pressed) {
            record.newValue = QString("伸缩平衡臂当前长度为%1mm，当前以%2mm/s速度伸长")
                                  .arg(currentLength, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("伸缩平衡臂当前长度为%1mm，伸长完成")
                                  .arg(currentLength, 0, 'f', 1);
        }
    }

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

//连接电池
//矩阵键盘
void MainWindow::onMatrixKeyPressed(int keyNumber, bool pressed)
{
    qCDebug(lcMainWindow) << "【主线程】按键信号接收 - 按键:" << keyNumber
             << "状态:" << (pressed ? "按下" : "释放");

    QString action = pressed ? "按下" : "释放";
    QString message = QString("矩阵按键 ○%1 被%2").arg(keyNumber).arg(action);
    ui->statusBar->showMessage(message, 2000);

    // 处理按键操作
    handleMatrixKeyAction(keyNumber, pressed);
}

QString MainWindow::robotInterlockHintMessage() const
{
    const quint16 status150 = m_mainRegister150Shadow;
    if (((status150 >> 1) & 0x01) == 1) {
        return QStringLiteral("重心偏高安全风险警告！！！请将立柱高度调整至1000mm以内。");
    }
    if (((status150 >> 2) & 0x01) == 1) {
        return QStringLiteral("高倾覆风险报警！！！请将伸缩臂长度调整至1000mm以内。");
    }
    return QString();
}

// 在 handleMatrixKeyAction 函数中修改 ○1 按键的处理
void MainWindow::handleMatrixKeyAction(int keyNumber, bool pressed)
{
    // 获取当前页面
    int currentPage = ui->StackedWidget->currentIndex();
    // 负载超重锁定（150.bit7）仍有效时：拦截全部外部按键
    if (pressed && isRobotWeightLockGateActive()) {
        blockRobotWeightLockOperation(QStringLiteral("负载超重锁定：该外部按键操作已无效"));
        return;
    }
    // 支腿异常（51.bit7=1）时：拦截全部外部按键，并确保异常操作弹窗可见
    if (m_agvLegAbnormal51Bit7Flag) {
        if (pressed) {
            updateParkingLegAbnormalDialogVisibility();
            ui->statusBar->showMessage(QStringLiteral("支腿异常：该外部按键操作已无效"), 3000);
        }
        return;
    }
    // 超载仍有效且用户已点「确认」关窗后：拦截外部按键（首页 ○3/○4 除外），并再次弹出超载提示
    if (pressed && m_robotWeightOverload150Bit3Flag && m_robotWeightOverloadUserAckedWhileActive) {
        const bool firstPageOverloadExempt = (currentPage == 0 && (keyNumber == 3 || keyNumber == 4));
        if (!firstPageOverloadExempt) {
            showRobotWeightOverloadDialog();
            ui->statusBar->showMessage(QStringLiteral("负载超限：该外部按键操作已无效"), 3000);
            return;
        }
    }

    QString pageName = m_pageNames.value(currentPage, "未知");
    const bool isRobotPage = (currentPage == 0 || pageName == "机械臂" || pageName == "page_Robot");
    const bool isSixAxisPage = (currentPage == 3 || pageName == "六自由度" || pageName == "page_SixAxies");
    const bool isAgvPage = (currentPage == 4);

    if (isRobotPage || isSixAxisPage || isAgvPage) {
        const bool stepModeHintShown = maybeShowUnselectedStepModeHintForExternalKey(keyNumber, pressed);
        const bool moveModeHintShown = maybeShowUnselectedMoveModeHintForExternalKey(keyNumber, pressed);
        if (stepModeHintShown || moveModeHintShown) {
            return;
        }
    }

    // 外部按钮逻辑门禁：
    // - 点动模式：仅允许在[关节]模式下执行；
    // - 步进模式：允许执行（由按键映射与目标选择进一步约束）。
    // 释放事件仍继续处理，避免切模后寄存器保持在按下态。
    const bool allowCoordinateExternalOnRobotPage =
        isRobotPage
        && !m_stepModeEnabled
        && !m_moveModeUnknown
        && !m_isJointMode
        && keyNumber >= 1
        && keyNumber <= 8;
    if ((!m_stepModeEnabled && !m_isJointMode) && pressed && !allowCoordinateExternalOnRobotPage) {
        qCDebug(lcMainWindow) << "外部按键忽略：当前未处于可执行模式，按键○" << keyNumber
                 << (pressed ? "按下" : "释放");
        return;
    }

    // 六自由度页面（第4页）外部键逻辑：
    // - ○13/○14：固定写500=5，514=4/2；释放时514回0；
    // - 点动模式：按键○1~○12映射到613写入±1~±6；按下/松开各记一条历史（RX/RY/RZ 角度，X/Y/Z 位置，文案仿步进列表）
    // - 步进模式：按键○1~○12映射轴1~6，写614轴号、601~602(CDAB浮点步进值，奇数键写相反数)，再写615触发。
    if (isSixAxisPage) {
        if (keyNumber == 13 || keyNumber == 14) {
            if (!pressed) {
                writeToMainDevice(514, 0);
                return;
            }

            const int value514 = (keyNumber == 13) ? 4 : 2;
            writeToMainDevice(500, 5);
            writeToMainDevice(514, value514);

            if (m_recorder) {
                const QString msg = (keyNumber == 13)
                                        ? QStringLiteral("正在收回卷样机钢缆")
                                        : QStringLiteral("正在放出卷样机钢缆");
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = getCurrentPageName();
                record.controlName = msg;
                record.controlType = QStringLiteral("MatrixKey");
                record.operation = QString();
                record.oldValue = QString();
                record.newValue = QString();
                m_recorder->addRecord(record);
                showNotification(msg);
            }

            qCDebug(lcMainWindow) << "六自由度页面外部按键○" << keyNumber
                                  << "写入 500=5, 514=" << value514;
            return;
        }

        if (keyNumber < 1 || keyNumber > 12) {
            return;
        }

        if (!m_stepModeUnknown && m_stepModeEnabled) {
            maybeShowUnconfiguredStepValueHintForExternalKey(keyNumber, pressed);
            if (!pressed) {
                m_sixAxisExternalKeyPressed[keyNumber] = false;
                if (m_sixAxisActiveKey == keyNumber) {
                    m_sixAxisActiveKey = -1;
                }
                ++m_sixAxisExternalWriteSeq;

                QLineEdit *sixStepValueEdit = findChild<QLineEdit*>("lineEdit_SixAxies_StepValue");
                if (sixStepValueEdit) {
                    sixStepValueEdit->clear();
                }
                return;
            }

            if (m_sixAxisExternalKeyPressed.value(keyNumber, false)) {
                qCDebug(lcMainWindow) << "六轴步进外部按键去重：按键○" << keyNumber << "重复按下已忽略";
                return;
            }

            QLineEdit *sixStepValueEdit = findChild<QLineEdit*>("lineEdit_SixAxies_StepValue");
            if (!sixStepValueEdit) {
                qCDebug(lcMainWindow) << "六轴步进外部按键忽略：未找到lineEdit_SixAxies_StepValue";
                return;
            }

            bool ok = false;
            double rawStepValue = sixStepValueEdit->text().toDouble(&ok);
            if (!ok) {
                qCDebug(lcMainWindow) << "六轴步进外部按键忽略：步进值无效" << sixStepValueEdit->text();
                return;
            }

            const int axisIndex = (keyNumber + 1) / 2;       // ○1/2->1 ... ○11/12->6

            // 门禁：仅当当前选中的“六轴步进目标轴”与外部按键对应轴一致时，才允许执行步进写入
            int selectedAxisIndex = -1;
            QString selectedTargetText;
            if (m_sixAxisStepTargetGroup && m_sixAxisStepTargetGroup->checkedButton()) {
                const QAbstractButton *checkedBtn = m_sixAxisStepTargetGroup->checkedButton();
                selectedTargetText = checkedBtn->text();
                const QString checkedObjectName = checkedBtn->objectName();
                if (checkedObjectName == "btnStepTargetSixAxis1") selectedAxisIndex = 1;
                else if (checkedObjectName == "btnStepTargetSixAxis2") selectedAxisIndex = 2;
                else if (checkedObjectName == "btnStepTargetSixAxis3") selectedAxisIndex = 3;
                else if (checkedObjectName == "btnStepTargetSixAxis4") selectedAxisIndex = 4;
                else if (checkedObjectName == "btnStepTargetSixAxis5") selectedAxisIndex = 5;
                else if (checkedObjectName == "btnStepTargetSixAxis6") selectedAxisIndex = 6;
            }

            if (selectedAxisIndex != axisIndex) {
                qCDebug(lcMainWindow) << "六轴步进外部按键忽略：按键○" << keyNumber
                                      << "与当前目标轴" << selectedTargetText << "不匹配";
                const QString mismatchTargetName = selectedTargetText.isEmpty()
                                                       ? QStringLiteral("轴%1").arg(axisIndex)
                                                       : selectedTargetText;
                showStepTargetMismatchHintDialog(keyNumber, mismatchTargetName);
                return;
            }

            const bool isOddKey = ((keyNumber % 2) == 1);    // 奇数键写相反数
            if (isOddKey) {
                rawStepValue = -rawStepValue;
            }
            // ○11/○12（轴6）相对其余轴再单独取反
            if (keyNumber == 11 || keyNumber == 12) {
                rawStepValue = -rawStepValue;
            }

            const float stepValueFloat = static_cast<float>(rawStepValue);
            m_sixAxisExternalKeyPressed[keyNumber] = true;
            m_sixAxisActiveKey = keyNumber;
            const quint64 seq = ++m_sixAxisExternalWriteSeq;

            writeToMainDevice(614, axisIndex);

            auto stagedWrite601 = [this, seq, keyNumber, stepValueFloat]() {
                if (seq != m_sixAxisExternalWriteSeq) {
                    return;
                }
                if (m_sixAxisActiveKey != keyNumber) {
                    return;
                }
                if (!m_sixAxisExternalKeyPressed.value(keyNumber, false)) {
                    return;
                }

                const auto regs = floatToRegistersCDAB(stepValueFloat);
                QVector<quint16> values;
                values.reserve(2);
                values << regs[0] << regs[1];
                if (!MainDeviceModbusApi::writeRegisters(m_modbusManager, 601, values)) {
                    writeToMainDevice(601, static_cast<int>(regs[0]));
                    writeToMainDevice(602, static_cast<int>(regs[1]));
                }
            };

            auto stagedWrite615 = [this, seq, keyNumber]() {
                if (seq != m_sixAxisExternalWriteSeq) {
                    return;
                }
                if (m_sixAxisActiveKey != keyNumber) {
                    return;
                }
                if (!m_sixAxisExternalKeyPressed.value(keyNumber, false)) {
                    return;
                }
                writeToMainDevice(615, 1);
            };

            QTimer::singleShot(20, this, stagedWrite601);
            QTimer::singleShot(70, this, stagedWrite601);
            QTimer::singleShot(120, this, stagedWrite615);
            QTimer::singleShot(180, this, stagedWrite615);

            qCDebug(lcMainWindow) << "六轴步进外部按键○" << keyNumber
                                  << "-> 614=" << axisIndex
                                  << "601~602(float CDAB)=" << stepValueFloat
                                  << "615=1";

            QString targetName = selectedTargetText;
            if (targetName.isEmpty()) {
                targetName = QStringLiteral("轴%1").arg(axisIndex);
            }

            double currentValue = 0.0;
            const QString sixAxisGaugeName = QStringLiteral("robot_ArcGauge_SixAxis%1").arg(axisIndex);
            if (TechArcGauge *gauge = m_arcGauges.value(sixAxisGaugeName, nullptr)) {
                currentValue = gauge->value();
            }

            recordStepMoveAction(targetName, currentValue, QString::number(stepValueFloat, 'f', 3), true);
            markStepMotionPendingStop(StepMotionStopKind::SixAxis, targetName, keyNumber);
            ui->statusBar->showMessage(
                QString("步进触发：按键○%1，目标%2，步进值%3")
                    .arg(keyNumber)
                    .arg(targetName)
                    .arg(stepValueFloat, 0, 'f', 3),
                2000);
            return;
        }

        if (m_stepModeUnknown) {
            if (pressed) {
                qCDebug(lcMainWindow) << "六自由度外部按键忽略：当前模式未确定，按键○" << keyNumber;
            }
            return;
        }

        if (!pressed) {
            recordSixAxisJogExternalKey(keyNumber, false);
            writeToMainDevice(613, 0);
            return;
        }

        recordSixAxisJogExternalKey(keyNumber, true);
        const int groupIndex = (keyNumber + 1) / 2;       // ○1/2->1 ... ○11/12->6
        int signedCommand = (keyNumber % 2 == 1) ? -groupIndex : groupIndex;
        // ○11/○12（轴6）相对其余轴再单独取反
        if (keyNumber == 11 || keyNumber == 12) {
            signedCommand = -signedCommand;
        }
        const quint16 encoded = static_cast<quint16>(signedCommand); // 负数按补码写入
        writeToMainDevice(613, static_cast<int>(encoded));

        qCDebug(lcMainWindow) << "六自由度外部按键○" << keyNumber
                              << "-> 地址613写入" << signedCommand
                              << "(补码:" << encoded << ")";
        return;
    }

    // 除第1页（机械臂）、第4页（六自由度）以及第5页（AGV控制）外，其他页面外部按键统一不响应。
    if (!isRobotPage && !isSixAxisPage && !isAgvPage) {
        return;
    }

    // AGV控制页面（第5页）外部按键逻辑：
    // ○13/○14：向500写入5，向514写入4/2 (点动/步进模式下)
    if (isAgvPage) {
        if (keyNumber != 13 && keyNumber != 14) {
            return;
        }

        if (!pressed) {
            writeToMainDevice(514, 0);
            return;
        }

        // 点动、步进模式下执行
        if (!m_moveModeUnknown && (m_isJointMode || m_stepModeEnabled)) {
            const int value514 = (keyNumber == 13) ? 4 : 2;
            
            // 先写目标轴(500=5)，再写动作(514)
            writeToMainDevice(500, 5);
            
            // 延时写514以确保PLC逻辑正确识别
            QTimer::singleShot(50, this, [this, value514]() {
                writeToMainDevice(514, value514);
            });

            qCDebug(lcMainWindow) << "AGV页面外部按键○" << keyNumber
                                  << "写入 500=5, 514=" << value514;
        }
        return;
    }

    // 特殊处理：如果是机械臂页面（索引为0），执行唯一的 500/514 寄存器逻辑并直接返回
    if (isRobotPage) {
        // 将原AGV页面的○1/○2动作迁移为首页上的○9/○10。
        if (keyNumber == 9) {
            handleAGVKeyAction(keyNumber, pressed);
            return;
        }
        if (keyNumber == 10) {
            handleAGVKey2Action(keyNumber, pressed);
            return;
        }

        auto recordRobotExternalMotion = [this](int key, bool isPressed) {
            if (!m_recorder || key < 1 || key > 8) {
                return;
            }

            const int axisIndex = (key - 1) / 2;
            const bool isOddKey = (key % 2) == 1;

            QString componentName;
            QString unit;
            QString oddAction;
            QString evenAction;

            switch (axisIndex) {
            case 0:
                componentName = "悬臂角度";
                unit = "°";
                oddAction = "减小";
                evenAction = "增大";
                break;
            case 1:
                componentName = "升降高度";
                unit = "mm";
                oddAction = "下降";
                evenAction = "上升";
                break;
            case 2:
                componentName = "悬臂长度";
                unit = "mm";
                oddAction = "缩短";
                evenAction = "伸长";
                break;
            case 3:
                componentName = "柔顺角度";
                unit = "°";
                oddAction = "负向旋转";
                evenAction = "正向旋转";
                break;
            default:
                return;
            }

            const double currentValue = getAxisCurrentValue(axisIndex + 1);
            double globalSpeedPercent = getSliderEditValue("TechSliderEdit_Robot_RobotSpeed");
            if (globalSpeedPercent <= 0.0) {
                globalSpeedPercent = getSliderEditValue("TechSliderEdit_EOAT_RotationSpeed");
            }

            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "机械臂";
            record.controlName = QString("外部按键○%1").arg(key);
            record.controlType = "MatrixKey";
            record.operation = isPressed ? "external_motion_start" : "external_motion_stop";
            record.oldValue = "";

            if (isPressed) {
                record.newValue = QString("%1当前为：%2%3，正在以%4%（机器人全局速度）%5")
                                      .arg(componentName)
                                      .arg(currentValue, 0, 'f', 1)
                                      .arg(unit)
                                      .arg(globalSpeedPercent, 0, 'f', 0)
                                      .arg(isOddKey ? oddAction : evenAction);
            } else {
                record.newValue = QString("运动停止，当前%1为%2%3")
                                      .arg(componentName)
                                      .arg(currentValue, 0, 'f', 1)
                                      .arg(unit);
            }

            m_recorder->addRecord(record);
        };

        auto recordRobotExternalCoordinateMotion = [this](int key, bool isPressed) {
            if (!m_recorder || key < 1 || key > 8) {
                return;
            }

            const int axisIndex = (key - 1) / 2;
            const bool isOddKey = (key % 2) == 1;

            QString coordName;
            QString unit;
            QString negativeAction;
            QString positiveAction;
            int regStart = -1;

            switch (axisIndex) {
            case 0:
                coordName = "X";
                unit = "mm";
                negativeAction = "负向";
                positiveAction = "正向";
                regStart = 103;
                break;
            case 1:
                coordName = "Y";
                unit = "mm";
                negativeAction = "负向";
                positiveAction = "正向";
                regStart = 107;
                break;
            case 2:
                coordName = "Z";
                unit = "mm";
                negativeAction = "负向";
                positiveAction = "正向";
                regStart = 111;
                break;
            case 3:
                coordName = "R";
                unit = "°";
                negativeAction = "负向";
                positiveAction = "正向";
                regStart = 115;
                break;
            default:
                return;
            }

            double currentValue = 0.0;
            if (regStart > 0
                && g_registerCache.contains(regStart)
                && g_registerCache.contains(regStart + 1)
                && g_registerCache.contains(regStart + 2)
                && g_registerCache.contains(regStart + 3)) {
                currentValue = registersToDoubleDCBAFEHG(
                    g_registerCache.value(regStart),
                    g_registerCache.value(regStart + 1),
                    g_registerCache.value(regStart + 2),
                    g_registerCache.value(regStart + 3));
            } else if (m_deviceCoordPanelQml && m_deviceCoordPanelQml->rootObject()) {
                const char *propName = (axisIndex == 0) ? "coordX"
                                      : (axisIndex == 1) ? "coordY"
                                      : (axisIndex == 2) ? "coordZ"
                                                         : "coordAr";
                currentValue = m_deviceCoordPanelQml->rootObject()->property(propName).toDouble();
            }

            const double speedPercent = getSliderEditValue("TechSliderEdit_Robot_RobotSpeed");
            const QString directionText = isOddKey ? negativeAction : positiveAction;

            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "机械臂";
            record.controlName = QString("外部按键○%1").arg(key);
            record.controlType = "MatrixKey";
            record.operation = isPressed ? "external_coordinate_start" : "external_coordinate_stop";
            record.oldValue = "";

            if (isPressed) {
                record.newValue = QString("坐标%1当前值为%2%3，当前设置速度为%4%，开始%5运行")
                                      .arg(coordName)
                                      .arg(currentValue, 0, 'f', 2)
                                      .arg(unit)
                                      .arg(speedPercent, 0, 'f', 0)
                                      .arg(directionText);
            } else {
                record.newValue = QString("坐标%1当前值为%2%3，运动结束")
                                      .arg(coordName)
                                      .arg(currentValue, 0, 'f', 2)
                                      .arg(unit);
            }

            m_recorder->addRecord(record);
        };

        if (keyNumber >= 1 && keyNumber <= 8) {
            maybeShowZeroSpeedHintForHomePageExternalKey(keyNumber, pressed);
            if (m_stepModeEnabled) {
                maybeShowUnconfiguredStepValueHintForExternalKey(keyNumber, pressed);
                const int axisIndex = (keyNumber - 1) / 2;   // 0~3
                const bool isOddKey = (keyNumber % 2) == 1;  // 奇数键为反向
                const int keyMappedTargetReg = 500 + axisIndex;

                if (!pressed) {
                    // 需求：步进模式由外部键按下触发，释放不再回写514。
                    m_robotExternalKeyPressed[keyNumber] = false;
                    maybeClearFirstPageStepValueIfAllExternalKeysReleased();
                    return;
                }

                // 同一按键长按/抖动导致的重复按下信号直接忽略，避免重复写入风暴。
                if (m_robotExternalKeyPressed.value(keyNumber, false)) {
                    qCDebug(lcMainWindow) << "步进外部按键去重：按键○" << keyNumber << "重复按下已忽略";
                    return;
                }

                // 步进模式下：仅当外部按键与当前选中目标轴匹配时触发。
                if (selectedStepTargetRegister() != keyMappedTargetReg) {
                    qCDebug(lcMainWindow) << "步进外部按键忽略：按键○" << keyNumber
                                          << "与当前目标" << selectedStepTargetName() << "不匹配";
                    showStepTargetMismatchHintDialog(keyNumber, selectedStepTargetName());
                    return;
                }

                if (!m_stepValueEdit) {
                    qCDebug(lcMainWindow) << "步进外部按键忽略：未找到lineEdit_StepValue";
                    return;
                }

                bool ok = false;
                double stepValue = m_stepValueEdit->text().toDouble(&ok);
                if (!ok) {
                    qCDebug(lcMainWindow) << "步进外部按键忽略：步进值无效" << m_stepValueEdit->text();
                    return;
                }

                // 奇数外部按键：先取相反数再写入。
                if (isOddKey) {
                    stepValue = -stepValue;
                }

                m_robotExternalKeyPressed[keyNumber] = true;
                const quint64 seq = ++m_robotExternalWriteSeq;

                const int targetCode = axisIndex + 1; // 轴1~4 -> 1~4
                writeToMainDevice(500, targetCode);
                writeStepValueDoubleToMainDevice(stepValue);

                auto stagedWrite514 = [this, seq]() {
                    if (seq != m_robotExternalWriteSeq) {
                        return;
                    }
                    writeToMainDevice(514, 1);
                };

                // 先给502~505留出采样窗口，再触发514，降低“值未落稳就触发”的概率。
                QTimer::singleShot(35, this, stagedWrite514);
                QTimer::singleShot(90, this, stagedWrite514);
                QTimer::singleShot(150, this, stagedWrite514);

                qCDebug(lcMainWindow) << "page_Robot (步进) 按键○" << keyNumber
                                      << "目标500=" << targetCode
                                      << "步进值=" << stepValue
                                      << "514=1";

                const QString targetName = selectedStepTargetName();
                double currentValue = 0.0;
                if (keyMappedTargetReg == 500) currentValue = getSliderLabelValue("label_Value1");
                else if (keyMappedTargetReg == 501) currentValue = getSliderLabelValue("label_Value2");
                else if (keyMappedTargetReg == 502) currentValue = getSliderLabelValue("label_Value3");
                else if (keyMappedTargetReg == 503) currentValue = getSliderLabelValue("label_Value4");

                recordStepMoveAction(targetName, currentValue,
                                     QString::number(stepValue, 'f', 3), true);
                markStepMotionPendingStop(StepMotionStopKind::RobotJoint, targetName);
                ui->statusBar->showMessage(
                    QString("步进触发：按键○%1，目标%2，步进值%3")
                        .arg(keyNumber)
                        .arg(targetName)
                        .arg(stepValue, 0, 'f', 3),
                    2000);
                return;
            }

            if (!m_isJointMode && !m_moveModeUnknown) {
                const int groupIndex = (keyNumber + 1) / 2;       // ○1/2->1 ... ○7/8->4
                const int signedCommand = (keyNumber % 2 == 1) ? -groupIndex : groupIndex;
                if (pressed) {
                    writeToMainDevice(526, signedCommand);
                } else {
                    writeToMainDevice(526, 0);
                }

                qCDebug(lcMainWindow) << "page_Robot (Index 0, 坐标) 按键 ○" << keyNumber << " "
                                      << (pressed ? "按下" : "释放")
                                      << " -> 地址526写入:" << (pressed ? signedCommand : 0);

                recordRobotExternalCoordinateMotion(keyNumber, pressed);
                return;
            }

            int value500 = 0;
            int value514 = 0;
            if (pressed) {
                // 地址500逻辑：○1,○2->1; ○3,○4->2; ○5,○6->3; ○7,○8->4
                value500 = (keyNumber + 1) / 2;

                // 地址514逻辑：奇数按键写4，偶数按键写2
                value514 = (keyNumber % 2 != 0) ? 4 : 2;

                // 先写轴组(500)，再延时写方向(514)，提高PLC扫描窗口内命中率。
                m_robotExternalKeyPressed[keyNumber] = true;
                m_robotActiveKey = keyNumber;
                const quint64 seq = ++m_robotExternalWriteSeq;

                writeToMainDevice(500, value500);

                auto stagedWrite514 = [this, keyNumber, value514, seq]() {
                    if (seq != m_robotExternalWriteSeq) {
                        return;
                    }
                    if (m_robotActiveKey != keyNumber) {
                        return;
                    }
                    if (!m_robotExternalKeyPressed.value(keyNumber, false)) {
                        return;
                    }
                    writeToMainDevice(514, value514);
                };

                QTimer::singleShot(25, this, stagedWrite514);
                QTimer::singleShot(90, this, stagedWrite514);
            } else {
                // 松开时的逻辑：500寄存器不再写0，514寄存器写0
                value500 = -1; // 用-1表示不操作
                value514 = 0;
                m_robotExternalKeyPressed[keyNumber] = false;
                if (m_robotActiveKey == keyNumber) {
                    m_robotActiveKey = -1;
                }
                ++m_robotExternalWriteSeq;
                writeToMainDevice(514, 0);
            }

            qCDebug(lcMainWindow) << "page_Robot (Index 0, 点动/关节) 按键 ○" << keyNumber << " " << (pressed ? "按下" : "释放")
                     << " -> 地址500写入:" << (pressed ? QString::number(value500) : "保持") 
                     << ", 地址514写入:" << value514;

            recordRobotExternalMotion(keyNumber, pressed);
        }
        return; // 机械臂页面不再执行后续逻辑
    }

    // 处理124地址的写入
    if (keyNumber >= 1 && keyNumber <= 4) {
        int value = getValueFor124Address(keyNumber, pressed);

        if (value != 0 || !pressed) {  // 按下时写入特定值，释放时写入0
            writeToMainDevice(124, value);
            qCDebug(lcMainWindow) << "" << keyNumber << (pressed ? "按下" : "释放")
                     << "，地址124写入:" << value;
        }
    }

    // 新增：记录回转升降页面的操作
    if (pageName == "回转升降") {
        if (keyNumber == 1 || keyNumber == 2) {
            recordVerticalSupportAction(keyNumber, pressed);
        }
    }

    // 新增：记录伸缩臂页面的操作
    if (pageName == "伸缩臂") {
        if (keyNumber == 1 || keyNumber == 2) {
            // 新增：处理伸缩臂的移动操作（缩短/伸长）
            recordHorizontalSupportMoveAction(keyNumber, pressed);
        } else if (keyNumber == 3 || keyNumber == 4) {
            // 原有：处理伸缩臂的旋转操作
            recordHorizontalSupportAction(keyNumber, pressed);
        }
    }

    // 新增：记录EOAT控制页面的操作
    if (pageName == "EOAT控制") {
        // 获取当前角度（label_Value4）
        double currentAngle = getSliderLabelValue("label_Value4");
        // 获取旋转速度
        double rotationSpeed = getSliderEditValue("TechSliderEdit_EOAT_RotationSpeed");

        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = pageName;
        record.controlName = QString("按键○%1").arg(keyNumber);
        record.controlType = "MatrixKey";
        record.operation = pressed ? "pressed" : "released";
        record.oldValue = "";

        if (keyNumber == 1) {  // ○1 柔顺组件负方向旋转
            if (pressed) {
                record.newValue = QString("柔顺组件当前角度为%1°，当前以%2°/s速度负方向旋转")
                                      .arg(currentAngle, 0, 'f', 1)
                                      .arg(rotationSpeed, 0, 'f', 1);
            } else {
                record.newValue = QString("柔顺组件当前角度为%1°，旋转完成")
                                      .arg(currentAngle, 0, 'f', 1);
            }
        } else if (keyNumber == 2) {  // ○2 柔顺组件正方向旋转
            if (pressed) {
                record.newValue = QString("柔顺组件当前角度为%1°，当前以%2°/s速度正方向旋转")
                                      .arg(currentAngle, 0, 'f', 1)
                                      .arg(rotationSpeed, 0, 'f', 1);
            } else {
                record.newValue = QString("柔顺组件当前角度为%1°，旋转完成")
                                      .arg(currentAngle, 0, 'f', 1);
            }
        }

        if (keyNumber == 1 || keyNumber == 2) {
            m_recorder->addRecord(record);
            showNotification(record.newValue.toString());
        }
    }

}
// 新增：处理AGV页面的按键○1动作
void MainWindow::handleAGVKeyAction(int keyNumber, bool pressed)
{
    if (!ui || !ui->StackedWidget || ui->StackedWidget->currentIndex() != 0) {
        return;
    }

    if (keyNumber != 9) {
        return;
    }

    const bool stepModeHintShown = maybeShowUnselectedStepModeHintForExternalKey(keyNumber, pressed);
    const bool moveModeHintShown = maybeShowUnselectedMoveModeHintForExternalKey(keyNumber, pressed);
    if (stepModeHintShown || moveModeHintShown) {
        return;
    }

    maybeShowZeroSpeedHintForHomePageExternalKey(keyNumber, pressed);

    if (pressed) {
        if (!m_mainRegister150Valid && MainDeviceModbusApi::isReady(m_modbusManager)) {
            MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 150, 1);
        }
        const QString interlockHint = robotInterlockHintMessage();
        if (!interlockHint.isEmpty()) {
            showRobotOperationHintDialog(interlockHint);
            return;
        }
    }

    if (pressed && !m_stepModeEnabled && !m_isJointMode) {
        qCDebug(lcMainWindow) << "首页○9外部按键忽略：当前非关节模式且非步进模式";
        return;
    }

    if (m_stepModeEnabled) {
        maybeShowUnconfiguredStepValueHintForExternalKey(keyNumber, pressed);
        if (!pressed) {
            m_robotExternalKeyPressed[keyNumber] = false;
            maybeClearFirstPageStepValueIfAllExternalKeysReleased();
            return;
        }

        if (selectedStepTargetRegister() != 504) {
            qCDebug(lcMainWindow) << "首页○9(步进)：未选中 AGV 步进目标(btnStepTargetAgv)，忽略";
            showStepTargetMismatchHintDialog(keyNumber, selectedStepTargetName());
            return;
        }

        if (!m_stepValueEdit) {
            qCDebug(lcMainWindow) << "首页○9(步进)：未找到 lineEdit_StepValue，仅写 bit4/bit5";
            writeAGVRegisterBits(0, { qMakePair(4, true) }, QStringLiteral("○9步进按下(1)：寄存器0 bit4=1"));
            writeAGVRegisterBits(0, { qMakePair(5, true) }, QStringLiteral("○9步进按下(3)：寄存器0 bit5=1"));
            appendAgvExternalKeyRecord(keyNumber, pressed);
            markStepMotionPendingStop(StepMotionStopKind::Agv, QStringLiteral("底盘(AGV)"), keyNumber);
            m_robotExternalKeyPressed[keyNumber] = true;
            return;
        }

        bool ok = false;
        const double raw = m_stepValueEdit->text().trimmed().toDouble(&ok);
        if (!ok) {
            qCDebug(lcMainWindow) << "首页○9(步进)：步进值无效" << m_stepValueEdit->text();
            return;
        }
        int stepInt = static_cast<int>(raw);
        stepInt = -stepInt;

        writeAGVRegisterBits(0, { qMakePair(4, true) }, QStringLiteral("○9步进按下(1)：寄存器0 bit4=1"));
        writeToAGVDevice(5, stepInt);
        writeAGVRegisterBits(0, { qMakePair(5, true) }, QStringLiteral("○9步进按下(3)：寄存器0 bit5=1"));
        appendAgvExternalKeyRecord(keyNumber, pressed, m_stepValueEdit->text().trimmed());
        markStepMotionPendingStop(StepMotionStopKind::Agv, QStringLiteral("底盘(AGV)"), keyNumber);
        m_robotExternalKeyPressed[keyNumber] = true;
        return;
    }

    if (m_isJointMode) {
        writeAGVRegisterBits(0,
                             { qMakePair(3, pressed) },
                             pressed ? QStringLiteral("○9点动按下：寄存器0 bit3=1")
                                     : QStringLiteral("○9点动释放：寄存器0 bit3=0"));
        appendAgvExternalKeyRecord(keyNumber, pressed);
        return;
    }

    writeAGVRegisterBits(0,
                         {
                             qMakePair(3, pressed),
                         },
                         pressed ? "○9按下(bit3=1)" : "○9释放(bit3=0)");
    appendAgvExternalKeyRecord(keyNumber, pressed);
}

int MainWindow::getValueFor124Address(int keyNumber, bool pressed)
{
    if (!pressed) {
        // 释放时统一写0
        return 0;
    }

    // 获取当前页面
    int currentPage = ui->StackedWidget->currentIndex();
    QString pageName = m_pageNames.value(currentPage, "未知");

    // 根据按键和页面确定值
    switch (keyNumber) {
    case 1: // 按键○1
        if (pageName == "回转升降") {  // page_VeSupSec
            return -2;
        } else if (pageName == "伸缩臂") {  // page_HoriSupSec
            return -3;
        } else if (pageName == "EOAT控制") {  // page_EOAT
            return -4;
        }
        break;

    case 2: // 按键○2
        if (pageName == "回转升降") {  // page_VeSupSec
            return 2;
        } else if (pageName == "伸缩臂") {  // page_HoriSupSec
            return 3;
        } else if (pageName == "EOAT控制") {  // page_EOAT
            return 4;
        }
        break;

    case 3: // 按键○3
        if (pageName == "伸缩臂") {  // page_HoriSupSec
            return -1;
        }
        break;

    case 4: // 按键○4
        if (pageName == "伸缩臂") {  // page_HoriSupSec
            return 1;
        }
        break;
    }

    // 默认返回0
    return 0;
}


void MainWindow::setupKeyManager()
{
    if (!isFeatureEnabled("input_devices", "input.matrix_key")) {
        qCDebug(lcMainWindow) << "矩阵键输入功能已关闭，跳过键盘管理器";
        return;
    }

    qCDebug(lcMainWindow) << "=== 设置键盘管理器 ===";
    qCDebug(lcMainWindow) << "当前线程:" << QThread::currentThread();

/**
 * @brief 设置 Modbus 线程管理器并完成基本连接配置
 *
 * 包括：获取单例、连接其信号到本窗口槽函数、设置轮询间隔与自动重连。
 * 该函数不会直接处理具体寄存器映射，映射通过 `setupSliderModbusAddresses` 等函数完成。
 */
    // 1. 连接键盘按下信号
    connect(m_keyManager, &MatrixKeyThreadManager::keyPressed,
            this, &MainWindow::onMatrixKeyPressed);

    qCDebug(lcMainWindow) << "信号连接完成";

    // 2. 启动键盘监控
    if (m_keyManager->start("/dev/input/event0")) {
        QString threadInfo;
        QThread* workerThread = m_keyManager->workerThread();
        if (workerThread) {
            QString threadId = QString::number((quintptr)workerThread, 16);
            threadInfo = QString(" [线程:0x%1]").arg(threadId);
        }

        ui->statusBar->showMessage(QString("矩阵按键监控已启动%1").arg(threadInfo), 3000);
        qCDebug(lcMainWindow) << "键盘管理器启动成功";
        qCDebug(lcMainWindow) << "工作线程:" << workerThread;
        qCDebug(lcMainWindow) << "管理器线程:" << m_keyManager->thread();
    } else {
        ui->statusBar->showMessage("错误：无法启动矩阵按键监控！", 5000);
        qWarning() << "键盘管理器启动失败";
    }

    // 删除对 setupThreadMonitorUI() 和 updateThreadStatus() 的调用
}

//modbus TCP
void MainWindow::setupModbusManager()
{
    if (!isBigFeatureEnabled("modbus_main")) {
        qCDebug(lcMainWindow) << "主控Modbus功能已关闭，跳过Modbus管理器";
        return;
    }

    // 获取Modbus管理器实例
    m_modbusManager = ModbusThreadManager::instance();

    qCDebug(lcMainWindow) << "设置Modbus管理器，管理器地址:" << m_modbusManager;

    // 连接信号到槽函数
    connect(m_modbusManager, &ModbusThreadManager::connected,
            this, &MainWindow::onModbusConnected);
    connect(m_modbusManager, &ModbusThreadManager::disconnected,
            this, &MainWindow::onModbusDisconnected);
    connect(m_modbusManager, &ModbusThreadManager::errorOccurred,
            this, &MainWindow::onModbusError);
    connect(m_modbusManager, &ModbusThreadManager::registerValueChanged,
            this, &MainWindow::onModbusRegisterValueChanged,
            Qt::QueuedConnection);

    connect(m_modbusManager, &ModbusThreadManager::writeOperationComplete,
            this,
            [this](bool success, const QString &message) {
                if (success || !ModbusWriteGate::messageIndicatesTeachingGateDenied(message)) {
                    return;
                }
                showTeachingWriteGateDeniedDialog();
            },
            Qt::QueuedConnection);

    qCDebug(lcMainWindow) << "Modbus信号连接完成";

    const MainModbusEndpoint endpoint = MainModbusConnector::selectEndpoint(
        isFeatureEnabled("tcp_transmission", "tcp.local_simulator"),
        isFeatureEnabled("tcp_transmission", "tcp.remote_simulator"),
        m_remoteSimulatorHost);
    if (endpoint.host == "127.0.0.1") {
        qCDebug(lcMainWindow) << "启用本机 TCP 模拟器模式：主设备 ->" << endpoint.host << ":" << endpoint.port;
    } else if (endpoint.port == 5020) {
        qCDebug(lcMainWindow) << "启用远程 TCP 模拟器模式：主设备 ->" << endpoint.host << ":" << endpoint.port;
    }

    bool deviceConnected = MainModbusConnector::connectAndConfigure(
        m_modbusManager, endpoint, m_mainModbusPollIntervalMs, m_mainReconnectIntervalMs);
    qCDebug(lcMainWindow) << "192.168.1.13 Modbus连接状态:" << (deviceConnected ? "已连接" : "连接失败");

    // 显示连接状态消息
    ui->statusBar->showMessage("正在连接192.168.1.13 Modbus...", 3000);

    qCDebug(lcMainWindow) << "192.168.1.13 Modbus管理器设置完成";
}

void MainWindow::setupInterlockingTeachingButton()
{
    if (!ui || !ui->TBtn_Interlocking) {
        return;
    }
    ui->TBtn_Interlocking->setToolTip(QStringLiteral("切换主控%1寄存器：当前显示「上方」时点按写入 0 并切换到「下方」；当前为「下方」时写入 1 并切换到「上方」")
                                           .arg(ModbusWriteGate::interlockRegisterAddress()));
    connect(ui->TBtn_Interlocking, &QToolButton::clicked,
            this, &MainWindow::on_TBtn_Interlocking_clicked);
    if (!m_interlockingSyncTimer) {
        m_interlockingSyncTimer = new QTimer(this);
        connect(m_interlockingSyncTimer, &QTimer::timeout,
                this, &MainWindow::refreshInterlockingButtonText);
    }
}

void MainWindow::refreshInterlockingButtonText()
{
    if (!ui || !ui->TBtn_Interlocking) {
        return;
    }
    ModbusThreadManager *mgr = m_modbusManager ? m_modbusManager : ModbusThreadManager::instance();
    if (!mgr || !mgr->isConnected()) {
        ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
        ui->TBtn_Interlocking->setText(QStringLiteral("--"));
        return;
    }
    if (!isFeatureEnabled("modbus_main", "modbus_main.read_enabled")) {
        ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
        ui->TBtn_Interlocking->setText(QStringLiteral("--"));
        return;
    }
    quint16 v = 0;
    const ButtonModbusMapping::Binding binding = buttonModbusBinding(QStringLiteral("TBtn_Interlocking"));
    const ModbusRegisterSpec spec = !binding.reads.isEmpty()
        ? binding.reads.first()
        : (!binding.writes.isEmpty() ? binding.writes.first() : ModbusRegisterSpec{});
    const int addr = ButtonModbusMapping::addressOr(spec, ModbusWriteGate::interlockRegisterAddress());
    const int upperValue = ButtonModbusMapping::stateValueOr(spec, 1, 1);
    if (!mgr->readSingleRegister(addr, v)) {
        ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
        ui->TBtn_Interlocking->setText(QStringLiteral("--"));
        return;
    }
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(true, v);
    ui->TBtn_Interlocking->setText(static_cast<int>(v) == upperValue
                                      ? QStringLiteral("上方示教器")
                                      : QStringLiteral("下方示教器"));
}

void MainWindow::applyMoveModeUiFromRegister126(quint16 value)
{
    if (!ui || !ui->TBtn_MoveMode) {
        return;
    }
    const ButtonModbusMapping::Binding moveBinding = buttonModbusBinding(QStringLiteral("TBtn_MoveMode"));
    const ModbusRegisterSpec readSpec = moveBinding.reads.isEmpty() ? ModbusRegisterSpec{} : moveBinding.reads.first();
    const int jointValue = ButtonModbusMapping::stateValueOr(readSpec, 1, 2);
    const int coordValue = ButtonModbusMapping::stateValueOr(readSpec, 2, 1);
    if (static_cast<int>(value) == jointValue) {
        m_moveModeUnknown = false;
        m_isJointMode = true;
        ui->TBtn_MoveMode->setText(QStringLiteral("关节模式"));
        QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarMoveModeLabel")) : nullptr;
        if (moveModeLabel) {
            moveModeLabel->setText(QStringLiteral("关节模式"));
            moveModeLabel->setStyleSheet(QStringLiteral("color: #55ff55; font-weight: bold; font-size: 11px;"));
        }
    } else if (static_cast<int>(value) == coordValue) {
        m_moveModeUnknown = false;
        m_isJointMode = false;
        ui->TBtn_MoveMode->setText(QStringLiteral("坐标模式"));
        QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarMoveModeLabel")) : nullptr;
        if (moveModeLabel) {
            moveModeLabel->setText(QStringLiteral("坐标模式"));
            moveModeLabel->setStyleSheet(QStringLiteral("color: #ffaa00; font-weight: bold; font-size: 11px;"));
        }
    } else {
        m_moveModeUnknown = true;
        ui->TBtn_MoveMode->setText(QStringLiteral("未选择模式"));
        QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarMoveModeLabel")) : nullptr;
        if (moveModeLabel) {
            moveModeLabel->setText(QStringLiteral("运动未选择"));
            moveModeLabel->setStyleSheet(QStringLiteral("color: #aaaaaa; font-weight: bold; font-size: 11px;"));
        }
    }
    updateStepTargetButtonsState();
}

void MainWindow::applyRobotSpeedUiFromRegister130(quint16 value)
{
    TechSliderEdit *robotSpeedSlider = findChild<TechSliderEdit*>(QStringLiteral("TechSliderEdit_Robot_RobotSpeed"));
    if (!robotSpeedSlider) {
        return;
    }
    const QSignalBlocker blocker(robotSpeedSlider);
    const double clamped = qBound(robotSpeedSlider->minimum(), static_cast<double>(value), robotSpeedSlider->maximum());
    robotSpeedSlider->setValue(clamped);
}

void MainWindow::applyCachedMainControlSyncRegistersToUi()
{
    if (!ui) {
        return;
    }
    syncStepModeUiByCurrentPage();
    updateStepTargetButtonsState();
    const ButtonModbusMapping::Binding moveBinding = buttonModbusBinding(QStringLiteral("TBtn_MoveMode"));
    const int moveReadAddr = moveBinding.reads.isEmpty()
        ? 126
        : ButtonModbusMapping::addressOr(moveBinding.reads.first(), 126);
    if (g_registerCache.contains(moveReadAddr)) {
        applyMoveModeUiFromRegister126(g_registerCache.value(moveReadAddr));
    }
    if (g_registerCache.contains(130)) {
        applyRobotSpeedUiFromRegister130(g_registerCache.value(130));
    }
    updateFunctionSwitchVisuals();
    updateDeviceCoordPanelFromCache();
}

void MainWindow::on_TBtn_Interlocking_clicked()
{
    if (!ui || !ui->TBtn_Interlocking) {
        return;
    }

    ModbusThreadManager *mgr = m_modbusManager ? m_modbusManager : ModbusThreadManager::instance();
    if (!mgr || !mgr->isConnected()) {
        showNotification(QStringLiteral("主控 Modbus 未连接，无法切换示教器"));
        return;
    }
    if (!isFeatureEnabled("modbus_main", "modbus_main.read_enabled")) {
        showNotification(QStringLiteral("Main Modbus 读功能已关闭，无法读取示教器状态"));
        return;
    }
    if (!isFeatureEnabled("modbus_main", "modbus_main.write_enabled")) {
        showModbusWriteDisabledToast();
        return;
    }

    const QString upper = QStringLiteral("上方示教器");
    const QString lower = QStringLiteral("下方示教器");

    QString label = ui->TBtn_Interlocking->text();
    if (label != upper && label != lower) {
        refreshInterlockingButtonText();
        label = ui->TBtn_Interlocking->text();
        if (label != upper && label != lower) {
            showNotification(QStringLiteral("无法读取示教器状态，请稍后再试"));
            return;
        }
    }

    const ButtonModbusMapping::Binding binding = buttonModbusBinding(QStringLiteral("TBtn_Interlocking"));
    const ModbusRegisterSpec spec = !binding.writes.isEmpty()
        ? binding.writes.first()
        : (!binding.reads.isEmpty() ? binding.reads.first() : ModbusRegisterSpec{});
    const int addr = ButtonModbusMapping::addressOr(spec, ModbusWriteGate::interlockRegisterAddress());
    const int upperValue = ButtonModbusMapping::stateValueOr(spec, 1, 1);
    const int lowerValue = ButtonModbusMapping::stateValueOr(spec, 2, 0);
    quint16 valueToWrite = 0;
    QString nextLabel;
    if (label == upper) {
        valueToWrite = static_cast<quint16>(lowerValue);
        nextLabel = lower;
    } else {
        valueToWrite = static_cast<quint16>(upperValue);
        nextLabel = upper;
    }

    if (!MainDeviceModbusApi::writeRegister(mgr, addr, static_cast<int>(valueToWrite))) {
        showNotification(QStringLiteral("写入联锁寄存器失败"));
        return;
    }
    ui->TBtn_Interlocking->setText(nextLabel);

    applyCachedMainControlSyncRegistersToUi();
    QTimer::singleShot(0, this, [this]() {
        if (MainDeviceModbusApi::isReady(m_modbusManager)) {
            readMainControlSyncRegisters();
            readAllFloatRegisters();
        }
    });
}

// 修改setupSliderModbusAddresses函数，添加对TechSliderLabel的支持
void MainWindow::setupSliderModbusAddresses()
{
    // 为每个TechSliderEdit配置Modbus地址
    QList<TechSliderEdit*> allSliders = this->findChildren<TechSliderEdit*>();
    int baseAddress = 0;

    for (int i = 0; i < allSliders.size(); ++i) {
        TechSliderEdit *slider = allSliders[i];
        QString sliderName = slider->objectName();
        int modbusAddress = baseAddress + i;

        m_modbusManager->registerSlider(slider, modbusAddress);

        qCDebug(lcMainWindow) << "注册slider到Modbus:" << sliderName
                 << "地址:" << modbusAddress << "(对应400"
                 << QString("%1").arg(modbusAddress + 40001, 3, 10, QChar('0')) << ")";
    }

    // 为每个TechSliderLabel配置Modbus地址
    QList<TechSliderLabel*> allSliderLabels = this->findChildren<TechSliderLabel*>();
    int labelBaseAddress = 100;  // 使用不同的地址范围

    for (int i = 0; i < allSliderLabels.size(); ++i) {
        TechSliderLabel *sliderLabel = allSliderLabels[i];
        QString sliderLabelName = sliderLabel->objectName();
        int modbusAddress = labelBaseAddress + i;

        m_modbusManager->registerSliderLabel(sliderLabel, modbusAddress);

        qCDebug(lcMainWindow) << "注册sliderLabel到Modbus:" << sliderLabelName
                 << "地址:" << modbusAddress << "(对应400"
                 << QString("%1").arg(modbusAddress + 40001, 3, 10, QChar('0')) << ")";
    }
}
// Modbus连接成功槽函数
void MainWindow::onModbusConnected()
{
    qCDebug(lcMainWindow) << "Modbus连接成功，启动交互任务...";
    MainModbusStatus::applyUiState(ui ? ui->statusBar : nullptr, MainModbusState::Connected);

    if (!m_runtimeBaselineReady && m_modbusManager
        && isFeatureEnabled("modbus_main", "modbus_main.read_enabled")) {
        quint16 persistedLow = 0;
        quint16 persistedHigh = 0;
        const bool lowOk = m_modbusManager->readSingleRegister(kRuntimePersistRegister, persistedLow);
        const bool highOk = m_modbusManager->readSingleRegister(kRuntimePersistRegisterHi, persistedHigh);
        if (lowOk && highOk) {
            const quint32 packed = (static_cast<quint32>(persistedHigh) << 16) | persistedLow;
            m_persistedTotalRuntimeSec = static_cast<qint64>(packed);
            m_lastSavedTotalRuntimeSec = m_persistedTotalRuntimeSec;
            m_runtimeBaselineReady = true;
            updateHistoryListRuntimeDisplay();
            qCDebug(lcMainWindow) << "已从主控寄存器" << kRuntimePersistRegister
                                  << "~" << kRuntimePersistRegisterHi
                                  << "加载总运行时间(秒):" << m_persistedTotalRuntimeSec;
        } else {
            qCWarning(lcMainWindow) << "读取主控寄存器" << kRuntimePersistRegister
                                    << "~" << kRuntimePersistRegisterHi
                                    << "失败，暂不写入运行时间";
        }
    }

    // 立即启动原本推迟的数据读取子系统
    if (isFeatureEnabled("startup_checks", "startup.write_registers")) {
        performStartupWrites();
    }

    const bool startupFloatReadingEnabled = isFeatureEnabled("modbus_main", "modbus_main.float_reading");
    if (startupFloatReadingEnabled) {
        // 延后批量浮点轮询，优先保障开机模式位读取（125/126）先完成，避免启动阶段请求拥塞。
        QTimer::singleShot(2500, this, [this]() {
            if (MainDeviceModbusApi::isReady(m_modbusManager)) {
                setupModbusFloatReading();
            }
        });
    }

    auto requestStartupModes = [this]() {
        if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
            return;
        }
        // 一次读取 125~126，减少请求数量并保证两个模式位在同一响应链路。
        MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 125, 2);
    };

    // 开机后主动读取模式寄存器，用于同步按钮文本状态（增强重试次数以提高首次命中率）
    for (int i = 0; i < 12; ++i) {
        QTimer::singleShot(i * 300, this, requestStartupModes);
    }

    // 二次补读：应对开机早期网络/设备抖动导致的首轮漏读。
    QTimer::singleShot(4000, this, [this, requestStartupModes]() {
        for (int i = 0; i < 4; ++i) {
            QTimer::singleShot(i * 250, this, requestStartupModes);
        }
    });

    // 持续重试直到125/126都回包或20秒超时，避免开机窗口期漏读。
    auto *modeStartupRetryTimer = new QTimer(this);
    modeStartupRetryTimer->setInterval(400);
    const qint64 startupBeginMs = QDateTime::currentMSecsSinceEpoch();
    connect(modeStartupRetryTimer, &QTimer::timeout, this, [this, modeStartupRetryTimer, startupBeginMs, requestStartupModes]() {
        requestStartupModes();

        const bool has125 = !m_stepModeUnknown;
        const bool has126 = !m_moveModeUnknown;
        if (has125 && has126) {
            modeStartupRetryTimer->stop();
            modeStartupRetryTimer->deleteLater();
            return;
        }

        if (QDateTime::currentMSecsSinceEpoch() - startupBeginMs >= 20000) {
            modeStartupRetryTimer->stop();
            modeStartupRetryTimer->deleteLater();

            OperationRecord timeoutRecord;
            timeoutRecord.timestamp = QDateTime::currentDateTime();
            timeoutRecord.pageName = "系统启动";
            timeoutRecord.controlName = "startup_mode_reader";
            timeoutRecord.controlType = "MainModbus";
            timeoutRecord.operation = "startup_mode_read_timeout";
            timeoutRecord.oldValue = "等待读取125/126";
            timeoutRecord.newValue = QString("20秒超时，步进=%1，运动=%2")
                                         .arg(ui && ui->TBtn_Stepmove ? ui->TBtn_Stepmove->text() : "未知")
                                         .arg(ui && ui->TBtn_MoveMode ? ui->TBtn_MoveMode->text() : "未知");
            m_recorder->addRecord(timeoutRecord);
        }
    });
    modeStartupRetryTimer->start();

    if (m_interlockingSyncTimer) {
        m_interlockingSyncTimer->setInterval(qMax(50, m_mainUiPollIntervalMs));
        m_interlockingSyncTimer->start();
    }
    refreshInterlockingButtonText();

    if (isBigFeatureEnabled("tcp_transmission")) {
        enableTcpTransmission(true);
    }

    MainModbusStatus::appendOperationRecord(m_recorder, MainModbusState::Connected);
}

// Modbus断开连接槽函数
void MainWindow::onModbusDisconnected()
{
    qCDebug(lcMainWindow) << "Modbus设备断开连接";
    ModbusWriteGate::updateOperationHistoryGateFromInterlockRead(false, 0);
    if (m_interlockingSyncTimer) {
        m_interlockingSyncTimer->stop();
    }
    if (ui && ui->TBtn_Interlocking) {
        ui->TBtn_Interlocking->setText(QStringLiteral("--"));
    }
    MainModbusStatus::applyUiState(ui ? ui->statusBar : nullptr, MainModbusState::Disconnected);
    MainModbusStatus::appendOperationRecord(m_recorder, MainModbusState::Disconnected);
}

// Modbus错误槽函数
void MainWindow::onModbusError(const QString &error)
{
    qCDebug(lcMainWindow) << "Modbus错误:" << error;
    MainModbusStatus::applyUiState(ui ? ui->statusBar : nullptr, MainModbusState::Error, error);
    MainModbusStatus::appendOperationRecord(m_recorder, MainModbusState::Error, error);
}
// 速度选择
void MainWindow::initSpeedModeSelector()
{
    if (!isFeatureEnabled("motion_control", "motion.speed_mode")) {
        qCDebug(lcMainWindow) << "速度模式功能已关闭，跳过初始化";
        return;
    }

    // 创建速度模式选择器
    SpeedModeSelector *speedModeSelector = new SpeedModeSelector(this);
    speedModeSelector->setObjectName("speedModeSelector");

    // 设置位置和大小（根据你的UI布局调整）
    // 例如，放在某个布局或widget中
    // ui->verticalLayout->addWidget(speedModeSelector);

    // 设置按钮样式为全息风格
    speedModeSelector->setButtonStyle(TechPushButton::StyleHolographic);

    // 设置自定义颜色
    speedModeSelector->setActiveColor(QColor(0, 200, 255));     // 激活状态颜色
    speedModeSelector->setInactiveColor(QColor(80, 80, 100));   // 非激活状态颜色
    speedModeSelector->setTextColor(Qt::white);                 // 文字颜色

    // 连接模式改变信号
    connect(speedModeSelector, &SpeedModeSelector::modeChanged,
            this, [this](SpeedMode mode) {
                qCDebug(lcMainWindow) << "速度模式改变为:" << mode;

                // 根据模式更新其他UI或执行操作
                switch(mode) {
                case MODE_LOW:
                    // 低速模式操作
                    updateSpeed(50.0);  // 更新仪表盘
                    showNotification("已切换至低速模式");
                    break;
                case MODE_MEDIUM:
                    // 中速模式操作
                    updateSpeed(100.0);
                    showNotification("已切换至中速模式");
                    break;
                case MODE_HIGH:
                    // 高速模式操作
                    updateSpeed(150.0);
                    showNotification("已切换至高速模式");
                    break;
                }

                // 记录操作
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "当前页面";  // 根据实际情况设置
                record.controlName = "speedModeSelector";
                record.controlType = "SpeedModeSelector";
                record.operation = "mode_changed";
                record.oldValue = "";  // 可以记录旧模式
                record.newValue = QString("模式%1").arg(mode);
                m_recorder->addRecord(record);
            });
}
//配置AGV的modbus


// 初始化Modbus变量表
void MainWindow::setupModbusVariables()
{
    if (!m_modbusVariables) {
        m_modbusVariables = new ModbusVariables(this);
    }

    // 如果需要从Excel文件加载
    // m_modbusVariables->loadFromExcel("上位机交互变量表(1).xls");
}

// 设置Modbus地址到Label的映射
void MainWindow::setupModbusLabels()
{
    m_modbusLabels = MainModbusLabelMapper::buildMap(this);

    qCDebug(lcMainWindow) << "找到" << m_modbusLabels.size() << "个Modbus显示Label";
}

// 启动Modbus变量轮询
void MainWindow::startModbusPolling()
{
    if (MainModbusPoller::shouldSkipStart(m_modbusPollTimer)) {
        qCDebug(lcMainWindow) << "主设备通用地址轮询已停用，仅保留四个SliderLabel地址轮询";
        return;
    }

    if (!isFeatureEnabled("modbus_main", "modbus_main.polling")) {
        qCDebug(lcMainWindow) << "主控Modbus轮询功能已关闭";
        return;
    }

    if (!m_modbusManager || !m_modbusManager->isConnected()) {
        qCDebug(lcMainWindow) << "Modbus未连接，无法启动轮询";
        return;
    }

    // 获取所有需要读取的变量
    QList<ModbusVariable> variables = m_modbusVariables->getAllVariables();

    // 添加所有变量到轮询列表
    for (const ModbusVariable &var : variables) {
        int modbusAddress = 0;
        int bitPos = -1;

        if (ModbusVariables::parseAddress(var.address, modbusAddress, bitPos)) {
            // 添加到轮询（这里需要扩展ModbusThreadManager）
            // 暂时先记录需要读取的地址
            qCDebug(lcMainWindow) << "需要读取变量:" << var.name
                     << "地址:" << var.address
                     << "Modbus地址:" << modbusAddress;
        }
    }

    // 启动定时器轮询
    connect(m_modbusPollTimer, &QTimer::timeout, this, &MainWindow::pollModbusVariables);
    m_modbusPollTimer->start(m_mainUiPollIntervalMs);
}

// 轮询Modbus变量
void MainWindow::pollModbusVariables()
{
    if (MainModbusPoller::shouldSkipPoll()) {
        qCDebug(lcMainWindow) << "pollModbusVariables已停用，避免轮询main其他地址";
        return;
    }
    static int currentIndex = 0;
    Q_UNUSED(MainModbusPoller::pollNextVariable(m_modbusManager, m_modbusVariables, currentIndex));
    return;
}

void MainWindow::updateSliderLabelValue(const QString& labelName, float value)
{
    // 根据首页控件名，更新所有相关页面的对应控件
    for (auto it = m_pageSliders.begin(); it != m_pageSliders.end(); ++it) {
        const QVector<TechSliderLabel*>& sliders = it.value();

        for (TechSliderLabel* slider : sliders) {
            if (slider) {
                QString objName = slider->objectName();
                // 检查是否是首页控件或相关副本
                if (objName == labelName || objName.startsWith(labelName + "_")) {
                    slider->setValue(value);
                    // 移除高频日志: qCDebug(lcMainWindow) << "更新" << objName << " = " << value;
                }
            }
        }
    }

    // 平面高度 = J2 升降高度 − 偏移量（偏移量可在功能控制台改，默认 1900 mm）。
    if (labelName == QStringLiteral("robot_ArcGauge_J2Height")) {
        m_lastJ2HeightMm = static_cast<double>(value);
        m_hasLastJ2Height = true;
        const double planeHeight = m_lastJ2HeightMm - m_planeHeightOffsetMm;
        if (ui && ui->label_PlaneHeightValue) {
            ui->label_PlaneHeightValue->setText(
                QStringLiteral("%1\nmm").arg(planeHeight, 0, 'f', 0));
        }
    }

    // 更新对应的环形仪表 (TechArcGauge)
    if (m_arcGauges.contains(labelName)) {
        m_arcGauges[labelName]->setValue(static_cast<double>(value));
        
        // 特殊逻辑：根据不同仪表解析对应的速度寄存器
        if (labelName == "robot_ArcGauge_J3Length") {
            // J3 速度 = (48-51) 的 double + (52-55) 的 double, 范围 0~40
            if (g_registerCache.contains(48) && g_registerCache.contains(51) && 
                g_registerCache.contains(52) && g_registerCache.contains(55)) {
                double v1 = registersToDoubleDCBAFEHG(g_registerCache[48], g_registerCache[49], g_registerCache[50], g_registerCache[51]);
                double v2 = registersToDoubleDCBAFEHG(g_registerCache[52], g_registerCache[53], g_registerCache[54], g_registerCache[55]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v1 + v2));
            }
        } else if (labelName == "robot_ArcGauge_J1Angle") {
            // J1 速度 = 36-39, 范围 0~2
            if (g_registerCache.contains(36) && g_registerCache.contains(39)) {
                double v = registersToDoubleDCBAFEHG(g_registerCache[36], g_registerCache[37], g_registerCache[38], g_registerCache[39]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v));
            }
        } else if (labelName == "robot_ArcGauge_J2Height") {
            // J2 速度 = 40-43, 范围 0~15
            if (g_registerCache.contains(40) && g_registerCache.contains(43)) {
                double v = registersToDoubleDCBAFEHG(g_registerCache[40], g_registerCache[41], g_registerCache[42], g_registerCache[43]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v));
            }
        } else if (labelName == "robot_ArcGauge_J4Angle") {
            // J4 速度 = 56-59, 范围 0~2
            if (g_registerCache.contains(56) && g_registerCache.contains(59)) {
                double v = registersToDoubleDCBAFEHG(g_registerCache[56], g_registerCache[57], g_registerCache[58], g_registerCache[59]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v));
            }
        }
    }
}

// 处理Modbus值变化
// 在mainwindow.cpp中添加这个函数
// 处理Modbus值变化

void MainWindow::onModbusRegisterValueChanged(int address, quint16 value)
{
    if (isFeatureEnabled("modbus_main", "modbus_main.read_logs")) {
        qCDebug(lcMainWindow) << "[Main] 寄存器值变化 - 地址:" << address
                              << "值:" << value
                              << "(0x" << QString::number(value, 16).toUpper() << ")";
    }

    // [调试] 无论如何都会输出，用来确认数据到底回来没
    if (address < 25) { 
        // qWarning() << "[Modbus原始数据] 地址:" << address << "值:" << value;
    }

    // 更新寄存器缓存
    g_registerCache[address] = value;

    const bool allowMainUiStateSync = m_mainUiStateSyncEnabled;

    // 步进/点动：首页用 125、六自由度页用 600；任一处变化都应刷新（syncStepModeUiByCurrentPage 按当前页选源）
    const bool shouldSyncStepMode = (address == 125 || address == 600);

    if (allowMainUiStateSync && shouldSyncStepMode) {
        Q_UNUSED(value);
        syncStepModeUiByCurrentPage();
        updateStepTargetButtonsState();
    }

    if (allowMainUiStateSync && address == 126) {
        applyMoveModeUiFromRegister126(value);
        updateFunctionSwitchVisuals();
    }

    if (allowMainUiStateSync && address == 130) {
        applyRobotSpeedUiFromRegister130(value);
    }

    if (allowMainUiStateSync && address == 5004 && m_weightOverloadLimitEdit) {
        const QPair<int, int> lim = weightOverloadLimitRangeFromSettings();
        const QSignalBlocker blocker(m_weightOverloadLimitEdit);
        m_weightOverloadLimitEdit->setText(QString::number(qBound(lim.first, static_cast<int>(value), lim.second)));
    }

    if (allowMainUiStateSync && address == 5005 && m_weightLockLimitEdit) {
        const QPair<int, int> lim = weightLockLimitRangeFromSettings();
        const QSignalBlocker blocker(m_weightLockLimitEdit);
        m_weightLockLimitEdit->setText(QString::number(qBound(lim.first, static_cast<int>(value), lim.second)));
    }

    if (address == 134) {
        updateRobotTotalPower(value);
    }

    if (address == kMainCurrentLoadWeightReg) {
        updateCurrentLoadWeight(value);
    }

    if (address >= 103 && address <= 118) {
        updateDeviceCoordPanelFromCache();
    }

    const QStringList targetLabels = {
        "robot_ArcGauge_J1Angle", "robot_ArcGauge_J2Height", "robot_ArcGauge_J3Length", "robot_ArcGauge_J4Angle"
    };

    // J1~J4 数值显示应始终跟随主寄存器数据，不受主控/AGV 控件状态同步开关影响。
    for (const QString &labelName : targetLabels) {
        if (!m_sliderLabelConfigs.contains(labelName)) {
            continue;
        }

        const SliderLabelConfig &config = m_sliderLabelConfigs[labelName];
        const QVector<int> regs = {
            config.modbusAddress1,
            config.modbusAddress2,
            config.modbusAddress3,
            config.modbusAddress4
        };

        if (!regs.contains(address)) {
            continue;
        }

        bool ready = true;
        for (int reg : regs) {
            if (!g_registerCache.contains(reg)) {
                ready = false;
                break;
            }
        }

        if (!ready) {
            continue;
        }

        const quint16 reg1 = g_registerCache[config.modbusAddress1];
        const quint16 reg2 = g_registerCache[config.modbusAddress2];
        const quint16 reg3 = g_registerCache[config.modbusAddress3];
        const quint16 reg4 = g_registerCache[config.modbusAddress4];
        double value64 = registersToDoubleDCBAFEHG(reg1, reg2, reg3, reg4);

        // 如果开启了求和模式 (用于 J3Length = 12-15 + 16-19)
        if (config.isSumMode) {
            bool sumReady = true;
            for (int i = 0; i < 4; ++i) {
                if (!g_registerCache.contains(config.sumAddress[i])) {
                    sumReady = false;
                    break;
                }
            }

            if (sumReady) {
                const double sumPart = registersToDoubleDCBAFEHG(
                    g_registerCache[config.sumAddress[0]],
                    g_registerCache[config.sumAddress[1]],
                    g_registerCache[config.sumAddress[2]],
                    g_registerCache[config.sumAddress[3]]
                );
                value64 += sumPart;
            }
        }

        static QMap<QString, int> debugCounter;
        if (debugCounter[labelName]++ % 8 == 0) {
            // qWarning() << "[四控件读数]" << labelName
            //          << (config.isSumMode ? " (求和模式)" : "")
            //          << "解析值:" << value64;
        }

        updateSliderLabelValue(labelName, static_cast<float>(value64));
    }

    // SixAxies：192.168.1.13 的 73~84，两个寄存器一组，按 CDAB 转 float。
    if (address >= 73 && address <= 84) {
        static const struct {
            int highAddr;
            int lowAddr;
            const char* gaugeName;
        } sixAxisRegPairs[] = {
            {73, 74, "robot_ArcGauge_SixAxis1"},
            {75, 76, "robot_ArcGauge_SixAxis2"},
            {77, 78, "robot_ArcGauge_SixAxis3"},
            {79, 80, "robot_ArcGauge_SixAxis4"},
            {81, 82, "robot_ArcGauge_SixAxis5"},
            {83, 84, "robot_ArcGauge_SixAxis6"}
        };

        for (const auto &pair : sixAxisRegPairs) {
            if (address != pair.highAddr && address != pair.lowAddr) {
                continue;
            }
            if (!g_registerCache.contains(pair.highAddr) || !g_registerCache.contains(pair.lowAddr)) {
                continue;
            }

            const quint16 regA = g_registerCache[pair.highAddr];
            const quint16 regB = g_registerCache[pair.lowAddr];
            float axisValue = registersToFloatCDAB(regA, regB);
            const QString gaugeName = QString::fromLatin1(pair.gaugeName);

            // 需求：SixAxis4~6 的显示值放大 1000 倍。
            if (gaugeName == "robot_ArcGauge_SixAxis4"
                || gaugeName == "robot_ArcGauge_SixAxis5"
                || gaugeName == "robot_ArcGauge_SixAxis6") {
                axisValue *= 1000.0f;
            }
            // widget_SixAxies_6（Z 轴）：解析后再取反显示
            if (gaugeName == "robot_ArcGauge_SixAxis6") {
                axisValue = -axisValue;
            }
            updateSliderLabelValue(gaugeName, axisValue);
        }
    }

    // ============ 仅保留主设备150急停报警源 ============
    if (address == 150) {
        m_mainRegister150Shadow = value;
        m_mainRegister150Valid = true;
        // 原约定 value==1；另支持 150.bit4/bit5 示教器急停（与现场寄存器定义一致）
        const bool emergencyStop = (value == 1)
            || (((value >> 4) & 0x03) != 0);
        if (emergencyStop != m_robotArmEmergency150Flag) {
            m_robotArmEmergency150Flag = emergencyStop;
            if (emergencyStop && m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "急停报警";
                record.controlType = "报警监控";
                record.operation = "报警触发";
                record.oldValue = "";
                record.newValue = "机械臂触发了急停报警";
                m_recorder->addRecord(record);
            }
            QTimer::singleShot(0, this, &MainWindow::checkAlarmConditions);
        }

        const bool weightOverload = (((value >> 3) & 0x01) == 1);
        if (weightOverload != m_robotWeightOverload150Bit3Flag) {
            m_robotWeightOverload150Bit3Flag = weightOverload;
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "提示系统";
                record.controlName = "负载超限提示";
                record.controlType = "提示窗口";
                record.operation = weightOverload ? "提示触发" : "提示解除";
                record.oldValue = "";
                record.newValue = weightOverload ? "检测到负载超限" : "负载超限提示已解除";
                m_recorder->addRecord(record);
            }
            if (weightOverload) {
                m_robotWeightOverloadUserAckedWhileActive = false;
                if (!m_robotWeightLock150Bit7Flag) {
                    showRobotWeightOverloadDialog();
                }
            } else {
                hideRobotWeightOverloadDialog();
                m_robotWeightOverloadUserAckedWhileActive = false;
            }
        }

        const bool weightLock = (((value >> 7) & 0x01) == 1);
        if (weightLock != m_robotWeightLock150Bit7Flag) {
            m_robotWeightLock150Bit7Flag = weightLock;
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = QStringLiteral("提示系统");
                record.controlName = QStringLiteral("负载超重锁定");
                record.controlType = QStringLiteral("提示窗口");
                record.operation = weightLock ? QStringLiteral("提示触发") : QStringLiteral("提示解除");
                record.oldValue = QString();
                record.newValue = weightLock ? QStringLiteral("检测到负载超重锁定")
                                             : QStringLiteral("负载超重锁定已解除");
                m_recorder->addRecord(record);
            }
            if (weightLock) {
                m_robotWeightLockUserAckedWhileActive = false;
                hideRobotWeightOverloadDialog();
                showRobotWeightLockDialog();
            } else {
                hideRobotWeightLockDialog();
                m_robotWeightLockUserAckedWhileActive = false;
                if (m_robotWeightOverload150Bit3Flag) {
                    showRobotWeightOverloadDialog();
                }
            }
        }

        const bool axisSyncDeviation = (((value >> 6) & 0x01) == 1);
        if (axisSyncDeviation != m_robotAxisSyncDeviation150Bit6Flag) {
            m_robotAxisSyncDeviation150Bit6Flag = axisSyncDeviation;
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = QStringLiteral("提示系统");
                record.controlName = QStringLiteral("主副轴位置偏差");
                record.controlType = QStringLiteral("提示窗口");
                record.operation = axisSyncDeviation ? QStringLiteral("提示触发") : QStringLiteral("提示解除");
                record.oldValue = QString();
                record.newValue = axisSyncDeviation
                    ? QStringLiteral("检测到主副轴位置偏差过大")
                    : QStringLiteral("主副轴位置偏差提示已解除");
                m_recorder->addRecord(record);
            }
            if (axisSyncDeviation) {
                showRobotAxisSyncDeviationDialog();
            } else {
                hideRobotAxisSyncDeviationDialog();
            }
        }

        const bool heightInterlock = (((value >> 1) & 0x01) == 1);
        if (heightInterlock != m_robotHeightInterlock150Bit1Flag) {
            m_robotHeightInterlock150Bit1Flag = heightInterlock;
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "高度互锁";
                record.controlType = "互锁监控";
                record.operation = heightInterlock ? "互锁触发" : "互锁解除";
                record.oldValue = "";
                record.newValue = heightInterlock ? "高度互锁激活，动作受限" : "高度互锁已解除";
                m_recorder->addRecord(record);
            }
        }

        const bool lengthInterlock = (((value >> 2) & 0x01) == 1);
        if (lengthInterlock != m_robotLengthInterlock150Bit2Flag) {
            m_robotLengthInterlock150Bit2Flag = lengthInterlock;
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "长度互锁";
                record.controlType = "互锁监控";
                record.operation = lengthInterlock ? "互锁触发" : "互锁解除";
                record.oldValue = "";
                record.newValue = lengthInterlock ? "长度互锁激活，动作受限" : "长度互锁已解除";
                m_recorder->addRecord(record);
            }
        }

        if (m_robotArmEmergency150Flag && isBigFeatureEnabled("alarm_system")) {
            QTimer::singleShot(0, this, &MainWindow::updateAlarmDisplay);
        }
    }

    if (address == 102) {
        const bool positiveLimitReached = (((value >> 2) & 0x01) == 1);
        const bool negativeLimitReached = (((value >> 3) & 0x01) == 1);

        if (positiveLimitReached != m_robotPositiveLimit102Bit2Flag) {
            m_robotPositiveLimit102Bit2Flag = positiveLimitReached;
            if (positiveLimitReached) {
                showRobotLimitReachedDialog(true);
            } else if (m_robotLimitDialogTrigger == RobotLimitDialogTrigger::Positive) {
                hideRobotLimitReachedDialog();
            }
        }

        if (negativeLimitReached != m_robotNegativeLimit102Bit3Flag) {
            m_robotNegativeLimit102Bit3Flag = negativeLimitReached;
            if (negativeLimitReached) {
                showRobotLimitReachedDialog(false);
            } else if (m_robotLimitDialogTrigger == RobotLimitDialogTrigger::Negative) {
                hideRobotLimitReachedDialog();
            }
        }

        if (m_recorder) {
            // 记录正限位
            static bool s_posLimitLogged = false;
            if (positiveLimitReached != s_posLimitLogged) {
                s_posLimitLogged = positiveLimitReached;
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "正限位报警";
                record.controlType = "限位监控";
                record.operation = positiveLimitReached ? "限制触发" : "限制解除";
                record.oldValue = "";
                record.newValue = positiveLimitReached ? "正方向行程已达限制" : "正限位已解除";
                m_recorder->addRecord(record);
            }

            // 记录负限位
            static bool s_negLimitLogged = false;
            if (negativeLimitReached != s_negLimitLogged) {
                s_negLimitLogged = negativeLimitReached;
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "负限位报警";
                record.controlType = "限位监控";
                record.operation = negativeLimitReached ? "限制触发" : "限制解除";
                record.oldValue = "";
                record.newValue = negativeLimitReached ? "负方向行程已达限制" : "负限位已解除";
                m_recorder->addRecord(record);
            }
        }
    }

    if (address == 151) {
        const bool cableRetracted = (((value >> 0) & 0x01) == 1);
        const bool cableExtended = (((value >> 1) & 0x01) == 1);

        if (cableRetracted != m_cableRetracted151Bit0Flag) {
            m_cableRetracted151Bit0Flag = cableRetracted;
            if (cableRetracted) {
                static const QString kMsg = QStringLiteral("卷样机钢缆已完全收回");
                if (m_recorder) {
                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = QStringLiteral("提示系统");
                    record.controlName = QStringLiteral("卷样机钢缆到位提示");
                    record.controlType = QStringLiteral("提示窗口");
                    record.operation = QStringLiteral("提示触发");
                    record.oldValue = QString();
                    record.newValue = kMsg;
                    m_recorder->addRecord(record);
                }
                showToast(kMsg, ToastKind::Warning);
            }
        }

        if (cableExtended != m_cableExtended151Bit1Flag) {
            m_cableExtended151Bit1Flag = cableExtended;
            if (cableExtended) {
                static const QString kMsg = QStringLiteral("卷样机钢缆已完全放出");
                if (m_recorder) {
                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = QStringLiteral("提示系统");
                    record.controlName = QStringLiteral("卷样机钢缆到位提示");
                    record.controlType = QStringLiteral("提示窗口");
                    record.operation = QStringLiteral("提示触发");
                    record.oldValue = QString();
                    record.newValue = kMsg;
                    m_recorder->addRecord(record);
                }
                showToast(kMsg, ToastKind::Warning);
            }
        }
    }

}

void MainWindow::syncStepModeUiByCurrentPage()
{
    if (!ui || !ui->StackedWidget || !ui->TBtn_Stepmove) {
        return;
    }

    const int currentPage = ui->StackedWidget->currentIndex();
    int syncAddress = -1;
    if (currentPage == 0) {
        syncAddress = 125;
    } else if (currentPage == 3) {
        syncAddress = 600;
    } else {
        return;
    }

    if (!g_registerCache.contains(syncAddress)) {
        return;
    }

    const quint16 stepModeValue = g_registerCache.value(syncAddress);
    QLabel *runModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarRunModeLabel") : nullptr;

    if (stepModeValue == 2) {
        m_stepModeUnknown = false;
        m_stepModeEnabled = true;
        ui->TBtn_Stepmove->setText("步进模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：步进模式");
        if (runModeLabel) {
            runModeLabel->setText("步进模式");
            runModeLabel->setStyleSheet("color: #00ff00; font-weight: bold; font-size: 11px;");
        }
    } else if (stepModeValue == 1) {
        m_stepModeUnknown = false;
        m_stepModeEnabled = false;
        ui->TBtn_Stepmove->setText("点动模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：点动模式");
        if (runModeLabel) {
            runModeLabel->setText("点动模式");
            runModeLabel->setStyleSheet("color: #00ccff; font-weight: bold; font-size: 11px;");
        }
    } else {
        m_stepModeUnknown = true;
        ui->TBtn_Stepmove->setText("未选择模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：未选择模式");
        if (runModeLabel) {
            runModeLabel->setText("步进未选择");
            runModeLabel->setStyleSheet("color: #aaaaaa; font-weight: bold; font-size: 11px;");
        }
    }

    updateFunctionSwitchVisuals();
}
//浮点数辅助函数
// 将两个16位寄存器转换为32位浮点数（IEEE 754标准）
// 根据测试数据：高位=0x427B，低位=0xAD49，组合成0x427BAD49 = 62.9192
// 输入参数：high - 高位寄存器值，low - 低位寄存器值
// 简洁高效的registersToFloat函数
// 简洁高效的registersToFloat函数
float MainWindow::registersToFloat(quint16 high, quint16 low)
{
    // 打印详细的调试信息
   // qCDebug(lcMainWindow) << "【浮点数转换】开始 - 高位:" << high << "(0x" << QString::number(high, 16).toUpper() << ")"
   //           << "低位:" << low << "(0x" << QString::number(low, 16).toUpper() << ")";

    // 合并为32位整数
    uint32_t combined = (static_cast<uint32_t>(high) << 16) | low;
    //qCDebug(lcMainWindow) << "合并后的32位整数: 0x" << QString::number(combined, 16).toUpper();

    float result;
    memcpy(&result, &combined, sizeof(float));

    //qCDebug(lcMainWindow) << "【浮点数转换结果】" << result;
    return result;
}

float MainWindow::registersToFloatCDAB(quint16 regA, quint16 regB)
{
    const quint8 A = static_cast<quint8>((regA >> 8) & 0xFF);
    const quint8 B = static_cast<quint8>(regA & 0xFF);
    const quint8 C = static_cast<quint8>((regB >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(regB & 0xFF);

    const uint32_t combined = (static_cast<uint32_t>(C) << 24)
                            | (static_cast<uint32_t>(D) << 16)
                            | (static_cast<uint32_t>(A) << 8)
                            | static_cast<uint32_t>(B);

    float result = 0.0f;
    memcpy(&result, &combined, sizeof(float));
    return result;
}

double MainWindow::registersToDoubleDCBAFEHG(quint16 reg1, quint16 reg2, quint16 reg3, quint16 reg4)
{
    // 修改：根据 64位大端 IEEE754 与 BADC FEHG 顺序转换
    // 从日志 [Modbus网络读] 收到字节数: 17 Hex: "... 00 00 00 00 00 00 3f 1a" 分析得到：
    // reg1: 0, 0
    // reg2: 0, 0
    // reg3: 0, 0
    // reg4: 3f, 1a
    // 假设目标是小值 (如 0.0001)，3f 应该是最高字节。
    // 这意味着寄存器顺序是 reg4, reg3, reg2, reg1 (或者 reg1/2/3/4 是相反的)
    // 且每个寄存器内是 Big Endian (0x3F1A -> 3F then 1A)
    
    // 重新映射：
    // 如果 Payload 是 00 00 00 00 00 00 3f 1a，而 3f 是指数位 (A)
    // A=3f, B=1a, C=00, D=00, E=00, F=00, G=00, H=00
    // A,B 来源 reg4 (high, low)
    // C,D 来源 reg3 (high, low)
    // E,F 来源 reg2 (high, low)
    // G,H 来源 reg1 (high, low)

    const quint8 A = static_cast<quint8>((reg4 >> 8) & 0xFF);
    const quint8 B = static_cast<quint8>(reg4 & 0xFF);
    const quint8 C = static_cast<quint8>((reg3 >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(reg3 & 0xFF);
    const quint8 E = static_cast<quint8>((reg2 >> 8) & 0xFF);
    const quint8 F = static_cast<quint8>(reg2 & 0xFF);
    const quint8 G = static_cast<quint8>((reg1 >> 8) & 0xFF);
    const quint8 H = static_cast<quint8>(reg1 & 0xFF);

    const quint64 combined =
        (static_cast<quint64>(A) << 56) |
        (static_cast<quint64>(B) << 48) |
        (static_cast<quint64>(C) << 40) |
        (static_cast<quint64>(D) << 32) |
        (static_cast<quint64>(E) << 24) |
        (static_cast<quint64>(F) << 16) |
        (static_cast<quint64>(G) << 8) |
        static_cast<quint64>(H);

    double result = 0.0;
    memcpy(&result, &combined, sizeof(double));
    return result;
}
//注册到sliderlabel

void MainWindow::setupModbusFloatReading()
{
    // 设备状态组定时器：0~84（默认）独立节拍。
    if (!m_modbusReadTimer) {
        m_modbusReadTimer = new QTimer(this);
        connect(m_modbusReadTimer, &QTimer::timeout,
                this, &MainWindow::readAllFloatRegisters,
                Qt::UniqueConnection);
    } else if (m_modbusReadTimer->isActive()) {
        m_modbusReadTimer->stop();
    }

    // 控制同步组定时器：默认125~130，独立于设备状态组节拍。
    if (!m_mainControlSyncTimer) {
        m_mainControlSyncTimer = new QTimer(this);
        connect(m_mainControlSyncTimer, &QTimer::timeout,
                this, &MainWindow::readMainControlSyncRegisters,
                Qt::UniqueConnection);
    } else if (m_mainControlSyncTimer->isActive()) {
        m_mainControlSyncTimer->stop();
    }

    // 清空之前的列表
    m_floatLabels.clear();

    // 查找所有TechSliderLabel控件
    QList<TechSliderLabel*> allSliderLabels = this->findChildren<TechSliderLabel*>();
    qCDebug(lcMainWindow) << "找到" << allSliderLabels.size() << "个TechSliderLabel";

    // 选择特定的四个TechSliderLabel（根据对象名或位置）
    // 方法1：按对象名筛选（如果对象名有规律）
    QStringList targetNames = {
        "label_Value1_HoriSupSec",
        "label_Value2_VeSupSec",
        "label_Value3_HoriSupSec",
        "label_Value4_EOAT"
    };

    for (const QString &name : targetNames) {
        TechSliderLabel* label = findChild<TechSliderLabel*>(name);
        if (label) {
            m_floatLabels.append(label);
            qCDebug(lcMainWindow) << "添加TechSliderLabel:" << name;
        } else {
            qWarning() << "未找到TechSliderLabel:" << name;
        }
    }

    // 方法2：如果对象名没有规律，使用前4个或特定的4个
    if (m_floatLabels.isEmpty() && allSliderLabels.size() >= 4) {
        for (int i = 0; i < 4; ++i) {
            m_floatLabels.append(allSliderLabels[i]);
            qCDebug(lcMainWindow) << "添加第" << i+1 << "个TechSliderLabel:"
                     << allSliderLabels[i]->objectName();
        }
    }

    if (m_floatLabels.size() < 4) {
        qWarning() << "警告：只找到" << m_floatLabels.size()
                   << "个TechSliderLabel，需要4个";
    }

    // 立即读取一次
    readAllFloatRegisters();
    readMainControlSyncRegisters();

    // 按运行时配置轮询，避免硬编码与配置不一致。
    m_modbusReadTimer->start(m_mainDeviceStatusPollIntervalMs);
    m_mainControlSyncTimer->start(m_mainUiPollIntervalMs);
}

void MainWindow::readAllFloatRegisters()
{
    // 修正轮询逻辑：J1-J4 以及其他状态数据在大全设备 (192.168.1.13) 上
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        static int warnCount = 0;
        if (warnCount++ % 10 == 0) {
            qWarning() << "[警告] 主 Modbus (192.168.1.13) 未连接，无法读取数据";
        }
        return;
    }

    // [调试日志]
    static int timerExecCount = 0;
    if (timerExecCount++ % 20 == 0) {
        // qWarning() << "[轮询执行] 正在轮询设备状态组" << m_mainDeviceStatusStart
        //            << "数量" << m_mainDeviceStatusCount;
    }

    // 设备状态组（默认 192.168.1.13 的 0~84）独立轮询。
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager,
                                              m_mainDeviceStatusStart,
                                              m_mainDeviceStatusCount);

    // 兼容旧配置：若当前轮询组未覆盖 73~84，则补读一次该区间，确保 SixAxies 数据可用。
    const int statusEnd = m_mainDeviceStatusStart + m_mainDeviceStatusCount - 1;
    const bool coversSixAxisRange = (m_mainDeviceStatusStart <= 73) && (statusEnd >= 84);
    if (!coversSixAxisRange) {
        MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 73, 12);
    }

    // 设备状态组由本函数独立负责；控制同步组由 readMainControlSyncRegisters 负责。
}

void MainWindow::readMainControlSyncRegisters()
{
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        return;
    }

    // 控制同步组（默认 125~130）：点动/步进、运动模式、机器人速度同步位。
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager,
                                              m_mainControlSyncStart,
                                              m_mainControlSyncCount);

    // 第四页步进/点动状态同步：读取192.168.1.13设备的600寄存器（1=点动，2=步进）。
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 600, 1);

    // 机器人总功率：192.168.1.13 的 134 寄存器
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 134, 1);

    // 当前负载重量：192.168.1.13 的 123 寄存器
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, kMainCurrentLoadWeightReg, 1);

    // 当前位姿 X/Y/Z/AR：192.168.1.13 保持寄存器 103~118，每组 4 个寄存器为 IEEE754 双精度（与 J1~J4 解析一致）
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 103, 16);

    // 管理员负载阈值：5004 负载超限、5005 负载超重
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 5004, 2);

    // 当前运动目标轴：500（1~4=J1~J4，5=六自由度），供限位 Toast 文案使用
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 500, 1);

    // 卷样机钢缆到位：151.bit0 完全收回 / 151.bit1 完全放出
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 151, 1);

    syncSpareButtonNamesFromRegisters();
}
// 配置所有TechSliderLabel的参数
void MainWindow::setupSliderLabelConfigs()
{
    qWarning() << "[配置中心] 正在初始化 J1-J4 地址映射...";
    m_sliderLabelConfigs.clear();

    // 所有四个控件都需要在三个页面中查找匹配
    QStringList allTargetPages = {"回转升降", "伸缩臂", "EOAT控制"};

    m_sliderLabelConfigs["robot_ArcGauge_J1Angle"] = {
        "J1当前角度:",           // labelText
        "°",                  // unit
        -170.0,              // minValue
        170.0,               // maxValue
        60.0,                // defaultValue
        "°",                 // suffix
        0,                   // modbusAddress1
        1,                   // modbusAddress2
        2,                   // modbusAddress3
        3,                   // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        1                    // 精度：1位小数
    };

    m_sliderLabelConfigs["robot_ArcGauge_J2Height"] = {
        "J2当前高度:",           // labelText
        "mm",                // unit
        4400.0,              // minValue
        7200.0,              // maxValue
        4432.0,               // defaultValue
        "mm",                // suffix
        4,                   // modbusAddress1
        5,                   // modbusAddress2
        6,                   // modbusAddress3
        7,                   // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        0                    // 精度：0位小数
    };

    m_sliderLabelConfigs["robot_ArcGauge_J3Length"] = {
        "J3当前长度:",           // labelText
        "mm",                // unit
        3765.0,                 // minValue
        6765.0,              // maxValue
        560.0,               // defaultValue
        "mm",                // suffix
        12,                  // modbusAddress1
        13,                  // modbusAddress2
        14,                  // modbusAddress3
        15,                  // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        0,                   // 精度：0位小数
        true,                // isSumMode
        {16, 17, 18, 19}     // sumAddress
    };

    m_sliderLabelConfigs["robot_ArcGauge_J4Angle"] = {
        "J4当前角度:",    // labelText (修改为末端组件)
        "°",                  // unit
        -45.0,              // minValue
        45.0,               // maxValue
        -34.0,               // defaultValue
        "°",                 // suffix
        20,                  // modbusAddress1
        21,                  // modbusAddress2
        22,                  // modbusAddress3
        23,                  // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        1                    // 精度：1位小数
    };

    auto addArcGaugeRangeOnly = [&](const QString &key, const QString &label, const QString &suffix,
                                    double minVal, double maxVal, int precision) {
        m_sliderLabelConfigs[key] = {
            label, suffix, minVal, maxVal, 0.0, suffix,
            -1, -1, -1, -1, false, {}, precision
        };
    };
    addArcGaugeRangeOnly(QStringLiteral("robot_ArcGauge_SixAxis1"), QStringLiteral("RX"), QStringLiteral("°"), -15.0, 15.0, 2);
    addArcGaugeRangeOnly(QStringLiteral("robot_ArcGauge_SixAxis2"), QStringLiteral("RY"), QStringLiteral("°"), -15.0, 15.0, 2);
    addArcGaugeRangeOnly(QStringLiteral("robot_ArcGauge_SixAxis3"), QStringLiteral("RZ"), QStringLiteral("°"), -12.0, 12.0, 2);
    addArcGaugeRangeOnly(QStringLiteral("robot_ArcGauge_SixAxis4"), QStringLiteral("X"), QStringLiteral("mm"), -110.0, 110.0, 2);
    addArcGaugeRangeOnly(QStringLiteral("robot_ArcGauge_SixAxis5"), QStringLiteral("Y"), QStringLiteral("mm"), -110.0, 110.0, 2);
    addArcGaugeRangeOnly(QStringLiteral("robot_ArcGauge_SixAxis6"), QStringLiteral("Z"), QStringLiteral("mm"), -90.0, 90.0, 2);

    // TechArcGauge 第二行标签（secondLabelText），与 initSpeedGaugeUI 中 cfg.name 对应
    m_sliderLabelConfigs["robot_ArcGauge_J1Angle"].secondLabelText = QStringLiteral("立柱旋转");
    m_sliderLabelConfigs["robot_ArcGauge_J2Height"].secondLabelText = QStringLiteral("立柱升降");
    m_sliderLabelConfigs["robot_ArcGauge_J3Length"].secondLabelText = QStringLiteral("伸缩平衡臂");
    m_sliderLabelConfigs["robot_ArcGauge_J4Angle"].secondLabelText = QStringLiteral("末端组件");
    m_sliderLabelConfigs["robot_ArcGauge_SixAxis1"].secondLabelText = QStringLiteral("绕X轴旋转");
    m_sliderLabelConfigs["robot_ArcGauge_SixAxis2"].secondLabelText = QStringLiteral("绕Y轴旋转");
    m_sliderLabelConfigs["robot_ArcGauge_SixAxis3"].secondLabelText = QStringLiteral("绕Z轴旋转");
    m_sliderLabelConfigs["robot_ArcGauge_SixAxis4"].secondLabelText = QStringLiteral("X轴位移");
    m_sliderLabelConfigs["robot_ArcGauge_SixAxis5"].secondLabelText = QStringLiteral("Y轴位移");
    m_sliderLabelConfigs["robot_ArcGauge_SixAxis6"].secondLabelText = QStringLiteral("Z轴位移");

    qCDebug(lcMainWindow) << "SliderLabel配置初始化完成";
    qCDebug(lcMainWindow) << "每个控件都需要在以下页面中查找匹配:" << allTargetPages;
}
void MainWindow::setupSliderLabelCopies()
{
    qCDebug(lcMainWindow) << "=== 开始设置SliderLabel副本 ===";

    // 正则表达式，用于提取控件名中的数字部分
    QRegularExpression re("Value(\\d+)");

    // 为每个首页控件建立映射：数字 -> 首页控件
    QMap<int, TechSliderLabel*> mainControlsByNumber;

    for (int i = 1; i <= 4; i++) {
        QString controlName = QString("label_Value%1").arg(i);
        if (m_sliderLabelInstances.contains(controlName)) {
            mainControlsByNumber[i] = m_sliderLabelInstances[controlName];
            qCDebug(lcMainWindow) << "首页控件映射: " << i << " -> " << controlName;
        }
    }

    // 定义要检查的三个目标页面
    QStringList targetPages = {"回转升降", "伸缩臂", "EOAT控制"};

    // 遍历每个目标页面
    for (const QString& pageName : targetPages) {
        qCDebug(lcMainWindow) << "\n处理页面: " << pageName;

        // 找到目标页面的索引
        int pageIndex = -1;
        for (auto pageIt = m_pageNames.begin(); pageIt != m_pageNames.end(); ++pageIt) {
            if (pageIt.value() == pageName) {
                pageIndex = pageIt.key();
                break;
            }
        }

        if (pageIndex == -1) {
            qWarning() << "找不到页面:" << pageName;
            continue;
        }

        // 获取目标页面
        QWidget* targetPage = ui->StackedWidget->widget(pageIndex);
        if (!targetPage) {
            qWarning() << "页面" << pageIndex << "不存在";
            continue;
        }

        // 获取目标页面中所有的TechSliderLabel
        QList<TechSliderLabel*> targetSliders = targetPage->findChildren<TechSliderLabel*>();
        qCDebug(lcMainWindow) << "  页面中包含" << targetSliders.size() << "个TechSliderLabel";

        // 遍历目标页面中的每个控件
        for (TechSliderLabel* targetSlider : targetSliders) {
            QString targetName = targetSlider->objectName();

            // 从控件名中提取数字
            QRegularExpressionMatch match = re.match(targetName);
            if (match.hasMatch()) {
                int targetNumber = match.captured(1).toInt();
                qCDebug(lcMainWindow) << "  检查控件:" << targetName << "，提取数字:" << targetNumber;

                // 查找对应的首页控件
                if (mainControlsByNumber.contains(targetNumber)) {
                    TechSliderLabel* original = mainControlsByNumber[targetNumber];
                    QString originalName = original->objectName();

                    // 获取配置
                    if (m_sliderLabelConfigs.contains(originalName)) {
                        const SliderLabelConfig& config = m_sliderLabelConfigs[originalName];

                        qCDebug(lcMainWindow) << "    匹配成功! 将" << originalName << "的配置复制给" << targetName;

                        // 复制配置
                        targetSlider->setLabelText(config.labelText);
                        targetSlider->setRange(config.minValue, config.maxValue);
                        targetSlider->setValue(config.defaultValue);
                        targetSlider->setSuffix(config.suffix);
                        targetSlider->setPrecision(config.precision); // 设置精度
                        targetSlider->setTechBlueStyle();

                        // 添加到管理列表
                        m_pageSliders[pageName].append(targetSlider);
                    } else {
                        qWarning() << "    警告: 找不到" << originalName << "的配置";
                    }
                } else {
                    qCDebug(lcMainWindow) << "    没有找到编号为" << targetNumber << "的首页控件";
                }
            } else {
                qCDebug(lcMainWindow) << "  控件" << targetName << "不包含'ValueX'模式，跳过";
            }
        }
    }

    // 验证结果
    qCDebug(lcMainWindow) << "\n=== 复制结果验证 ===";
    for (const QString& pageName : targetPages) {
        if (m_pageSliders.contains(pageName)) {
            const QVector<TechSliderLabel*>& sliders = m_pageSliders[pageName];
            qCDebug(lcMainWindow) << "页面[" << pageName << "]有" << sliders.size() << "个已配置的SliderLabel:";
            for (TechSliderLabel* slider : sliders) {
                qCDebug(lcMainWindow) << "  - " << slider->objectName() << "标签:" << slider->labelText();
            }
        } else {
            qCDebug(lcMainWindow) << "页面[" << pageName << "]没有已配置的SliderLabel";
        }
    }

    qCDebug(lcMainWindow) << "=== SliderLabel副本设置完成 ===";
}


void MainWindow::setupAGVModbus()
{
    if (!isBigFeatureEnabled("modbus_agv")) {
        qCDebug(lcMainWindow) << "AGV Modbus功能已关闭，跳过初始化";
        return;
    }

    // 创建AGV Modbus管理器（无父对象，便于迁移到专用线程）
    m_agvModbusManager = new AGVModbusManager(nullptr);
    if (!m_agvModbusManager->startWorkerThread()) {
        qWarning() << "AGV Modbus专用线程启动失败，回退为当前线程运行";
    }
    qCDebug(lcMainWindow) << "创建AGV Modbus管理器完成";

    // 连接信号槽
    connect(m_agvModbusManager, &AGVModbusManager::connected,
            this, &MainWindow::onAGVModbusConnected);
    connect(m_agvModbusManager, &AGVModbusManager::disconnected,
            this, &MainWindow::onAGVModbusDisconnected);
    connect(m_agvModbusManager, &AGVModbusManager::errorOccurred,
            this, &MainWindow::onAGVModbusError);
    connect(m_agvModbusManager, &AGVModbusManager::bitVariableChanged,
            this, &MainWindow::onAGVBitVariableChanged);
    connect(m_agvModbusManager, &AGVModbusManager::wordVariableChanged,
            this, &MainWindow::onAGVWordVariableChanged);
    connect(m_agvModbusManager, &AGVModbusManager::updateFaultsLabel,
            this, &MainWindow::onAGVUpdateFaultsLabel);
    connect(m_agvModbusManager, &AGVModbusManager::updateProgressBar,
            this, &MainWindow::onAGVUpdateProgressBar);
    connect(m_agvModbusManager, &AGVModbusManager::updateStatusLabel,
            this, &MainWindow::onAGVUpdateStatusLabel);
    connect(m_agvModbusManager, &AGVModbusManager::addFaultCodeToList,
            this, &MainWindow::onAGVAddFaultCodeToList);
    connect(m_agvModbusManager, &AGVModbusManager::clearFaultCodes,
            this, &MainWindow::onAGVClearFaultCodes);
    connect(m_agvModbusManager, &AGVModbusManager::heartbeatReceived,
            this, &MainWindow::onAGVHeartbeatReceived);

    connect(m_agvModbusManager, &AGVModbusManager::teachingWriteGateDenied,
            this, &MainWindow::showTeachingWriteGateDeniedDialog,
            Qt::QueuedConnection);

    // 连接转向切换检查槽函数
    connect(m_agvModbusManager, &AGVModbusManager::registerValueChanged,
            this, &MainWindow::checkSteeringSwitchCompletion);

    // 添加registerValueChanged信号连接用于调试
    connect(m_agvModbusManager, &AGVModbusManager::registerValueChanged,
            this, [this](int address, quint16 value) {
                m_agvRegisterShadow[address] = value;

                const ButtonModbusMapping::Binding controlBinding = buttonModbusBinding(QStringLiteral("TBtn_ControlMode"));
                const ModbusRegisterSpec controlRead = controlBinding.reads.isEmpty()
                    ? ModbusRegisterSpec{}
                    : controlBinding.reads.first();
                const int controlReadAddr = ButtonModbusMapping::addressOr(controlRead, 100);
                const int wirelessReadValue = ButtonModbusMapping::stateValueOr(controlRead, 1, 1);
                const int wiredReadValue = ButtonModbusMapping::stateValueOr(controlRead, 2, 2);
                if (address == controlReadAddr && m_controlModeBtn) {
                    if (static_cast<int>(value) == wirelessReadValue) {
                        m_controlMode = WIRELESS_MODE;
                        m_controlModeBtn->setText("遥控器控制");
                    } else if (static_cast<int>(value) == wiredReadValue) {
                        m_controlMode = WIRED_MODE;
                        m_controlModeBtn->setText("示教器控制");
                    }

                    QLabel *controlModeLabel = ui && ui->statusBar
                                                    ? ui->statusBar->findChild<QLabel*>("statusBarControlModeLabel")
                                                    : nullptr;
                    if (controlModeLabel) {
                        controlModeLabel->setText(m_controlMode == WIRED_MODE ? "示教器控制" : "遥控器控制");
                        controlModeLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                                            .arg(m_controlMode == WIRED_MODE ? "#ffffff" : "#ffff00"));
                    }
                }

                const ButtonModbusMapping::Binding oaBinding = buttonModbusBinding(QStringLiteral("techBtn_AGV_OA"));
                const ModbusRegisterSpec oaRead = oaBinding.reads.isEmpty()
                    ? ModbusRegisterSpec{}
                    : oaBinding.reads.first();
                const int oaReadAddr = ButtonModbusMapping::addressOr(oaRead, 50);
                const int oaReadBit = ButtonModbusMapping::bitOr(oaRead, 13);
                const int oaReadOnValue = ButtonModbusMapping::stateValueOr(oaRead, 1, 0);
                if (address == oaReadAddr) {
                    const bool oaEnabled = (((value >> oaReadBit) & 0x01) == (oaReadOnValue != 0 ? 1 : 0));
                    m_agvOaEnabled = oaEnabled;
                    if (m_techBtnAGV_OA) {
                        m_techBtnAGV_OA->setText(oaEnabled ? "避障开启" : "避障关闭");
                        m_techBtnAGV_OA->setPrimaryColor(oaEnabled ? QColor("#00C8FF") : QColor("#7F8C8D"));
                        m_techBtnAGV_OA->setGlowColor(oaEnabled ? QColor(0, 200, 255, 180) : QColor(127, 140, 141, 100));
                    }

                    QLabel *oaLabel = ui && ui->statusBar
                                           ? ui->statusBar->findChild<QLabel*>("statusBarOaLabel")
                                           : nullptr;
                    if (oaLabel) {
                        oaLabel->setText(oaEnabled ? "避障开" : "避障关");
                        oaLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                                   .arg(oaEnabled ? "#00C8FF" : "#7F8C8D"));
                    }
                }

                if (m_agvUiStateSyncEnabled && address == 153 && m_editAGV_MoveSpeed) {
                    const QSignalBlocker blocker(m_editAGV_MoveSpeed);
                    const double clamped = qBound(m_editAGV_MoveSpeed->minimum(), static_cast<double>(value), m_editAGV_MoveSpeed->maximum());
                    m_editAGV_MoveSpeed->setValue(clamped);
                }

                if (address == 151) {
                    updateInclinometerValue(true, value);
                }

                if (address == 152) {
                    updateInclinometerValue(false, value);
                }

                if (m_agvUiStateSyncEnabled && address == 154 && m_editAGV_Angle) {
                    const QSignalBlocker blocker(m_editAGV_Angle);
                    const double clamped = qBound(m_editAGV_Angle->minimum(), static_cast<double>(value), m_editAGV_Angle->maximum());
                    m_editAGV_Angle->setValue(clamped);
                }

                if (address == 51) {
                    const bool bit5 = (((value >> 5) & 0x01) == 1);

                    if (bit5 != m_agvChassisEmergency51Bit5Flag) {
                        m_agvChassisEmergency51Bit5Flag = bit5;
                        if (bit5 && m_recorder) {
                            OperationRecord record;
                            record.timestamp = QDateTime::currentDateTime();
                            record.pageName = "报警系统";
                            record.controlName = "急停报警";
                            record.controlType = "报警监控";
                            record.operation = "报警触发";
                            record.oldValue = "";
                            record.newValue = "AGV触发了急停报警";
                            m_recorder->addRecord(record);
                        }
                        if (bit5 && m_agvModbusManager && m_agvModbusManager->isConnected()) {
                            m_agvModbusManager->readMultipleRegisters(150, 1);
                        }
                        QTimer::singleShot(0, this, &MainWindow::checkAlarmConditions);
                    }

                    handleAGVRegister51Alerts(value);

                    const ButtonModbusMapping::Binding parkBinding = buttonModbusBinding(QStringLiteral("techBtn_AGV_Park"));
                    const int parkReadAddr = parkBinding.reads.isEmpty()
                        ? 51
                        : ButtonModbusMapping::addressOr(parkBinding.reads.first(), 51);
                    if (parkReadAddr == 51) {
                        syncAGVParkingStateFromRegister51(value, true);
                    }
                }

                const ButtonModbusMapping::Binding parkBindingForAddr = buttonModbusBinding(QStringLiteral("techBtn_AGV_Park"));
                const int parkReadAddrForAddr = parkBindingForAddr.reads.isEmpty()
                    ? 51
                    : ButtonModbusMapping::addressOr(parkBindingForAddr.reads.first(), 51);
                if (address == parkReadAddrForAddr && parkReadAddrForAddr != 51) {
                    syncAGVParkingStateFromRegister51(value, false);
                }

                if (address == 50) {
                    syncAGVSteeringModeFromRegister50(value);
                }

                if (address == 150 && m_agvChassisEmergency51Bit5Flag
                    && isBigFeatureEnabled("alarm_system")) {
                    QTimer::singleShot(0, this, &MainWindow::updateAlarmDisplay);
                }

                if (isFeatureEnabled("modbus_agv", "modbus_agv.read_logs")) {
                    qCDebug(lcMainWindow) << "[AGV] 寄存器值变化 - 地址:" << address
                             << "值:" << value
                             << "(0x" << QString::number(value, 16).toUpper() << ")";
                }
            });

    // 配置
    m_agvModbusManager->setPollInterval(m_agvPollIntervalMs);
    m_agvModbusManager->setAutoReconnect(true, m_agvReconnectIntervalMs);

    // 连接到设备 - 88 -> 100
    QString agvHost = m_agvHost;
    quint16 agvPort = m_agvPort;

    // 如果开启本机 TCP 模拟器模式，则重定向到本机 AGV 模拟端口
    if (isFeatureEnabled("tcp_transmission", "tcp.local_simulator")) {
        agvHost = "127.0.0.1";
        agvPort = 5021;
        qCDebug(lcMainWindow) << "启用本机 TCP 模拟器模式：AGV ->" << agvHost << ":" << agvPort;
    } else if (isFeatureEnabled("tcp_transmission", "tcp.remote_simulator")) {
        agvHost = m_remoteSimulatorHost;
        agvPort = 5021;
        qCDebug(lcMainWindow) << "启用远程 TCP 模拟器模式：AGV ->" << agvHost << ":" << agvPort;
    }

    m_agvModbusManager->connectToDevice(agvHost, agvPort);

    qCDebug(lcMainWindow) << "AGV Modbus管理器初始化完成，目标:" << agvHost << ":" << agvPort;

    // 添加定时器检查连接状态
    QTimer::singleShot(2000, this, [this]() {
        if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
            qCDebug(lcMainWindow) << "AGV Modbus已成功连接";
            ui->statusBar->showMessage("AGV Modbus已连接", 3000);
        } else {
            qCDebug(lcMainWindow) << "AGV Modbus未连接";
            ui->statusBar->showMessage("AGV Modbus连接失败", 3000);
        }
    });

    // 连接信号槽
    bool connected1 = connect(m_agvModbusManager, &AGVModbusManager::updateProgressBar,
                              this, &MainWindow::onAGVUpdateProgressBar, Qt::QueuedConnection);
    qCDebug(lcMainWindow) << "updateProgressBar信号连接状态:" << (connected1 ? "成功" : "失败");
}











/**
 * @brief 设置 AGV 页面相关的 UI 并进行初始化
 *
 * 在 AGV 控制页面中查找并缓存常用控件（电池进度条、故障列表、故障/状态标签等），
 * 并对速度仪表（`TechSpeedGauge`）做单位与量程调整。该函数假定 UI 已经加载完毕，
 * 仅执行查找与初始化，不创建页面控件。
 *
 * 用法示例：
 * @code
 * // 在 MainWindow 构造函数或 UI 初始化后调用
 * setupAGVUI();
 * @endcode
 */
void MainWindow::setupAGVUI()
{
    if (!isBigFeatureEnabled("modbus_agv")) {
        return;
    }

    // 查找AGV相关控件所在页面：当前UI中控件位于 page_Robot，而非 page_AGV。
    QWidget *agvPage = ui->StackedWidget->findChild<QWidget*>("page_Robot");
    if (!agvPage) {
        agvPage = ui->StackedWidget->findChild<QWidget*>("page_AGV");
    }
    if (!agvPage) {
        agvPage = ui->StackedWidget->widget(0);
    }

    qCDebug(lcMainWindow) << "AGV UI页面定位:" << (agvPage ? agvPage->objectName() : QString("<null>"));

    if (agvPage) {
        // 先清理可能存在的旧缓存，防止重复添加
        m_agvStatusLabels.clear();

        // 查找故障列表
        m_agvFaultListWidget = agvPage->findChild<QListWidget*>("listWidget_faultCodes");

        // 查找故障标签
        m_agvFaultsLabel = agvPage->findChild<QLabel*>("label_faults");

        // 查找状态标签（根据您的UI命名）
        QStringList statusLabelNames = {
            "label_jog_running", "label_steering_align", "label_transverse_mode",
            "label_rotate_mode", "label_speed", "label_jog_displacement",
            "label_battery1_text", "label_battery2_text", "label_agv_connection"
        };

        for (const QString &name : statusLabelNames) {
            QLabel *label = agvPage->findChild<QLabel*>(name);
            if (label) {
                m_agvStatusLabels.append(label);
                // 设置初始文本
                if (name.contains("_touch") || name.contains("_slow") ||
                    name.contains("_stop") || name.contains("_running") ||
                    name.contains("_align") || name.contains("_mode")) {
                    label->setText("无动作");
                } else if (name == "label_agv_connection") {
                    label->setText("未连接");
                } else if (name == "label_faults") {
                    label->setText("无故障");
                }
            }
        }
        if (m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
            QQuickItem *rootItem = m_speedGaugeQml->rootObject();
            rootItem->setProperty("minValue", 0);
            rootItem->setProperty("maxValue", 900);
            rootItem->setProperty("unit", "mm/s");
            rootItem->setProperty("currentValue", 0);
            rootItem->setProperty("touchFront", false);
            rootItem->setProperty("touchBack", false);
            rootItem->setProperty("touchLeft", false);
            rootItem->setProperty("touchRight", false);
            rootItem->setProperty("avoidFrontState", 0);
            rootItem->setProperty("avoidBackState", 0);
            rootItem->setProperty("avoidLeftState", 0);
            rootItem->setProperty("avoidRightState", 0);
            rootItem->setProperty("statusText", "正常");
            qCDebug(lcMainWindow) << "QML AGV速度仪表初始化完成，量程:0-900 mm/s";
        } else {
            qWarning() << "未找到QML AGV速度仪表";
        }

        qCDebug(lcMainWindow) << "找到" << m_agvStatusLabels.size() << "个AGV状态标签";
    }

    qCDebug(lcMainWindow) << "找到" << m_agvStatusLabels.size() << "个AGV状态标签";





    if (agvPage) {
        QList<QProgressBar*> allBars = agvPage->findChildren<QProgressBar*>();
        for (QProgressBar* bar : allBars) {
            qCDebug(lcMainWindow) << "进度条:" << bar->objectName()
                     << "当前值:" << bar->value()
                     << "范围:" << bar->minimum() << "-" << bar->maximum();
        }

        QList<BatteryWidget*> allBatteryWidgets = agvPage->findChildren<BatteryWidget*>();
        for (BatteryWidget* bar : allBatteryWidgets) {
            qCDebug(lcMainWindow) << "BatteryWidget:" << bar->objectName() << "当前值:" << bar->level();
        }
    }
}

// 添加槽函数实现
/**
 * @brief 处理 AGV Modbus 连接成功事件
 *
 * 更新状态栏并记录连接事件到操作记录器。
 */
void MainWindow::onAGVModbusConnected()
{
    qCDebug(lcMainWindow) << "AGV Modbus连接成功";
    m_agvDisconnectedWarnedAddresses.clear();
    ui->statusBar->showMessage("AGV Modbus已连接", 3000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Connected));
        agvIndicator->setToolTip("AGV Modbus连接正常");
    }

    // 记录连接事件
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "AGV Modbus连接";
    record.controlType = "ModbusTCP";
    record.operation = "connected";
    record.oldValue = "";
    record.newValue = "连接成功";
    m_recorder->addRecord(record);

    // 开机后短时重复读取关键寄存器，尽快回填模式按钮与驻车状态。
    for (int i = 0; i < 5; ++i) {
        QTimer::singleShot(i * 300, this, [this]() {
            if (!m_agvModbusManager || !m_agvModbusManager->isConnected()) {
                return;
            }
            m_agvModbusManager->readMultipleRegisters(50, 1);
            m_agvModbusManager->readMultipleRegisters(51, 1);
            m_agvModbusManager->readMultipleRegisters(153, 2);
        });
    }

    const bool hostEstop = isFeatureEnabled("alarm_system", "alarm.emergency_stop")
        && (m_robotArmEmergency150Flag || m_agvChassisEmergency51Bit5Flag);
    m_agvHostEstopCommandSynced = false;
    syncAgvHostEmergencyStopCommand(hostEstop);
}

/**
 * @brief 处理 AGV Modbus 断开连接事件
 *
 * 更新 UI 中的连接状态显示并记录断开事件。
 */
void MainWindow::onAGVModbusDisconnected()
{
    qCDebug(lcMainWindow) << "AGV Modbus连接断开";
    m_agvHostEstopCommandSynced = false;
    ui->statusBar->showMessage("AGV Modbus连接断开", 3000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Disconnected));
        agvIndicator->setToolTip("AGV Modbus已断开");
    }

    // 更新连接状态标签
    if (m_agvFaultsLabel) {
        m_agvFaultsLabel->setText("连接断开");
    }

    // 记录断开事件
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "AGV Modbus连接";
    record.controlType = "ModbusTCP";
    record.operation = "disconnected";
    record.oldValue = "";
    record.newValue = "连接断开";
    m_recorder->addRecord(record);
}

/**
 * @brief 处理 AGV Modbus 错误通知
 * @param error 错误描述字符串
 *
 * 在收到错误时显示到状态栏并记录到操作记录。
 */
void MainWindow::onAGVModbusError(const QString &error)
{
    qCDebug(lcMainWindow) << "AGV Modbus错误:" << error;
    ui->statusBar->showMessage(QString("AGV Modbus错误: %1").arg(error), 5000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Error));
        agvIndicator->setToolTip(QString("AGV Modbus错误: %1").arg(error));
    }

    // 记录错误事件
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "AGV Modbus连接";
    record.controlType = "ModbusTCP";
    record.operation = "error";
    record.oldValue = "";
    record.newValue = error;
    m_recorder->addRecord(record);
}

void MainWindow::onAGVBitVariableChanged(int address, int bitPos, bool value)
{
    // 处理障碍物传感器数据 (地址50)
    if (address == 50 && m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
        // 位定义：
        // 1 前触边, 2 后触边, 3 左触边, 4 右触边
        // 5 前避障减速, 6 前避障停止, 7 后避障减速, 8 后避障停止
        static bool touchFront = false, touchBack = false, touchLeft = false, touchRight = false;
        static bool avoidFrontSlow = false, avoidFrontStop = false;
        static bool avoidBackSlow = false, avoidBackStop = false;

        if (bitPos == 1) touchFront = value;
        else if (bitPos == 2) touchBack = value;
        else if (bitPos == 3) touchLeft = value;
        else if (bitPos == 4) touchRight = value;
        else if (bitPos == 5) avoidFrontSlow = value;
        else if (bitPos == 6) avoidFrontStop = value;
        else if (bitPos == 7) avoidBackSlow = value;
        else if (bitPos == 8) avoidBackStop = value;

        const int avoidFrontState = avoidFrontStop ? 2 : (avoidFrontSlow ? 1 : 0);
        const int avoidBackState = avoidBackStop ? 2 : (avoidBackSlow ? 1 : 0);

        QString statusText = "正常";
        if (touchFront || touchBack || touchLeft || touchRight) {
            statusText = "触边触发";
        } else if (avoidFrontState > 0 || avoidBackState > 0) {
            statusText = "避障触发";
        }

        QQuickItem *rootItem = m_speedGaugeQml->rootObject();
        rootItem->setProperty("touchFront", touchFront);
        rootItem->setProperty("touchBack", touchBack);
        rootItem->setProperty("touchLeft", touchLeft);
        rootItem->setProperty("touchRight", touchRight);
        rootItem->setProperty("avoidFrontState", avoidFrontState);
        rootItem->setProperty("avoidBackState", avoidBackState);
        rootItem->setProperty("avoidLeftState", 0);
        rootItem->setProperty("avoidRightState", 0);
        rootItem->setProperty("statusText", statusText);
    }
}

void MainWindow::onAGVWordVariableChanged(int address, quint16 value)
{
    // 电池电量寄存器兜底更新：避免仅依赖 updateProgressBar 路径
    if (address == 102) {
        int batteryPercent = qMin(static_cast<int>(value), 100);
        onAGVUpdateProgressBar("progressBar_battery1", batteryPercent);
        onAGVUpdateStatusLabel("label_battery1_text", QString("%1%").arg(batteryPercent));
    } else if (address == 103) {
        int batteryPercent = qMin(static_cast<int>(value), 100);
        onAGVUpdateStatusLabel("label_battery2_text", QString("%1%").arg(batteryPercent));
    } else if (address == 156) {
        const bool isCharging = (value == 1);

        QObject *root = ui->StackedWidget;
        BatteryWidget *bw = root ? root->findChild<BatteryWidget*>(QStringLiteral("progressBar_battery1"),
                                                                   Qt::FindChildrenRecursively)
                                 : nullptr;
        if (!bw) {
            bw = this->findChild<BatteryWidget*>(QStringLiteral("progressBar_battery1"),
                                                 Qt::FindChildrenRecursively);
        }

        if (bw) {
            bw->setCharging(isCharging);
        }
    }

    // 特别处理行驶速度（地址104）
    if (address == 104 && isFeatureEnabled("modbus_agv", "agv.speed_gauge")) {
        // 行驶速度 (mm/s)
        qreal speedValue = static_cast<qreal>(value);

        updateSpeed(speedValue);

        // 同时更新原来的label_speed（如果需要保持兼容）
        // 直接调用onAGVUpdateStatusLabel函数，而不是发射信号
        onAGVUpdateStatusLabel("label_speed", QString("%1 mm/s").arg(value));
    }

    if (m_agvUiStateSyncEnabled && address == 153 && m_editAGV_MoveSpeed) {
        const QSignalBlocker blocker(m_editAGV_MoveSpeed);
        const double clamped = qBound(m_editAGV_MoveSpeed->minimum(), static_cast<double>(value), m_editAGV_MoveSpeed->maximum());
        m_editAGV_MoveSpeed->setValue(clamped);
    }

    if (m_agvUiStateSyncEnabled && address == 154 && m_editAGV_Angle) {
        const QSignalBlocker blocker(m_editAGV_Angle);
        const double clamped = qBound(m_editAGV_Angle->minimum(), static_cast<double>(value), m_editAGV_Angle->maximum());
        m_editAGV_Angle->setValue(clamped);
    }
}






void MainWindow::onAGVUpdateFaultsLabel(const QString &text)
{
    if (m_agvFaultsLabel) {
        m_agvFaultsLabel->setText(text);
    }
}
void MainWindow::onAGVUpdateProgressBar(const QString &name, int value)
{
    // 在整个 StackedWidget（及其子页面）中递归查找控件进行更新，避免依赖特定页面索引
    QObject *root = ui->StackedWidget;

    // 优先尝试查找并更新提升后的 BatteryWidget
    BatteryWidget *bw = root->findChild<BatteryWidget*>(name, Qt::FindChildrenRecursively);
    if (bw) {
        bw->setLevel(static_cast<double>(value));
        return;
    }

    // 后备方案：查找传统 QProgressBar (如果 UI 还没来得及替换或作为回退)
    QProgressBar *progressBar = root->findChild<QProgressBar*>(name, Qt::FindChildrenRecursively);
    if (progressBar) {
        progressBar->setValue(value);
        progressBar->update();
        return;
    }

    // 最后尝试在 MainWindow 范围内查找（更宽泛的回退）
    bw = this->findChild<BatteryWidget*>(name, Qt::FindChildrenRecursively);
    if (bw) {
        bw->setLevel(static_cast<double>(value));
        return;
    }
    progressBar = this->findChild<QProgressBar*>(name, Qt::FindChildrenRecursively);
    if (progressBar) {
        progressBar->setValue(value);
        progressBar->update();
        return;
    }

    qWarning() << "未找到AGV电池控件:" << name << "值:" << value;
}

void MainWindow::onAGVAddFaultCodeToList(const QString &faultCode)
{
    if (!isFeatureEnabled("modbus_agv", "agv.fault_codes")) {
        return;
    }

    if (isFeatureEnabled("modbus_agv", "modbus_agv.read_logs")) {
        qCDebug(lcMainWindow) << "[MainWindow] 添加故障代码到列表:" << faultCode;
    }
    if (m_agvFaultListWidget) {
        m_agvFaultListWidget->addItem(faultCode);
        if (isFeatureEnabled("modbus_agv", "modbus_agv.read_logs")) {
            qCDebug(lcMainWindow) << "  成功添加到列表";
        }
    } else {
        static bool warnedOnce = false;
        if (!warnedOnce) {
            warnedOnce = true;
            qWarning() << "故障列表控件未找到(listWidget_faultCodes)，后续同类日志已抑制";
        }
    }
}

void MainWindow::onAGVClearFaultCodes()
{
    if (!isFeatureEnabled("modbus_agv", "agv.fault_codes")) {
        return;
    }

    if (m_agvFaultListWidget) {
        m_agvFaultListWidget->clear();
    }
}

void MainWindow::onAGVHeartbeatReceived()
{
    // 心跳接收，可以用于更新连接状态或显示最后通信时间
    static int heartbeatCount = 0;
    heartbeatCount++;

    if (heartbeatCount % 10 == 0) {
        qCDebug(lcMainWindow) << "AGV心跳 - 计数:" << heartbeatCount;
    }
}
void MainWindow::onAGVUpdateStatusLabel(const QString &name, const QString &text)
{
    // 查找对应的标签并更新运动速度
    for (QLabel *label : m_agvStatusLabels) {
        if (label && label->objectName() == name) {
            label->setText(text);
            break;
        }
    }
}
// ============ 使能按钮功能实现 ============

// 处理使能按钮状态变化
void MainWindow::onEnableButtonStateChanged(bool enabled)
{
    if (!isFeatureEnabled("input_devices", "input.enable_button")) {
        return;
    }

    // 这个槽函数在主线程中执行
    qCDebug(lcMainWindow) << "[主线程] 收到使能按钮状态:" << (enabled ? "按下" : "松开");

    // 如果状态发生变化
    if (enabled != m_lastEnableButtonState) {
        m_lastEnableButtonState = enabled;

        QString statusText = enabled ? "使能按钮: 按下" : "使能按钮: 松开";
        qCDebug(lcMainWindow) << "=== 使能按钮状态变化: " << statusText << " ===";

        // 更新状态栏
        ui->statusBar->showMessage(statusText, 2000);

        // 处理使能状态变化
        processEnableButton(enabled);

        // 记录到操作记录
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "系统";
        record.controlName = "使能按钮";
        record.controlType = "EnableButton";
        record.operation = "状态变化";
        record.oldValue = !enabled;
        record.newValue = enabled;
        m_recorder->addRecord(record);
    }
}

// 处理使能按钮错误
void MainWindow::onEnableButtonError(const QString &error)
{
    qWarning() << "使能按钮错误:" << error;
    ui->statusBar->showMessage(error, 5000);
}

/**
 * @brief 初始化并启动使能按钮的监控线程
 *
 * 该函数打开设备文件 `/dev/buttonstop`，创建 `EnableButtonWorker` 并将其
 * 移动到工作线程中以异步读取按键状态，接收信号并在主线程中更新 UI。
 */
void MainWindow::setupEnableButton()
{
    if (!isFeatureEnabled("input_devices", "input.enable_button")) {
        qCDebug(lcMainWindow) << "使能按钮功能已关闭，跳过初始化";
        return;
    }

    if (m_enableButtonThread && m_enableButtonThread->isRunning()) {
        qCDebug(lcMainWindow) << "使能按钮监控线程已在运行，跳过重复初始化";
        return;
    }

    qCDebug(lcMainWindow) << "初始化使能按钮...";

    // 检查设备文件是否存在
    if (access("/dev/buttonstop", F_OK) == -1) {
        qWarning() << "错误：/dev/buttonstop 设备文件不存在";
        ui->statusBar->showMessage("使能按钮设备不存在", 5000);
        return;
    }

    // 注意：使用阻塞模式打开，不在主线程中读取
    m_enableButtonFd = open("/dev/buttonstop", O_RDONLY);
    if (m_enableButtonFd < 0) {
        QString error = QString("无法打开使能按钮设备 /dev/buttonstop: %1 (errno: %2)")
                            .arg(strerror(errno)).arg(errno);
        qWarning() << error;
        ui->statusBar->showMessage(error, 5000);
        return;
    }

    qCDebug(lcMainWindow) << "使能按钮设备打开成功，文件描述符:" << m_enableButtonFd;

    // 使用成员指针托管线程与 worker 生命周期，便于析构阶段可靠停止
    m_enableButtonThread = new QThread(this);
    m_enableButtonWorker = new EnableButtonWorker(m_enableButtonFd);
    m_enableButtonWorker->moveToThread(m_enableButtonThread);

    // 连接信号槽
    connect(m_enableButtonThread, &QThread::started,
            m_enableButtonWorker, &EnableButtonWorker::startPolling);
    connect(m_enableButtonWorker, &EnableButtonWorker::buttonStateChanged,
            this, &MainWindow::onEnableButtonStateChanged, Qt::QueuedConnection);
    connect(m_enableButtonWorker, &EnableButtonWorker::errorOccurred,
            this, &MainWindow::onEnableButtonError, Qt::QueuedConnection);

    // 线程结束时清理
    connect(m_enableButtonThread, &QThread::finished,
            m_enableButtonWorker, &EnableButtonWorker::deleteLater);
    connect(m_enableButtonThread, &QThread::finished,
            this, [this]() { m_enableButtonWorker = nullptr; });
    connect(m_enableButtonThread, &QThread::finished,
            this, [this]() { m_enableButtonThread = nullptr; });

    // 启动线程
    m_enableButtonThread->start();

    qCDebug(lcMainWindow) << "使能按钮监控线程已启动";
    ui->statusBar->showMessage("使能按钮监控已启动", 3000);
}

void MainWindow::applyAGVParkingButtonUi(bool enabled)
{
    if (!m_techBtnAGV_Park || m_agvLegAbnormal51Bit7Flag) {
        return;
    }
    m_techBtnAGV_Park->setText(enabled ? QStringLiteral("驻车开启") : QStringLiteral("驻车关闭"));
    m_techBtnAGV_Park->setPrimaryColor(enabled ? QColor("#00C8FF") : QColor("#7F8C8D"));
    m_techBtnAGV_Park->setGlowColor(enabled ? QColor(0, 200, 255, 180) : QColor(127, 140, 141, 100));
}

void MainWindow::applyAGVParkingLegAbnormalUi()
{
    if (m_techBtnAGV_Park) {
        m_techBtnAGV_Park->setText(QStringLiteral("支腿异常"));
        m_techBtnAGV_Park->setPrimaryColor(QColor("#FF6600"));
        m_techBtnAGV_Park->setGlowColor(QColor(255, 102, 0, 180));
    }

    QLabel *parkLabel = ui && ui->statusBar
                             ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarParkLabel"))
                             : nullptr;
    if (parkLabel) {
        parkLabel->setText(QStringLiteral("支腿异常"));
        parkLabel->setStyleSheet(QStringLiteral("color: #FF6600; font-weight: bold; font-size: 11px;"));
    }
}

void MainWindow::applyAGVParkingStatusBarUi(bool enabled)
{
    if (m_agvLegAbnormal51Bit7Flag) {
        return;
    }

    QLabel *parkLabel = ui && ui->statusBar
                             ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarParkLabel"))
                             : nullptr;
    if (parkLabel) {
        parkLabel->setText(enabled ? QStringLiteral("驻车开") : QStringLiteral("驻车关"));
        parkLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                     .arg(enabled ? "#00C8FF" : "#7F8C8D"));
    }
}

void MainWindow::restoreParkingUiAfterFailure(bool enabled)
{
    if (m_agvLegAbnormal51Bit7Flag) {
        applyAGVParkingLegAbnormalUi();
    } else {
        applyAGVParkingButtonUi(enabled);
        applyAGVParkingStatusBarUi(enabled);
    }
}

void MainWindow::syncAGVParkingStateFromRegister51(quint16 value, bool updateLegAbnormal)
{
    const ButtonModbusMapping::Binding parkBinding = buttonModbusBinding(QStringLiteral("techBtn_AGV_Park"));
    const int onBit = parkBinding.reads.isEmpty()
        ? 3
        : ButtonModbusMapping::bitOr(parkBinding.reads.first(), 3);
    const int offBit = parkBinding.reads.size() > 1
        ? ButtonModbusMapping::bitOr(parkBinding.reads.at(1), 4)
        : 4;

    const bool bit7 = (((value >> 7) & 0x01) == 1);
    if (updateLegAbnormal) {
        m_agvLegAbnormal51Bit7Flag = bit7;
    }

    const bool bit3 = (((value >> onBit) & 0x01) == 1);
    const bool bit4 = (((value >> offBit) & 0x01) == 1);

    const bool parkingSwitchWaiting = property("parkingSwitchWaiting").toBool();
    const int pendingBit = property("parkingTargetBit").toInt();
    const bool pendingTargetReached = (pendingBit >= 0 && pendingBit <= 15)
                                          ? ((((value >> pendingBit) & 0x01) == 1))
                                          : false;

    if (parkingSwitchWaiting && !pendingTargetReached) {
        return;
    }

    if (bit7) {
        applyAGVParkingLegAbnormalUi();
        if (bit3 != bit4) {
            m_agvParkingEnabled = bit3 && !bit4;
        }
    } else if (bit3 != bit4) {
        const bool parkingEnabled = bit3 && !bit4;
        m_agvParkingEnabled = parkingEnabled;
        applyAGVParkingButtonUi(parkingEnabled);
        applyAGVParkingStatusBarUi(parkingEnabled);
    }

    updateParkingLegAbnormalDialogVisibility();
}

void MainWindow::syncAGVSteeringModeFromRegister50(quint16 value)
{
    if (!m_steeringModeSelector) {
        return;
    }

    const bool bit10 = ((value >> 10) & 0x01);
    const bool bit11 = ((value >> 11) & 0x01);
    const bool bit12 = ((value >> 12) & 0x01);

    SteeringMode mode = m_steeringModeSelector->currentMode();
    bool shouldUpdate = false;

    if (bit11) {
        mode = STEER_LATERAL;
        shouldUpdate = true;
    } else if (bit12) {
        mode = STEER_ROTATE;
        shouldUpdate = true;
    } else if (bit10) {
        if (!isSteeringModeIn123Group(mode)) {
            mode = steeringModeForReg50Bit10(m_lastSteeringMode);
            shouldUpdate = true;
        }
    } else {
        return;
    }

    if (!shouldUpdate || mode == m_steeringModeSelector->currentMode()) {
        return;
    }

    const QSignalBlocker blocker(m_steeringModeSelector);
    m_steeringModeSelector->setCurrentMode(mode);
    m_lastSteeringMode = mode;

    QLabel *steeringLabel = ui && ui->statusBar
                                 ? ui->statusBar->findChild<QLabel*>("statusBarSteeringLabel")
                                 : nullptr;
    if (steeringLabel) {
        steeringLabel->setText(QString("转向:%1").arg(m_steeringModeSelector->modeText(mode)));
        steeringLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
    }
}

void MainWindow::pollEnableButton()
{
    if (m_enableButtonFd < 0) {
        return;
    }

    char data[8];
    ssize_t bytesRead = read(m_enableButtonFd, data, sizeof(data));

    // 调试输出
    static int debugCount = 0;
    if (debugCount++ % 20 == 0) {  // 每20次输出一次，避免日志过多
        qCDebug(lcMainWindow) << "轮询使能按钮，读取结果:" << bytesRead << "字节";
    }

    if (bytesRead != sizeof(data)) {
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下没有数据是正常的
                return;
            }
            qWarning() << "读取使能按钮数据失败:" << strerror(errno);
        } else if (bytesRead > 0) {
            qWarning() << "读取到不完整数据:" << bytesRead << "字节";
        }
        return;
    }

    // 转换为字符串以便调试
    QString dataStr;
    for (int i = 0; i < 8; i++) {
        dataStr.append(QChar(data[i]));
    }

    qCDebug(lcMainWindow) << "收到使能按钮数据:" << dataStr;

    // 解析状态
    // 根据规格书和你的测试，byte[0] 是 '1' 或 '0'
    bool enabled = (data[0] == '1');

    // 详细调试
    qCDebug(lcMainWindow) << "解析使能按钮状态: data[0] = " << data[0]
             << " (ASCII: " << (int)data[0] << ")"
             << " -> " << (enabled ? "按下" : "松开");

    // 如果状态发生变化
    if (enabled != m_lastEnableButtonState) {
        m_lastEnableButtonState = enabled;

        qCDebug(lcMainWindow) << "=== 使能按钮状态变化 ==="
                 << (enabled ? "按下" : "松开");

        // 更新状态栏
        QString statusText = enabled ? "使能按钮: 按下" : "使能按钮: 松开";
        ui->statusBar->showMessage(statusText, 2000);

        // 处理使能状态变化
        processEnableButton(enabled);

        // 记录到操作记录
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "系统";
        record.controlName = "使能按钮";
        record.controlType = "EnableButton";
        record.operation = "状态变化";
        record.oldValue = !enabled;
        record.newValue = enabled;
        m_recorder->addRecord(record);
    }
}


void MainWindow::onEnableButtonActivated(int socket)
{
    qCDebug(lcMainWindow) << "=== 使能按钮读取触发 ===";

    char data[8] = {0};
    ssize_t bytesRead = read(socket, data, sizeof(data));

    qCDebug(lcMainWindow) << "读取到" << bytesRead << "字节数据";

    if (bytesRead < 0) {
        if (errno == EAGAIN) {
            qCDebug(lcMainWindow) << "没有数据（非阻塞模式正常返回）";
            return;
        }
        qWarning() << "读取使能按钮数据失败:" << strerror(errno);
        return;
    }

    // 打印所有字节的16进制值
    QString hexData;
    for (int i = 0; i < bytesRead; i++) {
        hexData += QString("0x%1 ").arg((unsigned char)data[i], 2, 16, QChar('0')).toUpper();
    }
    qCDebug(lcMainWindow) << "原始数据（十六进制）:" << hexData;

    // 打印每个字节的二进制值
    QString binaryData;
    for (int i = 0; i < bytesRead; i++) {
        binaryData += QString("byte[%1]: ").arg(i);
        for (int bit = 7; bit >= 0; bit--) {
            binaryData += ((data[i] >> bit) & 1) ? "1" : "0";
        }
        binaryData += " ";
    }
    qCDebug(lcMainWindow) << "原始数据（二进制）:" << binaryData;

    // 解析使能按钮状态（根据规格书，byte 0表示S1开关）
    bool enabled = (data[0] == 1);

    qCDebug(lcMainWindow) << "解析结果: byte[0] =" << (int)data[0] << "->" << (enabled ? "激活" : "未激活");

    // 如果状态发生变化
    if (enabled != m_lastEnableButtonState) {
        m_lastEnableButtonState = enabled;

        // 记录状态变化
        qCDebug(lcMainWindow) << "使能按钮状态变化:" << (enabled ? "激活" : "未激活");

        // 更新状态栏
        ui->statusBar->showMessage(QString("使能按钮: %1").arg(enabled ? "激活" : "未激活"), 2000);

        // 处理使能状态变化
        processEnableButton(enabled);

        // 记录到操作记录
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "系统";
        record.controlName = "使能按钮";
        record.controlType = "EnableButton";
        record.operation = "状态变化";
        record.oldValue = !enabled;
        record.newValue = enabled;
        m_recorder->addRecord(record);
    } else {
        qCDebug(lcMainWindow) << "状态未变化，忽略";
    }
}




/**
 * @brief 处理来自外部使能按钮的状态变化
 * @param enabled 是否处于使能/按下状态
 *
 * 根据当前步进/点动模式分别处理使能按键的行为，包含发送 Modbus 写命令
 * 以启用/禁用运动控制，并记录操作。
 */
void MainWindow::processEnableButton(bool enabled)
{
    if (!m_stepModeEnabled && !m_isJointMode) {
        if (enabled) {
            qCDebug(lcMainWindow) << "使能按钮按下忽略：当前非关节模式且非步进模式";
        }
        return;
    }

    if (m_stepModeEnabled) {
        // 步进模式下的使能按钮处理
        if (enabled) {
            onEnableButtonPressedStepMode();
        } else {
            onEnableButtonReleasedStepMode();
        }
    } else {
        // 原有的点动模式处理逻辑
        if (enabled) {
            // 使能按钮按下（激活）时的处理
            qCDebug(lcMainWindow) << "外部使能按钮激活，允许运动控制";

            // 执行原来的 onEnableButtonPressed 逻辑
            // writeToMainDevice(19, 1);
            // writeToMainDevice(119, 1);

            qCDebug(lcMainWindow) << "使能按钮按下，跳过地址19和119写入";
            ui->statusBar->showMessage("使能按钮按下，运动控制已激活", 2000);
        } else {
            // 使能按钮释放（未激活）时的处理
            qCDebug(lcMainWindow) << "外部使能按钮未激活，禁止运动控制";

            // 执行原来的 onEnableButtonReleased 逻辑
            // writeToMainDevice(19, 0);
            // writeToMainDevice(119, 0);

            qCDebug(lcMainWindow) << "使能按钮释放，跳过地址19和119写入";
            ui->statusBar->showMessage("使能按钮释放，运动控制已禁用", 2000);
        }
    }

}

// 修改 MainWindow::performStartupWrites() 函数
void MainWindow::performStartupWrites()
{
    qCDebug(lcMainWindow) << "=== 执行开机写寄存器流程 ===";

    bool executedAnyAction = false;

    // 启动清除伺服报警（地址29脉冲写入）
    if (isFeatureEnabled("startup_checks", "startup.clear_servo_alarm")) {
        writeToMainDevice(290, 1);
        QTimer::singleShot(120, this, [this]() {
            writeToMainDevice(290, 0);
        });
        executedAnyAction = true;
        qCDebug(lcMainWindow) << "已执行启动清除伺服报警动作";
    }

    if (!executedAnyAction) {
        qCDebug(lcMainWindow) << "未配置任何启动写寄存器子动作，流程结束";
    }
}

// 修改 writeToAGVDevice 函数以支持负数（如果需要）
/**
 * @brief 向 AGV 设备写入单个寄存器（支持负值转换）
 * @param address 寄存器地址
 * @param value 要写入的数值（可以为负数，内部会转换为补码）
 * @note 若未连接会尝试延迟重试
 */
bool MainWindow::writeToAGVDevice(int address, int value, bool bypassWirelessWarning)
{
    if (!isFeatureEnabled("modbus_agv", "modbus_agv.write_enabled")) {
        showModbusWriteDisabledToast();
        return false;
    }

    if (!m_agvModbusManager || !m_agvModbusManager->isConnected()) {
        if (!m_agvDisconnectedWarnedAddresses.contains(address)) {
            qWarning() << "AGV Modbus未连接，无法写入地址" << address;
            m_agvDisconnectedWarnedAddresses.insert(address);
        }
        return false;
    }

    m_agvDisconnectedWarnedAddresses.remove(address);

    const bool teachingWriteAllowed = verifyTeachingWriteGateOrShowDialog();
    if (!bypassWirelessWarning && m_controlMode == WIRELESS_MODE) {
        showWirelessModeWarningDialog();
    }
    if (!teachingWriteAllowed) {
        return false;
    }

    if (isFeatureEnabled("modbus_agv", "modbus_agv.write_logs")) {
        qCDebug(lcMainWindow) << "[AGV] 写入地址:" << address << "值:" << value;
    }

    // 转换负数为无符号数（如果需要）
    quint16 writeValue;
    if (value < 0) {
        writeValue = static_cast<quint16>(65536 + value);  // 负数转换为补码
        if (isFeatureEnabled("modbus_agv", "modbus_agv.write_logs")) {
            qCDebug(lcMainWindow) << "[AGV] 负数转换:" << value << "->" << writeValue;
        }
    } else {
        writeValue = static_cast<quint16>(value);
    }

    // 执行写入
    bool writeSuccess = m_agvModbusManager->writeSingleRegister(address, writeValue);

    if (!writeSuccess) {
        qWarning() << "[AGV] 写入请求发送失败 - 地址:" << address;
        return false;
    }
    m_agvRegisterShadow[address] = writeValue;
    return true;
}

bool MainWindow::writeAGVRegisterBits(int address,
                                      const QList<QPair<int, bool>> &bitUpdates,
                                      const QString &scene,
                                      bool bypassWirelessWarning)
{
    if (!isFeatureEnabled("modbus_agv", "modbus_agv.write_enabled")) {
        showModbusWriteDisabledToast();
        return false;
    }

    if (address < 0 || address > 65535) {
        qWarning() << "[AGV按位写入] 非法寄存器地址:" << address;
        return false;
    }

    if (bitUpdates.isEmpty()) {
        qWarning() << "[AGV按位写入] 未提供任何位更新，已拒绝写入";
        return false;
    }

    const bool teachingWriteAllowed = verifyTeachingWriteGateOrShowDialog();
    if (!bypassWirelessWarning && m_controlMode == WIRELESS_MODE) {
        showWirelessModeWarningDialog();
    }
    if (!teachingWriteAllowed) {
        return false;
    }

    quint16 baseValue = m_agvRegisterShadow.value(address, 0);
    quint16 newValue = baseValue;

    for (const auto &bitUpdate : bitUpdates) {
        const int bit = bitUpdate.first;
        const bool set = bitUpdate.second;
        if (bit < 0 || bit > 15) {
            qWarning() << "[AGV按位写入] 非法位索引:" << bit;
            return false;
        }

        if (set) {
            newValue = static_cast<quint16>(newValue | (static_cast<quint16>(1u) << bit));
        } else {
            newValue = static_cast<quint16>(newValue & ~(static_cast<quint16>(1u) << bit));
        }
    }

    bool ok = m_agvModbusManager && m_agvModbusManager->writeSingleRegister(address, newValue);
    if (ok) {
        m_agvRegisterShadow[address] = newValue;
        if (isFeatureEnabled("modbus_agv", "modbus_agv.write_logs")) {
            qCDebug(lcMainWindow) << "[AGV按位写入]" << (scene.isEmpty() ? QString("场景未命名") : scene)
                                  << "地址:" << address
                                  << "基值:" << baseValue
                                  << "新值:" << newValue;
        }
    } else {
        qWarning() << "[AGV按位写入失败]" << (scene.isEmpty() ? QString("场景未命名") : scene)
                   << "地址:" << address
                   << "基值:" << baseValue
                   << "目标值:" << newValue;
    }
    return ok;
}

void MainWindow::syncAgvHostEmergencyStopCommand(bool emergencyActive)
{
    const bool bit6Normal = !emergencyActive;
    if (m_agvHostEstopCommandSynced && m_agvHostEstopCommandBit6Normal == bit6Normal) {
        return;
    }
    if (!m_agvModbusManager || !m_agvModbusManager->isConnected()) {
        return;
    }

    ModbusThreadManager *gateMgr = m_modbusManager ? m_modbusManager : ModbusThreadManager::instance();
    if (!ModbusWriteGate::verifyWriteAllowed(gateMgr)) {
        return;
    }

    const bool ok = writeAGVRegisterBits(0,
                                         { qMakePair(6, bit6Normal) },
                                         emergencyActive ? QStringLiteral("急停命令HR0.6=0")
                                                         : QStringLiteral("急停恢复HR0.6=1"),
                                         true);
    if (ok) {
        m_agvHostEstopCommandSynced = true;
        m_agvHostEstopCommandBit6Normal = bit6Normal;
        qCDebug(lcMainWindow) << "AGV HR0.6 急停命令已写入:" << (bit6Normal ? 1 : 0)
                              << (emergencyActive ? "(急停)" : "(正常)");
    } else {
        m_agvHostEstopCommandSynced = false;
        qWarning() << "AGV HR0.6 急停命令写入失败，目标bit6=" << (bit6Normal ? 1 : 0);
    }
}

void MainWindow::writeToMainDevice(int address, int value)
{
    if (!isFeatureEnabled("modbus_main", "modbus_main.write_enabled")) {
        showModbusWriteDisabledToast();
        return;
    }

    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        qWarning() << "主Modbus未连接，无法写入地址" << address;
        return;
    }

    if (isFeatureEnabled("modbus_main", "modbus_main.write_logs")) {
        qCDebug(lcMainWindow) << "[主设备] 写入地址:" << address << "(&MB" << (address + 1) << ")"
                 << "值:" << value;
    }

    MainDeviceModbusApi::writeRegister(m_modbusManager, address, value);
}
void MainWindow::onControlModeClicked()
{
    if (!isFeatureEnabled("motion_control", "motion.control_mode_switch")) {
        showNotification("控制模式切换功能已关闭");
        return;
    }
    if (isRobotWeightLockGateActive()) {
        blockRobotWeightLockOperation(QStringLiteral("负载超重锁定：控制模式切换已无效"));
        return;
    }

    const ButtonModbusMapping::Binding modeBinding = buttonModbusBinding(QStringLiteral("TBtn_ControlMode"));
    const ModbusRegisterSpec writeSpec = modeBinding.writes.isEmpty()
        ? ModbusRegisterSpec{}
        : modeBinding.writes.first();
    const int writeAddr = ButtonModbusMapping::addressOr(writeSpec, 500);
    const int wirelessValue = ButtonModbusMapping::stateValueOr(writeSpec, 1, 1);
    const int wiredValue = ButtonModbusMapping::stateValueOr(writeSpec, 2, 2);
    const auto writeMode = [this, writeSpec, writeAddr](int value) {
        if (writeSpec.device == QStringLiteral("主控")) {
            writeToMainDevice(writeAddr, value);
        } else {
            writeToAGVDevice(writeAddr, value);
        }
    };

    // 切换控制模式
    if (m_controlMode == WIRED_MODE) {
        m_controlMode = WIRELESS_MODE;
        m_controlModeBtn->setText("遥控器控制");

        writeMode(wirelessValue);

        qCDebug(lcMainWindow) << "切换到遥控器控制模式";
        ui->statusBar->showMessage("已切换到遥控器控制模式", 2000);

        // 更新状态栏显示
        QLabel *controlModeLabel = ui->statusBar->findChild<QLabel*>("statusBarControlModeLabel");
        if (controlModeLabel) {
            controlModeLabel->setText("遥控器控制");
            controlModeLabel->setStyleSheet("color: #ffff00; font-weight: bold; font-size: 11px;");
        }
    } else {
        m_controlMode = WIRED_MODE;
        m_controlModeBtn->setText("示教器控制");

        writeMode(wiredValue);

        qCDebug(lcMainWindow) << "切换到示教器控制模式";
        ui->statusBar->showMessage("已切换到示教器控制模式", 2000);

        // 更新状态栏显示
        QLabel *controlModeLabel = ui->statusBar->findChild<QLabel*>("statusBarControlModeLabel");
        if (controlModeLabel) {
            controlModeLabel->setText("示教器控制");
            controlModeLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 11px;");
        }
    }

    updateFunctionSwitchVisuals();
    updateStepTargetButtonsState();

    if (!m_stepModeUnknown && m_stepModeEnabled) {
        int targetCode = 1;
        const int targetReg = selectedStepTargetRegister();
        if (targetReg == 501) targetCode = 2;
        else if (targetReg == 502) targetCode = 3;
        else if (targetReg == 503) targetCode = 4;
        else if (targetReg == 504) targetCode = 5;
        writeToMainDevice(500, targetCode);
    }

}
// void MainWindow::onEnableButtonPressed()
// {
//     // 给192.168.1.13的19地址写1
//     writeToMainDevice(19, 1);
//     // 给192.168.1.13的119地址写1
//     writeToMainDevice(119, 1);

//     qCDebug(lcMainWindow) << "使能按钮按下，地址119写入1";
//     ui->statusBar->showMessage("使能按钮按下", 1000);
// }

// void MainWindow::onEnableButtonReleased()
// {
//     // 给192.168.1.13的119地址写0
//     writeToMainDevice(119, 0);

//     qCDebug(lcMainWindow) << "使能按钮释放，地址119写入0";
//     ui->statusBar->showMessage("使能按钮释放", 1000);
// }
QString MainWindow::getCurrentPageName() const
{
    int currentIndex = ui->StackedWidget->currentIndex();
    return m_pageNames.value(currentIndex, QString("未知页面"));
}
// 设置AGV避障开关控制
void MainWindow::setupAGVOAControl()
{
    if (!isBigFeatureEnabled("motion_control")) {
        return;
    }

    // 查找避障开关按钮
    m_techBtnAGV_OA = findChild<TechPushButton*>("techBtn_AGV_OA");
    if (m_techBtnAGV_OA) {
        // 初始状态为避障开启
        m_agvOaEnabled = true;
        m_techBtnAGV_OA->setText("避障开启");
        m_techBtnAGV_OA->setPrimaryColor(QColor("#00C8FF"));
        m_techBtnAGV_OA->setGlowColor(QColor(0, 200, 255, 180));

        // 连接点击信号
        connect(m_techBtnAGV_OA, &TechPushButton::clicked,
            this, &MainWindow::onAGVOABtnClicked,
            Qt::UniqueConnection);

        qCDebug(lcMainWindow) << "AGV避障开关按钮初始化完成";
    } else {
        qWarning() << "未找到techBtn_AGV_OA按钮";
    }

    // 查找驻车按钮
    m_techBtnAGV_Park = findChild<TechPushButton*>("techBtn_AGV_Park");
    if (m_techBtnAGV_Park) {
        m_agvParkingEnabled = false;
        applyAGVParkingButtonUi(false);

        connect(m_techBtnAGV_Park, &TechPushButton::clicked,
            this, &MainWindow::onAGVParkBtnClicked,
            Qt::UniqueConnection);

        qCDebug(lcMainWindow) << "AGV驻车按钮初始化完成";
    } else {
        qWarning() << "未找到techBtn_AGV_Park按钮";
    }

    // 备用按钮：仅 UI 两态（第二态可配置为变暗）
    m_techBtnSpare1 = findChild<TechPushButton*>(QStringLiteral("techBtn_spare_1"));
    if (m_techBtnSpare1) {
        m_spareButtonDefaultFirstText.insert(m_techBtnSpare1->objectName(), m_techBtnSpare1->text());
        m_techBtnSpare1->setProperty("spareSecondState", false);
        m_techBtnSpare1->setProperty("spareFirstText", m_techBtnSpare1->text());
        connect(m_techBtnSpare1, &TechPushButton::clicked, this, &MainWindow::onSpareButtonClicked, Qt::UniqueConnection);
    }
    m_techBtnSpare2 = findChild<TechPushButton*>(QStringLiteral("techBtn_spare_2"));
    if (m_techBtnSpare2) {
        m_spareButtonDefaultFirstText.insert(m_techBtnSpare2->objectName(), m_techBtnSpare2->text());
        m_techBtnSpare2->setProperty("spareSecondState", false);
        m_techBtnSpare2->setProperty("spareFirstText", m_techBtnSpare2->text());
        connect(m_techBtnSpare2, &TechPushButton::clicked, this, &MainWindow::onSpareButtonClicked, Qt::UniqueConnection);
    }
    loadSpareButtonNameRegisterSettings();
    applySpareButtonRuntimeSettings();

    if (ui->LEdit_AGV_EstimatedWeight) {
        applyEstimatedWeightRuntimeSettings();
        connect(ui->LEdit_AGV_EstimatedWeight, &QLineEdit::editingFinished, this, [this]() {
            if (!ui || !ui->LEdit_AGV_EstimatedWeight) {
                return;
            }
            const QString text = ui->LEdit_AGV_EstimatedWeight->text().trimmed();
            if (text.isEmpty()) {
                return;
            }
            const QPair<int, int> lim = estimatedWeightLimits();
            bool ok = false;
            int v = text.toInt(&ok);
            if (!ok) {
                ui->LEdit_AGV_EstimatedWeight->clear();
                return;
            }
            const int c = qBound(lim.first, v, lim.second);
            if (c != v || !ok) {
                ui->LEdit_AGV_EstimatedWeight->setText(QString::number(c));
            }
            if (!writeToAGVDevice(kAgvEstimatedWeightReg, c)) {
                if (ui->statusBar) {
                    ui->statusBar->showMessage(QStringLiteral("预计负载写入寄存器157失败"), 4000);
                }
            }
        }, Qt::UniqueConnection);
    }

}

// 设置AGV运动速度控制
void MainWindow::setupAGVMoveSpeedControl()
{
    if (!isBigFeatureEnabled("motion_control")) {
        return;
    }

    m_editAGV_MoveSpeed = findChild<TechSliderEdit*>("SEdit_AGV_MoveSpeed");
    if (m_editAGV_MoveSpeed) {
        m_editAGV_MoveSpeed->setLabelText("全向平台速度");
        m_editAGV_MoveSpeed->setSuffix("mm/s");
        m_editAGV_MoveSpeed->setPrecision(0);

        connect(m_editAGV_MoveSpeed, &TechSliderEdit::valueChanged,
                this, &MainWindow::onAGVMoveSpeedChanged);

        qCDebug(lcMainWindow) << "AGV运动速度控件初始化完成，范围:"
             << m_editAGV_MoveSpeed->minimum() << "~" << m_editAGV_MoveSpeed->maximum();
    } else {
        qWarning() << "未找到SEdit_AGV_MoveSpeed控件";
    }
}


// 设置AGV转向角度控制
void MainWindow::setupAGVAngleControl()
{
    if (!isBigFeatureEnabled("motion_control")) {
        return;
    }

    m_editAGV_Angle = findChild<TechSliderEdit*>("SEdit_AGV_Angle");
    if (m_editAGV_Angle) {
        m_editAGV_Angle->setLabelText("底盘当前角度");
        m_editAGV_Angle->setSuffix("°");
        m_editAGV_Angle->setPrecision(0);
        m_editAGV_Angle->setPresetButtonsVisible(false);

        connect(m_editAGV_Angle, &TechSliderEdit::valueChanged,
                this, &MainWindow::onAGVAngleChanged);

        qCDebug(lcMainWindow) << "AGV转向角度控件初始化完成，范围:"
             << m_editAGV_Angle->minimum() << "~" << m_editAGV_Angle->maximum();
    } else {
        qWarning() << "未找到SEdit_AGV_Angle控件";
    }
}

void MainWindow::applySpareButtonRuntimeSettings()
{
    auto applyOne = [](TechPushButton *btn) {
        if (!btn) {
            return;
        }
        const bool secondState = btn->property("spareSecondState").toBool();
        const QString firstText = btn->property("spareFirstText").toString().trimmed().isEmpty()
            ? btn->text()
            : btn->property("spareFirstText").toString();
        const QString configuredSecondText = btn->property("spareSecondText").toString().trimmed();
        const QString secondText = configuredSecondText.isEmpty()
            ? firstText + QStringLiteral(" (第二态)")
            : configuredSecondText;
        const bool dimEnabled = spareButtonSecondStateDarkeningEnabled(btn->objectName());
        applyTwoStateButtonStyle(btn, secondState, dimEnabled, firstText, secondText);
    };
    applyOne(m_techBtnSpare1);
    applyOne(m_techBtnSpare2);
}

void MainWindow::loadSpareButtonNameRegisterSettings()
{
    m_spareButtonNameBindings.clear();

    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ButtonModbusMapping"));

    const QStringList spareNames = {
        QStringLiteral("techBtn_spare_1"),
        QStringLiteral("techBtn_spare_2")
    };

    const auto parseSpec = [&settings](const QString &prefix) -> SpareButtonNameRegisterSpec {
        SpareButtonNameRegisterSpec spec;
        spec.device = settings.value(prefix + QStringLiteral("_device"), QStringLiteral("无")).toString().trimmed();
        if (spec.device.isEmpty()) {
            spec.device = QStringLiteral("无");
        }
        bool ok = false;
        const int addr = settings.value(prefix + QStringLiteral("_addr")).toString().trimmed().toInt(&ok);
        spec.startAddress = ok ? addr : -1;
        return spec;
    };

    for (const QString &name : spareNames) {
        SpareButtonNameRegisterBinding binding;
        binding.state1 = parseSpec(name + QStringLiteral("_name1"));
        binding.state2 = parseSpec(name + QStringLiteral("_name2"));
        if (binding.state1.isConfigured() || binding.state2.isConfigured()) {
            m_spareButtonNameBindings.insert(name, binding);
        }
    }

    settings.endGroup();
}

bool MainWindow::readModbusUtf8StringRegisters(const QString &device,
                                               int startAddress,
                                               QString &textOut) const
{
    textOut.clear();
    if (startAddress < 0) {
        return false;
    }

    QVector<quint16> regs;
    if (device == QStringLiteral("主控")) {
        if (!MainDeviceModbusApi::readHoldingRegistersSync(m_modbusManager,
                                                         startAddress,
                                                         kModbusUtf8StringRegisterCount,
                                                         regs,
                                                         nullptr)) {
            return false;
        }
    } else if (device == QStringLiteral("AGV")) {
        if (!m_agvModbusManager || !m_agvModbusManager->isConnected()) {
            return false;
        }
        if (!m_agvModbusManager->readHoldingRegistersSync(startAddress,
                                                          kModbusUtf8StringRegisterCount,
                                                          regs)) {
            return false;
        }
    } else {
        return false;
    }

    if (regs.size() < kModbusUtf8StringRegisterCount) {
        return false;
    }

    textOut = decodeUtf8FromRegisters(regs);
    return true;
}

void MainWindow::syncSpareButtonNamesFromRegisters()
{
    if (m_spareButtonNameBindings.isEmpty()) {
        return;
    }

    const auto syncOne = [this](TechPushButton *btn) {
        if (!btn) {
            return;
        }
        const QString objectName = btn->objectName();
        if (!m_spareButtonNameBindings.contains(objectName)) {
            return;
        }

        const SpareButtonNameRegisterBinding binding = m_spareButtonNameBindings.value(objectName);
        const QString defaultFirst = m_spareButtonDefaultFirstText.value(
            objectName,
            MappingConfig::instance()->mapControlName(objectName));

        QString firstText = defaultFirst;
        if (binding.state1.isConfigured()) {
            QString decoded;
            if (readModbusUtf8StringRegisters(binding.state1.device, binding.state1.startAddress, decoded)
                && !decoded.isEmpty()) {
                firstText = decoded;
            }
        }
        btn->setProperty("spareFirstText", firstText);

        QString secondText;
        if (binding.state2.isConfigured()) {
            QString decoded;
            if (readModbusUtf8StringRegisters(binding.state2.device, binding.state2.startAddress, decoded)
                && !decoded.isEmpty()) {
                secondText = decoded;
            }
        }
        if (secondText.isEmpty()) {
            secondText = firstText + QStringLiteral(" (第二态)");
        }
        btn->setProperty("spareSecondText", secondText);
    };

    syncOne(m_techBtnSpare1);
    syncOne(m_techBtnSpare2);
    applySpareButtonRuntimeSettings();
}

void MainWindow::executeSpareButtonConfiguredWrites(const QString &buttonObjectName, int stateIndex)
{
    executeConfiguredRegisterWrites(buttonModbusBinding(buttonObjectName).writes,
                                    stateIndex,
                                    QStringLiteral("备用按钮%1状态%2").arg(buttonObjectName).arg(stateIndex));
}

void MainWindow::executeConfiguredRegisterWrites(const QList<ModbusRegisterSpec> &specs,
                                                 int stateIndex,
                                                 const QString &logTag)
{
    for (const ModbusRegisterSpec &spec : specs) {
        if (!spec.isConfigured() || isParkLengthWriteSpec(spec)) {
            continue;
        }
        int address = 0;
        if (!ButtonModbusMapping::parseNumber(spec.address, address)) {
            continue;
        }
        const QString valueText = pickStateValue(spec, stateIndex);
        int valueInt = 0;
        const bool hasValue = ButtonModbusMapping::parseNumber(valueText, valueInt);

        const bool isAgv = spec.device == QStringLiteral("AGV");
        const bool isMain = spec.device == QStringLiteral("主控");

        int bitIndex = -1;
        const bool hasBit = ButtonModbusMapping::parseNumber(spec.bit, bitIndex)
            && bitIndex >= 0 && bitIndex <= 15;

        if (hasBit && !hasValue) {
            continue;
        }

        if (isAgv) {
            if (hasBit) {
                writeAGVRegisterBits(address, {qMakePair(bitIndex, valueInt != 0)}, logTag);
            } else if (hasValue) {
                writeToAGVDevice(address, valueInt, true);
            }
        } else if (isMain) {
            if (hasBit) {
                if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
                    continue;
                }
                quint16 cur = 0;
                if (!m_modbusManager->readSingleRegister(address, cur)) {
                    continue;
                }
                quint16 next = cur;
                if (valueInt != 0) {
                    next = static_cast<quint16>(next | (static_cast<quint16>(1u) << bitIndex));
                } else {
                    next = static_cast<quint16>(next & ~(static_cast<quint16>(1u) << bitIndex));
                }
                writeToMainDevice(address, next);
            } else if (hasValue) {
                writeToMainDevice(address, valueInt);
            }
        }
    }
}

void MainWindow::onSpareButtonClicked()
{
    TechPushButton *btn = qobject_cast<TechPushButton*>(sender());
    if (!btn) {
        return;
    }
    const bool secondState = !btn->property("spareSecondState").toBool();
    btn->setProperty("spareSecondState", secondState);
    applySpareButtonRuntimeSettings();
    executeSpareButtonConfiguredWrites(btn->objectName(), secondState ? 2 : 1);
}

// AGV避障开关按钮点击槽函数
void MainWindow::onAGVOABtnClicked()
{
    if (!isFeatureEnabled("motion_control", "motion.agv_oa_switch")) {
        showNotification("AGV避障开关功能已关闭");
        return;
    }

    if (property("oaSwitchBusy").toBool()) {
        ui->statusBar->showMessage("避障切换进行中，请稍候...", 1500);
        qWarning() << "OA切换请求被忽略：上一条指令仍在确认中";
        return;
    }

    setProperty("oaSwitchBusy", true);

    const bool previousOaEnabled = m_agvOaEnabled;
    const bool targetOaEnabled = !previousOaEnabled;
    m_agvOaEnabled = targetOaEnabled;
    setProperty("oaTargetEnabled", targetOaEnabled);

    const ButtonModbusMapping::Binding oaBinding = buttonModbusBinding(QStringLiteral("techBtn_AGV_OA"));
    const ModbusRegisterSpec oaWrite = oaBinding.writes.isEmpty()
        ? ModbusRegisterSpec{}
        : oaBinding.writes.first();
    const int oaWriteAddr = ButtonModbusMapping::addressOr(oaWrite, 0);
    const int oaWriteBit = ButtonModbusMapping::bitOr(oaWrite, 1);
    const int oaOnValue = ButtonModbusMapping::stateValueOr(oaWrite, 1, 0);
    const int oaOffValue = ButtonModbusMapping::stateValueOr(oaWrite, 2, 1);
    const int oaCommandValue = targetOaEnabled ? oaOnValue : oaOffValue;
    const bool oaCommandBitSet = oaCommandValue != 0;

    if (targetOaEnabled) {
        // 避障开启
        m_techBtnAGV_OA->setText("避障开启");
        m_techBtnAGV_OA->setPrimaryColor(QColor("#00C8FF"));
        m_techBtnAGV_OA->setGlowColor(QColor(0, 200, 255, 180));
    } else {
        // 避障关闭
        m_techBtnAGV_OA->setText("避障关闭");
        m_techBtnAGV_OA->setPrimaryColor(QColor("#7F8C8D"));
        m_techBtnAGV_OA->setGlowColor(QColor(127, 140, 141, 100));
    }

    const bool writeOk = writeAGVRegisterBits(oaWriteAddr,
                                              {
                                                  qMakePair(oaWriteBit, oaCommandBitSet),
                                              },
                                              targetOaEnabled ? "OA开启" : "OA关闭");
    if (!writeOk) {
        m_agvOaEnabled = previousOaEnabled;
        if (m_techBtnAGV_OA) {
            m_techBtnAGV_OA->setText(previousOaEnabled ? "避障开启" : "避障关闭");
            m_techBtnAGV_OA->setPrimaryColor(previousOaEnabled ? QColor("#00C8FF") : QColor("#7F8C8D"));
            m_techBtnAGV_OA->setGlowColor(previousOaEnabled ? QColor(0, 200, 255, 180)
                                                            : QColor(127, 140, 141, 100));
        }
        ui->statusBar->showMessage("AGV避障切换失败：写入未发送", 3000);
        qWarning() << "AGV避障切换失败：写入请求发送失败，地址" << oaWriteAddr << "bit" << oaWriteBit << "=" << oaCommandBitSet;
        setProperty("oaSwitchBusy", false);
        return;
    }

    if (targetOaEnabled) {
        qCDebug(lcMainWindow) << "AGV避障开启，地址0按位更新(bit1=0)";
        ui->statusBar->showMessage("AGV避障开启", 2000);
    } else {
        qCDebug(lcMainWindow) << "AGV避障关闭，地址0按位更新(bit1=1)";
        ui->statusBar->showMessage("AGV避障关闭", 2000);
    }

    QLabel *oaLabel = ui && ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarOaLabel") : nullptr;
    if (oaLabel) {
        oaLabel->setText(m_agvOaEnabled ? "避障开" : "避障关");
        oaLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                   .arg(m_agvOaEnabled ? "#00C8FF" : "#7F8C8D"));
    }

    // 写后做一次回读确认，目标设备偶发丢响应时自动补发一次。
    QTimer::singleShot(250, this, [this, oaWriteAddr]() {
        if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
            m_agvModbusManager->readMultipleRegisters(oaWriteAddr, 1);
        }
    });

    QTimer::singleShot(650, this, [this, targetOaEnabled, oaWriteAddr, oaWriteBit, oaOnValue, oaCommandBitSet]() {
        if (property("oaTargetEnabled").toBool() != targetOaEnabled) {
            // 已有更新的目标请求。
            return;
        }

        const quint16 reg0 = m_agvRegisterShadow.value(oaWriteAddr, 0);
        const bool actualOaEnabled = (((reg0 >> oaWriteBit) & 0x01) == (oaOnValue != 0 ? 1 : 0));
        if (actualOaEnabled == targetOaEnabled) {
            setProperty("oaSwitchBusy", false);
            return;
        }

        qWarning() << "OA回读不一致，准备自动重试。目标bit=" << oaCommandBitSet
                   << "实际寄存器=" << reg0;

        const bool retryOk = writeAGVRegisterBits(oaWriteAddr,
                                                  {
                                                      qMakePair(oaWriteBit, oaCommandBitSet),
                                                  },
                                                  targetOaEnabled ? "OA开启自动重试" : "OA关闭自动重试");
        if (!retryOk) {
            qWarning() << "OA自动重试发送失败";
            ui->statusBar->showMessage("AGV避障自动重试失败，请检查网络/控制器状态", 4000);
            setProperty("oaSwitchBusy", false);
            return;
        }

        if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
            m_agvModbusManager->readMultipleRegisters(oaWriteAddr, 1);
        }

        QTimer::singleShot(350, this, [this, targetOaEnabled, oaWriteAddr, oaWriteBit, oaOnValue]() {
            if (property("oaTargetEnabled").toBool() != targetOaEnabled) {
                return;
            }
            const quint16 reg0AfterRetry = m_agvRegisterShadow.value(oaWriteAddr, 0);
            const bool actualAfterRetry = (((reg0AfterRetry >> oaWriteBit) & 0x01) == (oaOnValue != 0 ? 1 : 0));
            if (actualAfterRetry != targetOaEnabled) {
                qWarning() << "OA自动重试后仍未达到目标。当前寄存器=" << reg0AfterRetry;
                ui->statusBar->showMessage("避障状态确认失败，请检查控制器侧地址0写保护/上位覆盖", 4500);
            }
            setProperty("oaSwitchBusy", false);
        });
    });

    // 兜底超时，避免在极端网络条件下锁无法释放。
    QTimer::singleShot(1500, this, [this, targetOaEnabled]() {
        if (!property("oaSwitchBusy").toBool()) {
            return;
        }
        if (property("oaTargetEnabled").toBool() != targetOaEnabled) {
            return;
        }
        qWarning() << "OA切换确认超时，已自动释放在途锁";
        setProperty("oaSwitchBusy", false);
    });
}

void MainWindow::executeAGVParkingSwitch(bool targetParkingEnabled, int legLengthMm)
{
    if (!isFeatureEnabled("motion_control", "motion.agv_park_switch")) {
        showNotification("AGV驻车开关功能已关闭");
        return;
    }

    if (m_controlMode != WIRED_MODE) {
        ui->statusBar->showMessage("当前为无线控制，驻车功能仅在有线控制模式下生效", 3000);
        qWarning() << "驻车请求被拒绝：当前不是有线控制模式";
        return;
    }

    if (property("parkingSwitchWaiting").toBool()) {
        ui->statusBar->showMessage("驻车切换进行中，请等待完成", 2000);
        return;
    }

    const bool oldParkingEnabled = m_agvParkingEnabled;
    const ButtonModbusMapping::Binding parkBinding = buttonModbusBinding(QStringLiteral("techBtn_AGV_Park"));
    int parkLengthAddr = kAgvParkOutTriggerLengthRegStart;
    int parkWriteAddr = 0;
    QList<QPair<int, bool>> parkBitUpdates;
    for (const ModbusRegisterSpec &spec : parkBinding.writes) {
        if (!spec.isConfigured()) {
            continue;
        }
        if (isParkLengthWriteSpec(spec)) {
            parkLengthAddr = ButtonModbusMapping::addressOr(spec, kAgvParkOutTriggerLengthRegStart);
            continue;
        }
        int addr = 0;
        int bit = -1;
        if (!ButtonModbusMapping::parseNumber(spec.address, addr)) {
            continue;
        }
        if (!ButtonModbusMapping::parseNumber(spec.bit, bit) || bit < 0 || bit > 15) {
            continue;
        }
        parkWriteAddr = addr;
        const int stateVal = ButtonModbusMapping::stateValueOr(spec, targetParkingEnabled ? 1 : 2, targetParkingEnabled ? 1 : 0);
        parkBitUpdates.append(qMakePair(bit, stateVal != 0));
    }
    if (parkBitUpdates.isEmpty()) {
        parkWriteAddr = 0;
        if (targetParkingEnabled) {
            parkBitUpdates = {qMakePair(9, true), qMakePair(10, false)};
        } else {
            parkBitUpdates = {qMakePair(9, false), qMakePair(10, true)};
        }
    }
    const int parkReadAddr = parkBinding.reads.isEmpty()
        ? 51
        : ButtonModbusMapping::addressOr(parkBinding.reads.first(), 51);
    const int onBit = parkBinding.reads.isEmpty()
        ? 3
        : ButtonModbusMapping::bitOr(parkBinding.reads.first(), 3);
    const int offBit = parkBinding.reads.size() > 1
        ? ButtonModbusMapping::bitOr(parkBinding.reads.at(1), 4)
        : 4;
    const int targetWaitBit = targetParkingEnabled ? onBit : offBit;

    if (targetParkingEnabled) {
        const QPair<int, int> lim = parkOutTriggerLengthLimitsFromSettings();
        int mm = legLengthMm;
        if (mm < 0) {
            QLineEdit *lenEdit = m_parkingLegAbnormalLengthEdit;
            bool lenOk = false;
            mm = lenEdit ? lenEdit->text().trimmed().toInt(&lenOk) : 0;
            if (!lenOk) {
                mm = 1100;
            }
        }
        const int clampedMm = qBound(lim.first, mm, lim.second);

        if (m_parkingLegAbnormalLengthEdit) {
            m_parkingLegAbnormalLengthEdit->setText(QString::number(clampedMm));
        }

        const auto wordsArr = doubleToRegistersGHEFCDAB(static_cast<double>(clampedMm));
        const QVector<quint16> parkLenWords = {wordsArr[0], wordsArr[1], wordsArr[2], wordsArr[3]};

        if (!writeAgvHoldingRegisterBlock(parkLengthAddr, parkLenWords)) {
            m_agvParkingEnabled = oldParkingEnabled;
            restoreParkingUiAfterFailure(oldParkingEnabled);

            ui->statusBar->showMessage(QStringLiteral("驻车伸出长度写入寄存器失败，未开启驻车"), 4000);
            OperationRecord failRecord;
            failRecord.timestamp = QDateTime::currentDateTime();
            failRecord.pageName = QStringLiteral("AGV控制");
            failRecord.controlName = QStringLiteral("驻车伸出长度写入失败");
            failRecord.controlType = "";
            failRecord.operation = "";
            failRecord.oldValue = "";
            failRecord.newValue = "";
            m_recorder->addRecord(failRecord);
            updateParkingLegAbnormalDialogVisibility();
            return;
        }
    }

    const bool writeOk = writeAGVRegisterBits(parkWriteAddr,
                                              parkBitUpdates,
                                              targetParkingEnabled ? QStringLiteral("驻车开启")
                                                                   : QStringLiteral("驻车关闭"));

    if (!writeOk) {
        m_agvParkingEnabled = oldParkingEnabled;
        restoreParkingUiAfterFailure(oldParkingEnabled);

        ui->statusBar->showMessage(QStringLiteral("AGV驻车指令发送失败"), 3000);
        OperationRecord failRecord;
        failRecord.timestamp = QDateTime::currentDateTime();
        failRecord.pageName = QStringLiteral("AGV控制");
        failRecord.controlName = QStringLiteral("驻车指令发送失败");
        failRecord.controlType = "";
        failRecord.operation = "";
        failRecord.oldValue = "";
        failRecord.newValue = "";
        m_recorder->addRecord(failRecord);
        updateParkingLegAbnormalDialogVisibility();
        return;
    }

    m_agvParkingEnabled = targetParkingEnabled;

    if (!m_agvLegAbnormal51Bit7Flag) {
        applyAGVParkingButtonUi(targetParkingEnabled);
        applyAGVParkingStatusBarUi(targetParkingEnabled);
    }

    ui->statusBar->showMessage(targetParkingEnabled ? QStringLiteral("AGV驻车开启")
                                                    : QStringLiteral("AGV驻车关闭"),
                               2000);

    if (QTimer *oldTimer = findChild<QTimer*>(QStringLiteral("parkingSwitchWaitTimer"))) {
        oldTimer->stop();
        oldTimer->deleteLater();
    }

    hideParkingLegAbnormalDialog();

    setProperty("parkingSwitchWaiting", true);
    setProperty("parkingTargetBit", targetWaitBit);
    setProperty("parkingTargetEnabled", targetParkingEnabled);

    showParkingSwitchHintDialog(QStringLiteral("正在切换驻车模式"));

    auto *parkingWaitTimer = new QTimer(this);
    parkingWaitTimer->setObjectName(QStringLiteral("parkingSwitchWaitTimer"));
    parkingWaitTimer->setInterval(300);
    const qint64 parkingSwitchBeginMs = QDateTime::currentMSecsSinceEpoch();

    connect(parkingWaitTimer, &QTimer::timeout, this,
            [this, parkingWaitTimer, targetWaitBit, targetParkingEnabled, parkingSwitchBeginMs, parkReadAddr]() {
                if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
                    m_agvModbusManager->readMultipleRegisters(parkReadAddr, 1);
                }

                if (m_agvRegisterShadow.contains(parkReadAddr)) {
                    const quint16 reg51 = m_agvRegisterShadow.value(parkReadAddr);
                    const bool targetBitSet = (((reg51 >> targetWaitBit) & 0x01) == 1);
                    if (targetBitSet) {
                        parkingWaitTimer->stop();
                        parkingWaitTimer->deleteLater();
                        setProperty("parkingSwitchWaiting", false);
                        setProperty("parkingTargetBit", -1);
                        hideParkingSwitchHintDialog();
                        updateParkingLegAbnormalDialogVisibility();

                        OperationRecord okRecord;
                        okRecord.timestamp = QDateTime::currentDateTime();
                        okRecord.pageName = QStringLiteral("AGV控制");
                        okRecord.controlName = targetParkingEnabled
                                                     ? QStringLiteral("驻车模式已开启")
                                                     : QStringLiteral("驻车模式已关闭");
                        okRecord.controlType = "";
                        okRecord.operation = "";
                        okRecord.oldValue = "";
                        okRecord.newValue = "";
                        m_recorder->addRecord(okRecord);
                        return;
                    }
                }

                if (QDateTime::currentMSecsSinceEpoch() - parkingSwitchBeginMs >= 90000) {
                    parkingWaitTimer->stop();
                    parkingWaitTimer->deleteLater();
                    setProperty("parkingSwitchWaiting", false);
                    setProperty("parkingTargetBit", -1);
                    hideParkingSwitchHintDialog();
                    updateParkingLegAbnormalDialogVisibility();

                    OperationRecord timeoutRecord;
                    timeoutRecord.timestamp = QDateTime::currentDateTime();
                    timeoutRecord.pageName = QStringLiteral("AGV控制");
                    timeoutRecord.controlName = QStringLiteral("驻车模式90秒已超时");
                    timeoutRecord.controlType = "";
                    timeoutRecord.operation = "";
                    timeoutRecord.oldValue = "";
                    timeoutRecord.newValue = "";
                    m_recorder->addRecord(timeoutRecord);
                }
            });
    parkingWaitTimer->start();
}

void MainWindow::onAGVParkBtnClicked()
{
    if (m_controlMode != WIRED_MODE) {
        ui->statusBar->showMessage("当前为无线控制，驻车功能仅在有线控制模式下生效", 3000);
        qWarning() << "驻车请求被拒绝：当前不是有线控制模式";
        return;
    }

    if (property("parkingSwitchWaiting").toBool()) {
        ui->statusBar->showMessage("驻车切换进行中，请等待完成", 2000);
        return;
    }

    // 绕车检查进行中：不进入其它驻车分支，避免异常窗抢走焦点。
    if (isLegOpenPathCheckActive()) {
        m_legOpenPathCheckDialog->raise();
        m_legOpenPathCheckDialog->activateWindow();
        ui->statusBar->showMessage(QStringLiteral("请先完成支腿伸出路径检查"), 2000);
        return;
    }

    if (!m_mainRegister150Valid && MainDeviceModbusApi::isReady(m_modbusManager)) {
        MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 150, 1);
    }
    const QString interlockHint = robotInterlockHintMessage();
    if (!interlockHint.isEmpty()) {
        showRobotOperationHintDialog(interlockHint);
        return;
    }

    if (isRobotWeightLockGateActive()) {
        blockRobotWeightLockOperation(QStringLiteral("负载超重锁定：驻车操作已无效"));
        return;
    }

    // 预计负载空值提示仅由主驻车按钮触发；支腿异常弹窗内开/关驻车不检查
    if (isEstimatedWeightEmpty()) {
        showExpectedLoadEmptyDialog();
        return;
    }

    if (m_agvLegAbnormal51Bit7Flag) {
        updateParkingLegAbnormalDialogVisibility();
        if (m_parkingLegAbnormalDialog) {
            m_parkingLegAbnormalDialog->raise();
            m_parkingLegAbnormalDialog->activateWindow();
        }
        return;
    }

    // 支腿打开：先绕车干涉检查；支腿关闭：直接执行原逻辑。
    if (!m_agvParkingEnabled) {
        showLegOpenPathCheckDialog(-1);
        return;
    }

    executeAGVParkingSwitch(false);
}

// AGV运动速度变化槽函数
void MainWindow::onAGVMoveSpeedChanged(double value)
{
    if (!isFeatureEnabled("motion_control", "motion.agv_speed_control")) {
        return;
    }

    const int intValue = clampTechSliderEditToInt(m_editAGV_MoveSpeed, value);

    // 按需求直接写入地址3（单位:mm/s）
    // 速度控件在遥控器控制下也允许直接下发，不触发无线模式门禁弹窗。
    writeToAGVDevice(3, intValue, true);

}

// AGV转向角度变化槽函数
void MainWindow::onAGVAngleChanged(double value)
{
    if (!isFeatureEnabled("motion_control", "motion.agv_angle_control")) {
        return;
    }
    if (isRobotWeightLockGateActive()) {
        blockRobotWeightLockOperation(QStringLiteral("负载超重锁定：底盘当前角度调整已无效"));
        if (m_editAGV_Angle) {
            double revertValue = 0;
            if (m_agvRegisterShadow.contains(154)) {
                revertValue = qBound(m_editAGV_Angle->minimum(),
                                     static_cast<double>(m_agvRegisterShadow.value(154)),
                                     m_editAGV_Angle->maximum());
            } else if (m_agvRegisterShadow.contains(4)) {
                revertValue = qBound(m_editAGV_Angle->minimum(),
                                     static_cast<double>(static_cast<qint16>(m_agvRegisterShadow.value(4))),
                                     m_editAGV_Angle->maximum());
            }
            const QSignalBlocker blocker(m_editAGV_Angle);
            m_editAGV_Angle->setValue(revertValue);
        }
        return;
    }

    const qint16 signedValue = static_cast<qint16>(clampTechSliderEditToInt(m_editAGV_Angle, value));
    const quint16 rawUintValue = static_cast<quint16>(signedValue);

    // 地址4为UINT寄存器：通过qint16->quint16显式转换，负数按16位补码发送。
    writeToAGVDevice(4, static_cast<int>(signedValue));

    qCDebug(lcMainWindow) << "AGV转向角度:" << value
                          << "°，地址4写入(有符号):" << signedValue
                          << "原始UINT:" << rawUintValue;

}


// 新增：处理AGV页面的按键○2动作
void MainWindow::handleAGVKey2Action(int keyNumber, bool pressed)
{
    if (!ui || !ui->StackedWidget || ui->StackedWidget->currentIndex() != 0) {
        return;
    }

    if (keyNumber != 10) {
        return;
    }

    const bool stepModeHintShown = maybeShowUnselectedStepModeHintForExternalKey(keyNumber, pressed);
    const bool moveModeHintShown = maybeShowUnselectedMoveModeHintForExternalKey(keyNumber, pressed);
    if (stepModeHintShown || moveModeHintShown) {
        return;
    }

    maybeShowZeroSpeedHintForHomePageExternalKey(keyNumber, pressed);

    if (pressed) {
        if (!m_mainRegister150Valid && MainDeviceModbusApi::isReady(m_modbusManager)) {
            MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 150, 1);
        }
        const QString interlockHint = robotInterlockHintMessage();
        if (!interlockHint.isEmpty()) {
            showRobotOperationHintDialog(interlockHint);
            return;
        }
    }

    if (pressed && !m_stepModeEnabled && !m_isJointMode) {
        qCDebug(lcMainWindow) << "首页○10外部按键忽略：当前非关节模式且非步进模式";
        return;
    }

    if (m_stepModeEnabled) {
        maybeShowUnconfiguredStepValueHintForExternalKey(keyNumber, pressed);
        if (!pressed) {
            m_robotExternalKeyPressed[keyNumber] = false;
            maybeClearFirstPageStepValueIfAllExternalKeysReleased();
            return;
        }

        if (selectedStepTargetRegister() != 504) {
            qCDebug(lcMainWindow) << "首页○10(步进)：未选中 AGV 步进目标(btnStepTargetAgv)，忽略";
            showStepTargetMismatchHintDialog(keyNumber, selectedStepTargetName());
            return;
        }

        if (!m_stepValueEdit) {
            qCDebug(lcMainWindow) << "首页○10(步进)：未找到 lineEdit_StepValue，仅写 bit4/bit5";
            writeAGVRegisterBits(0, { qMakePair(4, true) }, QStringLiteral("○10步进按下(1)：寄存器0 bit4=1"));
            writeAGVRegisterBits(0, { qMakePair(5, true) }, QStringLiteral("○10步进按下(3)：寄存器0 bit5=1"));
            appendAgvExternalKeyRecord(keyNumber, pressed);
            markStepMotionPendingStop(StepMotionStopKind::Agv, QStringLiteral("底盘(AGV)"), keyNumber);
            m_robotExternalKeyPressed[keyNumber] = true;
            return;
        }

        bool ok = false;
        const double raw = m_stepValueEdit->text().trimmed().toDouble(&ok);
        if (!ok) {
            qCDebug(lcMainWindow) << "首页○10(步进)：步进值无效" << m_stepValueEdit->text();
            return;
        }
        const int stepInt = static_cast<int>(raw);

        writeAGVRegisterBits(0, { qMakePair(4, true) }, QStringLiteral("○10步进按下(1)：寄存器0 bit4=1"));
        writeToAGVDevice(5, stepInt);
        writeAGVRegisterBits(0, { qMakePair(5, true) }, QStringLiteral("○10步进按下(3)：寄存器0 bit5=1"));
        appendAgvExternalKeyRecord(keyNumber, pressed, m_stepValueEdit->text().trimmed());
        markStepMotionPendingStop(StepMotionStopKind::Agv, QStringLiteral("底盘(AGV)"), keyNumber);
        m_robotExternalKeyPressed[keyNumber] = true;
        return;
    }

    if (m_isJointMode) {
        writeAGVRegisterBits(0,
                             { qMakePair(2, pressed) },
                             pressed ? QStringLiteral("○10点动按下：寄存器0 bit2=1")
                                     : QStringLiteral("○10点动释放：寄存器0 bit2=0"));
        appendAgvExternalKeyRecord(keyNumber, pressed);
        return;
    }

    writeAGVRegisterBits(0,
                         {
                             qMakePair(2, pressed),
                         },
                         pressed ? "○10按下(bit2=1)" : "○10释放(bit2=0)");
    appendAgvExternalKeyRecord(keyNumber, pressed);
}

QString MainWindow::currentSteeringModeText() const
{
    if (m_steeringModeSelector) {
        return m_steeringModeSelector->modeText(m_steeringModeSelector->currentMode());
    }

    SteeringMode mode = STEER_FRONT_BACK;
    if (resolveSteeringModeFromStatus50(m_agvRegisterShadow.value(50, 0), &mode, nullptr, m_lastSteeringMode)) {
        return steeringModeStatusText(mode);
    }
    return QStringLiteral("未知模式");
}

void MainWindow::appendAgvExternalKeyRecord(int keyNumber, bool pressed, const QString &stepValueFromLineEdit)
{
    if (!m_recorder) {
        return;
    }

    const double speedValue = m_editAGV_MoveSpeed ? m_editAGV_MoveSpeed->value() : getSliderEditValue("SEdit_AGV_MoveSpeed");
    const double angleValue = m_editAGV_Angle ? m_editAGV_Angle->value() : getSliderEditValue("SEdit_AGV_Angle");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = QString("外部按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "agv_external_motion_start" : "agv_external_motion_end";
    record.oldValue = "";
    QString detail = pressed
                         ? QString("当前模式为%1，设置速度为%2，设置角度为%3，开始运动")
                               .arg(currentSteeringModeText())
                               .arg(speedValue, 0, 'f', 0)
                               .arg(angleValue, 0, 'f', 0)
                         : QString("运动完成，当前模式为%1，设置速度为%2，设置角度为%3")
                               .arg(currentSteeringModeText())
                               .arg(speedValue, 0, 'f', 0)
                               .arg(angleValue, 0, 'f', 0);
    if (!stepValueFromLineEdit.isEmpty()) {
        detail += QStringLiteral("，步进值为：%1").arg(stepValueFromLineEdit);
    }
    record.newValue = detail;
    m_recorder->addRecord(record);
}
//运动模式选择

// 新增：初始化转向模式控制
void MainWindow::setupSteeringModeControl()
{
    if (!isFeatureEnabled("motion_control", "motion.steering_mode")) {
        qCDebug(lcMainWindow) << "转向模式功能已关闭，跳过初始化";
        return;
    }

    // 如果已经存在控件，先查找
    m_steeringModeSelector = findChild<SteeringModeSelector*>("steeringModeSelector");

    if (!m_steeringModeSelector) {
        // 在UI中放置SteeringModeSelector，可以通过提升Widget实现
        // 或者动态创建
        QWidget *agvPage = ui->StackedWidget->widget(4);  // AGV控制页面
        if (agvPage) {
            m_steeringModeSelector = agvPage->findChild<SteeringModeSelector*>("steeringModeSelector");
        }
    }

    if (m_steeringModeSelector) {
        if (m_steeringModeSelector->height() < 130) {
            m_steeringModeSelector->setGeometry(m_steeringModeSelector->x(),
                                                m_steeringModeSelector->y(),
                                                m_steeringModeSelector->width(),
                                                130);
        }

        // 设置样式为全息风格
        m_steeringModeSelector->setButtonStyle(TechPushButton::StyleHolographic);

        // 设置自定义颜色
        m_steeringModeSelector->setActiveColor(QColor(0, 200, 255));     // 激活状态颜色
        m_steeringModeSelector->setInactiveColor(QColor(80, 80, 100));   // 非激活状态颜色
        m_steeringModeSelector->setTextColor(Qt::white);                 // 文字颜色

        // 连接模式切换信号到报警逻辑
        connect(m_steeringModeSelector, &SteeringModeSelector::modeChanged,
            this, &MainWindow::onSteeringModeChanged,
            Qt::UniqueConnection);

        qCDebug(lcMainWindow) << "转向模式选择器初始化完成";
    } else {
        qWarning() << "未找到转向模式选择器控件";
    }
}

// 新增：转向模式改变槽函数
void MainWindow::onSteeringModeChanged(SteeringMode mode, int modbusValue)
{
    qCDebug(lcMainWindow) << "转向模式改变为:" << mode << "，Modbus值:" << modbusValue;

    if (!m_mainRegister150Valid && MainDeviceModbusApi::isReady(m_modbusManager)) {
        MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 150, 1);
    }
    const QString interlockHint = robotInterlockHintMessage();
    if (!interlockHint.isEmpty()) {
        showRobotOperationHintDialog(interlockHint);
        if (m_steeringModeSelector) {
            const QSignalBlocker blocker(m_steeringModeSelector);
            m_steeringModeSelector->setCurrentMode(m_lastSteeringMode);
        }
        return;
    }

    QLabel *steeringLabel = ui && ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarSteeringLabel") : nullptr;

    if (isRobotWeightLockGateActive()) {
        blockRobotWeightLockOperation(QStringLiteral("负载超重锁定：底盘转向模式切换已无效"));
        if (m_steeringModeSelector) {
            const QSignalBlocker blocker(m_steeringModeSelector);
            m_steeringModeSelector->setCurrentMode(m_lastSteeringMode);
        }
        if (steeringLabel && m_steeringModeSelector) {
            steeringLabel->setText(QStringLiteral("转向:%1").arg(m_steeringModeSelector->modeText(m_lastSteeringMode)));
            steeringLabel->setStyleSheet(QStringLiteral("color: #55ff55; font-weight: bold; font-size: 11px;"));
        }
        return;
    }

    if (m_robotWeightOverload150Bit3Flag && m_robotWeightOverloadUserAckedWhileActive) {
        showRobotWeightOverloadDialog();
        showNotification(QStringLiteral("负载超限：底盘转向模式切换已无效"));
        if (m_steeringModeSelector) {
            const QSignalBlocker blocker(m_steeringModeSelector);
            m_steeringModeSelector->setCurrentMode(m_lastSteeringMode);
        }
        if (steeringLabel && m_steeringModeSelector) {
            steeringLabel->setText(QStringLiteral("转向:%1").arg(m_steeringModeSelector->modeText(m_lastSteeringMode)));
            steeringLabel->setStyleSheet(QStringLiteral("color: #55ff55; font-weight: bold; font-size: 11px;"));
        }
        return;
    }

    // 向192.168.1.88的2地址写入对应值
    const bool steerWriteOk = writeToAGVDevice(2, modbusValue);
    if (!steerWriteOk) {
        if (m_steeringModeSelector) {
            const QSignalBlocker blocker(m_steeringModeSelector);
            m_steeringModeSelector->setCurrentMode(m_lastSteeringMode);
        }
        if (steeringLabel && m_steeringModeSelector) {
            steeringLabel->setText(QString("转向:%1").arg(m_steeringModeSelector->modeText(m_lastSteeringMode)));
            steeringLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
        }
        qCDebug(lcMainWindow) << "转向模式 AGV 写寄存器2失败（含示教写门禁等），已回滚选择器";
        return;
    }

    if (steeringLabel && m_steeringModeSelector) {
        steeringLabel->setText(QString("转向:%1").arg(m_steeringModeSelector->modeText(mode)));
        steeringLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
    }

    if (!isFeatureEnabled("alarm_system", "alarm.steering_switch")) {
        qCDebug(lcMainWindow) << "转向模式切换报警功能已关闭，跳过报警窗口逻辑";
        m_isSwitchingSteeringMode = false;
        m_targetSteeringWaitBit = -1;
        if (m_isSteeringAlarmActive) {
            m_isSteeringAlarmActive = false;
            updateAlarmDisplay();
        }
        return;
    }

    const bool oldIn123 = (m_lastSteeringMode == STEER_FRONT_BACK
                           || m_lastSteeringMode == STEER_FRONT_ONLY
                           || m_lastSteeringMode == STEER_PARALLEL);
    const bool newIn123 = (mode == STEER_FRONT_BACK
                           || mode == STEER_FRONT_ONLY
                           || mode == STEER_PARALLEL);

    m_lastSteeringMode = mode;
    m_isSwitchingSteeringMode = false;
    m_targetSteeringWaitBit = -1;

    // 1/2/3 之间互换不弹窗
    if (oldIn123 && newIn123) {
        m_isSteeringAlarmActive = false;
        hideAlarm();
        qCDebug(lcMainWindow) << "模式1/2/3之间切换，不弹出提示窗口";
        return;
    }

    // 其余切换：示教器控制下弹窗并等待位信号；遥控器模式不弹出
    if (m_controlMode == WIRELESS_MODE) {
        m_isSteeringAlarmActive = false;
        hideAlarm();
        qCDebug(lcMainWindow) << "遥控器控制模式，不弹出底盘切换等待窗口";
    } else {
        m_isSteeringAlarmActive = true;
        showAlarm("正在更换底盘模式", "#FFFF00", false);
        m_isSwitchingSteeringMode = true;

        if (mode == STEER_LATERAL) {
            // 切到4，等待地址50的bit10=1
            m_targetSteeringWaitBit = 11;
        } else if (mode == STEER_ROTATE) {
            // 切到5，等待地址50的bit12=1
            m_targetSteeringWaitBit = 12;
        } else {
            // 4/5切到1/2/3，等待地址50的bit9=1
            m_targetSteeringWaitBit = 10;
        }

        qCDebug(lcMainWindow) << "底盘模式切换等待地址50的bit" << m_targetSteeringWaitBit << "=1 后隐藏提示窗口";

        const SteeringMode expectedMode = mode;
        const int expectedBit = m_targetSteeringWaitBit;
        QTimer::singleShot(20000, this, [this, expectedMode, expectedBit]() {
            if (!m_isSwitchingSteeringMode || !m_isSteeringAlarmActive) {
                return;
            }
            if (m_lastSteeringMode != expectedMode || m_targetSteeringWaitBit != expectedBit) {
                return;
            }

            m_isSwitchingSteeringMode = false;
            m_targetSteeringWaitBit = -1;
            m_isSteeringAlarmActive = false;
            hideAlarm();

            if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
                m_agvModbusManager->readMultipleRegisters(50, 1);
            }

            QTimer::singleShot(300, this, [this, expectedBit]() {
                const bool hasStatusWord = m_agvRegisterShadow.contains(50);
                const quint16 regValue = hasStatusWord ? m_agvRegisterShadow.value(50) : static_cast<quint16>(0xFFFF);
                QString currentModeText;
                if (hasStatusWord) {
                    SteeringMode currentMode = STEER_FRONT_BACK;
                    const bool modeResolved = resolveSteeringModeFromStatus50(
                        regValue, &currentMode, &currentModeText, m_lastSteeringMode);
                    if (!modeResolved) {
                        currentModeText = QStringLiteral("未知模式");
                    }
                    if (m_steeringModeSelector) {
                        if (modeResolved) {
                            const QSignalBlocker blocker(m_steeringModeSelector);
                            m_steeringModeSelector->setCurrentMode(currentMode);
                            currentModeText = m_steeringModeSelector->modeText(currentMode);
                        }
                    }
                    if (modeResolved) {
                        m_lastSteeringMode = currentMode;
                    }
                }

                OperationRecord timeoutRecord;
                timeoutRecord.timestamp = QDateTime::currentDateTime();
                timeoutRecord.pageName = "AGV控制";
                timeoutRecord.controlName = "steeringModeSelector";
                timeoutRecord.controlType = "SteeringModeSelector";
                timeoutRecord.operation = "steering_switch_timeout";
                timeoutRecord.oldValue = QString("等待bit%1").arg(expectedBit);
                if (hasStatusWord) {
                    timeoutRecord.newValue = QString("20秒超时，当前在%1（读值:%2）").arg(currentModeText).arg(regValue);
                } else {
                    timeoutRecord.newValue = "20秒超时，当前模式读取失败";
                }
                m_recorder->addRecord(timeoutRecord);
            });
        });
    }

    // 显示通知
    showNotification(QString("转向模式: %1").arg(m_steeringModeSelector->modeText(mode)));
}
// 新增：设置TCP传输UI
void MainWindow::setupTcpTransmissionUI()
{
    if (!isBigFeatureEnabled("tcp_transmission")) {
        return;
    }

    // 在状态栏添加连接状态指示器
    if (ui->statusBar) {
        // 创建中间的状态容器，用于放置时间、权限、运行模式
        QWidget *centerWidget = new QWidget(ui->statusBar);
        QHBoxLayout *centerLayout = new QHBoxLayout(centerWidget);
        centerLayout->setContentsMargins(0, 15, 0, 0); // 增加上边距，使文字整体下移
        centerLayout->setSpacing(15); // 紧凑排布
        centerLayout->setAlignment(Qt::AlignCenter); // 居中对齐

        // 时间日期标签
        QLabel *timeLabel = new QLabel(centerWidget);
        timeLabel->setObjectName("statusBarTimeLabel");
        timeLabel->setStyleSheet("color: #ffffff; font-family: 'Consolas'; font-size: 11px;");
        timeLabel->setFixedHeight(12); // 固定高度，与右侧 MAIN 指示灯 12px 一致
        centerLayout->addWidget(timeLabel);

        // 权限等级指示灯
        QWidget *roleWidget = new QWidget(centerWidget);
        QHBoxLayout *roleLayout = new QHBoxLayout(roleWidget);
        roleLayout->setContentsMargins(0,0,0,0);
        roleLayout->setSpacing(2);
        QLabel *roleLed = new QLabel("●", roleWidget);
        roleLed->setObjectName("statusBarRoleLed");
        roleLed->setStyleSheet("color: #aaaaaa; font-size: 12px;"); // 稍小一点的指示灯
        roleLed->setFixedHeight(12);
        QLabel *roleText = new QLabel("操作员", roleWidget);
        roleText->setObjectName("statusBarRoleText");
        roleText->setStyleSheet("color: #ffffff; font-size: 11px;");
        roleText->setFixedHeight(12);
        roleLayout->addWidget(roleLed);
        roleLayout->addWidget(roleText);
        centerLayout->addWidget(roleWidget);

        // 系统运行模式 (点动/步进)
        const QString startupRunModeText = m_stepModeUnknown ? "步进未选择" : (m_stepModeEnabled ? "步进模式" : "点动模式");
        const QString startupRunModeColor = m_stepModeUnknown ? "#aaaaaa" : (m_stepModeEnabled ? "#00ff00" : "#00ccff");
        QLabel *runModeLabel = new QLabel(startupRunModeText, centerWidget);
        runModeLabel->setObjectName("statusBarRunModeLabel");
        runModeLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                        .arg(startupRunModeColor));
        runModeLabel->setFixedHeight(12);
        centerLayout->addWidget(runModeLabel);

        // 运动模式 (关节/坐标)
        const QString startupMoveModeText = m_moveModeUnknown ? "运动未选择" : (m_isJointMode ? "关节模式" : "坐标模式");
        const QString startupMoveModeColor = m_moveModeUnknown ? "#aaaaaa" : (m_isJointMode ? "#55ff55" : "#ffaa00");
        QLabel *moveModeLabel = new QLabel(startupMoveModeText, centerWidget);
        moveModeLabel->setObjectName("statusBarMoveModeLabel");
        moveModeLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                         .arg(startupMoveModeColor));
        moveModeLabel->setFixedHeight(12);
        centerLayout->addWidget(moveModeLabel);
        
        // 控制模式 (有线/无线)
        QLabel *controlModeLabel = new QLabel(m_controlMode == WIRED_MODE ? "有线控制" : "无线控制", centerWidget);
        controlModeLabel->setObjectName("statusBarControlModeLabel");
        controlModeLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                        .arg(m_controlMode == WIRED_MODE ? "#ffffff" : "#ffff00"));
        controlModeLabel->setFixedHeight(12);
        centerLayout->addWidget(controlModeLabel);

        QLabel *oaLabel = new QLabel(m_agvOaEnabled ? "避障开" : "避障关", centerWidget);
        oaLabel->setObjectName("statusBarOaLabel");
        oaLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                       .arg(m_agvOaEnabled ? "#00C8FF" : "#7F8C8D"));
        oaLabel->setFixedHeight(12);
        centerLayout->addWidget(oaLabel);

        QLabel *parkLabel = new QLabel(m_agvParkingEnabled ? "驻车开" : "驻车关", centerWidget);
        parkLabel->setObjectName("statusBarParkLabel");
        parkLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                         .arg(m_agvParkingEnabled ? "#00C8FF" : "#7F8C8D"));
        parkLabel->setFixedHeight(12);
        centerLayout->addWidget(parkLabel);

        QLabel *steeringLabel = new QLabel(QString("转向:%1").arg(currentSteeringModeText()), centerWidget);
        steeringLabel->setObjectName("statusBarSteeringLabel");
        steeringLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
        steeringLabel->setFixedHeight(12);
        centerLayout->addWidget(steeringLabel);

        centerWidget->setLayout(centerLayout);
        ui->statusBar->addPermanentWidget(centerWidget, 1); // 占据中间大部分空间

        // 2. TCP, AGV 和 MAIN 状态指示器竖直排布并在右侧对齐
        QWidget *statusGroupWidget = new QWidget(ui->statusBar);
        QVBoxLayout *statusGroupLayout = new QVBoxLayout(statusGroupWidget);
        statusGroupLayout->setContentsMargins(0, 0, 10, 0);
        statusGroupLayout->setSpacing(0);
        statusGroupLayout->setAlignment(Qt::AlignRight); // 向右对齐

        auto createIndicator = [statusGroupWidget](const QString& objName, const QString& text) {
            QLabel *label = new QLabel(text, statusGroupWidget);
            label->setObjectName(objName);
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            label->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Disconnected));
            label->setFixedHeight(12);
            return label;
        };

        QLabel *tcpStatusIndicator = createIndicator("tcpStatusIndicator", "TCP: ●");
        tcpStatusIndicator->setToolTip("TCP连接状态");
        
        QLabel *agvModbusStatusIndicator = createIndicator("agvModbusStatusIndicator", "AGV: ●");
        agvModbusStatusIndicator->setToolTip("AGV Modbus连接状态");

        QLabel *mainModbusStatusIndicator = createIndicator("mainModbusStatusIndicator", "MAIN: ●");
        mainModbusStatusIndicator->setToolTip("主设备 Modbus连接状态");

        statusGroupLayout->addWidget(tcpStatusIndicator);
        statusGroupLayout->addWidget(agvModbusStatusIndicator);
        statusGroupLayout->addWidget(mainModbusStatusIndicator);
        
        statusGroupWidget->setLayout(statusGroupLayout);
        ui->statusBar->addPermanentWidget(statusGroupWidget);

        // 启动时间更新定时器
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateStatusBarTime);
        timer->start(1000);
        updateStatusBarTime(); // 立即执行一次

        // 初始化一次颜色
        if (m_modbusManager && m_modbusManager->isConnected()) {
            mainModbusStatusIndicator->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Connected));
            mainModbusStatusIndicator->setToolTip("主设备 Modbus连接正常");
        }
    }
}

void MainWindow::updateStatusBarTime()
{
    QLabel *timeLabel = ui->statusBar->findChild<QLabel*>("statusBarTimeLabel");
    if (timeLabel) {
        timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    }

    // 顺便在这里同步一下权限指示灯（因为它是全局变量变化的，没法通过信号捕捉）
    QLabel *roleLed = ui->statusBar->findChild<QLabel*>("statusBarRoleLed");
    QLabel *roleText = ui->statusBar->findChild<QLabel*>("statusBarRoleText");
    if (roleLed && roleText) {
        QString color = "#aaaaaa";
        QString text = "操作员";
        if (isPermissionSelectionPending()) {
            color = "#aaaaaa";
            text = "未选择权限";
        } else {
            switch(m_currentUserRole) {
                case UserRole::Operator: color = "#aaaaaa"; text = "操作员"; break;
                case UserRole::Engineer: color = "#55aaff"; text = "工程师"; break;
                case UserRole::Admin: color = "#55ff55"; text = "管理员"; break;
                case UserRole::Manufacturer: color = "#ffaa00"; text = "厂家"; break;
            }
        }
        roleLed->setStyleSheet(QString("color: %1; font-size: 14px;").arg(color));
        roleText->setText(text);
    }
}

// 新增：启用/禁用TCP传输
void MainWindow::enableTcpTransmission(bool enabled)
{
    if (!isBigFeatureEnabled("tcp_transmission")) {
        qCDebug(lcMainWindow) << "TCP传输功能已关闭，忽略请求";
        return;
    }

    m_tcpTransmissionEnabled = enabled;

    if (m_recorder) {
        m_recorder->enableTcpTransmission(enabled);

        // 设置服务器地址
        m_recorder->setTcpServer(m_tcpServerHost, WIN7_PORT);

        if (enabled) {
            qCDebug(lcMainWindow) << "启用TCP传输，服务器:" << m_tcpServerHost << ":" << WIN7_PORT;
            ui->statusBar->showMessage("TCP传输已启用，正在连接服务器...", 3000);
        } else {
            qCDebug(lcMainWindow) << "禁用TCP传输";
            ui->statusBar->showMessage("TCP传输已禁用", 3000);
        }
    }
}

void MainWindow::updateTcpServerHost(const QString &subnetOctet, const QString &hostOctet)
{
    bool subnetOk = false;
    bool hostOk = false;
    const int subnet = subnetOctet.trimmed().toInt(&subnetOk);
    const int host = hostOctet.trimmed().toInt(&hostOk);
    if (!subnetOk || !hostOk || subnet < 0 || subnet > 255 || host < 0 || host > 255) {
        showNotification(QStringLiteral("WIN7 IP 无效，请输入 0-255"));
        return;
    }

    const QString newIp = QStringLiteral("192.168.%1.%2").arg(subnet).arg(host);
    m_tcpServerHost = newIp;

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Network");
    settings.setValue("tcp_server_host", m_tcpServerHost);
    settings.endGroup();
    settings.sync();

    if (m_recorder) {
        m_recorder->setTcpServer(m_tcpServerHost, WIN7_PORT);
        qCDebug(lcMainWindow) << "更新TCP服务器IP为:" << m_tcpServerHost;
    }
}

void MainWindow::updateSimulatorHost(const QString &subnetOctet, const QString &hostOctet)
{
    bool subnetOk = false;
    bool hostOk = false;
    const int subnet = subnetOctet.trimmed().toInt(&subnetOk);
    const int host = hostOctet.trimmed().toInt(&hostOk);
    if (!subnetOk || !hostOk || subnet < 0 || subnet > 255 || host < 0 || host > 255) {
        showNotification(QStringLiteral("模拟器 IP 无效，请输入 0-255"));
        return;
    }

    const QString newIp = QStringLiteral("192.168.%1.%2").arg(subnet).arg(host);
    m_remoteSimulatorHost = newIp;

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Network");
    settings.setValue("remote_simulator_host", m_remoteSimulatorHost);
    settings.endGroup();
    settings.sync();

    qCDebug(lcMainWindow) << "尝试更新模拟器IP为:" << newIp;

    applyNetworkRuntimeSettings();
}

void MainWindow::applyNetworkRuntimeSettings()
{
    if (m_recorder) {
        m_recorder->setTcpServer(m_tcpServerHost, WIN7_PORT);
    }

    if (isFeatureEnabled("tcp_transmission", "tcp.remote_simulator")) {
        const QString &simHost = m_remoteSimulatorHost;
        if (m_modbusManager) {
            m_modbusManager->disconnectFromDevice();
            MainModbusConnector::connectAndConfigure(
                m_modbusManager,
                MainModbusEndpoint{simHost, 5020},
                m_mainModbusPollIntervalMs,
                m_mainReconnectIntervalMs);
            qCDebug(lcMainWindow) << "[MainModbus] 已切换模拟器并重新连接:" << simHost << ":5020";
        }
        if (m_agvModbusManager) {
            m_agvModbusManager->disconnectFromDevice();
            m_agvModbusManager->connectToDevice(simHost, 5021);
            qCDebug(lcMainWindow) << "[AGVModbus] 已切换模拟器并重新连接:" << simHost << ":5021";
        }
    } else {
        qCDebug(lcMainWindow) << "当前未启用远程模拟器模式，模拟器 IP 将在下次切换模式时生效";
    }
}

// 新增：TCP传输复选框槽函数
void MainWindow::onEnableTcpTransmission(bool checked)
{
    enableTcpTransmission(checked);
}

// 新增：发送所有记录槽函数
void MainWindow::onSendAllRecords()
{
    if (!isFeatureEnabled("tcp_transmission", "tcp.send_all")) {
        showNotification("TCP全量发送功能已关闭");
        return;
    }

    if (!m_tcpTransmissionEnabled) {
        showToast(QStringLiteral("请先启用TCP传输"), ToastKind::Warning);
        return;
    }

    if (!m_recorder->isTcpConnected()) {
        showToast(QStringLiteral("TCP连接未建立，无法发送记录"), ToastKind::Warning);
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认",
                                  QString("确定要发送所有 %1 条记录到服务器吗？").arg(m_recorder->recordCount()),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_recorder->sendAllRecordsToServer();
        ui->statusBar->showMessage("开始发送所有记录到服务器...", 3000);
    }
}

// 新增：TCP连接状态变化槽函数
void MainWindow::onTcpConnectionStatusChanged(bool connected)
{
    // 更新状态栏指示器
    QLabel *tcpStatusIndicator = ui->statusBar->findChild<QLabel*>("tcpStatusIndicator");
    if (tcpStatusIndicator) {
        if (connected) {
            tcpStatusIndicator->setText("TCP: ●");
            tcpStatusIndicator->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Connected));
            tcpStatusIndicator->setToolTip("TCP连接正常");
        } else {
            tcpStatusIndicator->setText("TCP: ●");
            tcpStatusIndicator->setStyleSheet(MainModbusStatus::indicatorStyle(MainModbusState::Disconnected));
            tcpStatusIndicator->setToolTip("TCP连接断开");
        }
    }

    // 更新记录页面状态标签
    QWidget *recordPage = ui->page_HistoryRecord;
    if (recordPage) {
        QLabel *tcpStatusLabel = recordPage->findChild<QLabel*>("tcpStatusLabel");
        if (tcpStatusLabel) {
            if (connected) {
                tcpStatusLabel->setText("TCP: 已连接");
                tcpStatusLabel->setStyleSheet("color: #55ff55; font-weight: bold;");
            } else {
                tcpStatusLabel->setText("TCP: 未连接");
                tcpStatusLabel->setStyleSheet("color: #ff5555; font-weight: bold;");
            }
        }
    }

    // 显示通知
    if (connected) {
        showNotification("TCP服务器连接成功");
    } else {
        showNotification("TCP服务器连接断开");
    }
}

// 新增：TCP传输完成槽函数
void MainWindow::onTcpTransmissionComplete()
{
    qCDebug(lcMainWindow) << "所有记录已发送到TCP服务器";
    showNotification("所有记录已发送到服务器");
}

// 新增：TCP传输错误槽函数
void MainWindow::onTcpTransmissionError(const QString &error)
{
    // 在记录页面显示错误
    QWidget *recordPage = ui->page_HistoryRecord;
    if (recordPage) {
        QLabel *tcpStatusLabel = recordPage->findChild<QLabel*>("tcpStatusLabel");
        if (tcpStatusLabel) {
            tcpStatusLabel->setText("TCP: 错误");
            tcpStatusLabel->setStyleSheet("color: #ffaa00; font-weight: bold;");
            tcpStatusLabel->setToolTip(error);
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString notificationText = QString("TCP传输错误: %1").arg(error);
    const bool shouldNotify = (notificationText != m_lastTcpErrorNotification)
            || ((nowMs - m_lastTcpErrorNotificationMs) > 2000);

    if (shouldNotify) {
        qWarning() << "TCP传输错误:" << error;
        m_lastTcpErrorNotification = notificationText;
        m_lastTcpErrorNotificationMs = nowMs;
        showNotification(notificationText);
    } else {
        qCDebug(lcMainWindow) << "TCP传输错误(已抑制重复):" << error;
    }
}
// void MainWindow::on_TBtn_Stepmove_clicked()
// {
//      writeToMainDevice(5,2);
//     // writeToMainDevice(402,1);

//     writeToMainDevice(19,1);

//     writeToMainDevice(44,1);
//     writeToMainDevice(10,1);


// }


// void MainWindow::on_Btn_SwitchAGV_2_clicked()
// {
//      writeToMainDevice(5,1);
//     // writeToMainDevice(402,0);

//     writeToMainDevice(19,0);
//     writeToMainDevice(10,0);

// }
// 添加步进/点动模式切换函数
void MainWindow::onStepMoveButtonClicked()
{
    if (!isFeatureEnabled("motion_control", "motion.step_mode")) {
        showNotification("步进模式功能已关闭");
        return;
    }

    // 寄存器值为0时，首次点击进入默认模式（点动）
    if (m_stepModeUnknown) {
        m_stepModeUnknown = false;
        m_stepModeEnabled = false;
    } else {
        m_stepModeEnabled = !m_stepModeEnabled;
    }

    if (m_stepModeEnabled) {
        // 切换到步进模式
        ui->TBtn_Stepmove->setText("步进模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：步进模式");

        // 根据当前页面决定写入的寄存器：第一页(0)->501，第四页(3)->600
        int currentPage = ui->StackedWidget ? ui->StackedWidget->currentIndex() : 0;
        if (currentPage == 0) {
            writeToMainDevice(501, 2);
            qCDebug(lcMainWindow) << "首页：切换到步进模式，地址501写入2";
        } else if (currentPage == 3) {
            writeToMainDevice(600, 2);
            qCDebug(lcMainWindow) << "第四页：切换到步进模式，地址600写入2";
        }

        ui->statusBar->showMessage("已切换到步进模式", 2000);

        // 更新状态栏显示
        QLabel *runModeLabel = ui->statusBar->findChild<QLabel*>("statusBarRunModeLabel");
        if (runModeLabel) {
            runModeLabel->setText("步进模式");
            runModeLabel->setStyleSheet("color: #00ff00; font-weight: bold; font-size: 11px;");
        }

    } else {
        // 切换到点动模式
        ui->TBtn_Stepmove->setText("点动模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：点动模式");

        // 更新状态栏显示
        QLabel *runModeLabel = ui->statusBar->findChild<QLabel*>("statusBarRunModeLabel");
        if (runModeLabel) {
            runModeLabel->setText("点动模式");
            runModeLabel->setStyleSheet("color: #00ccff; font-weight: bold; font-size: 11px;");
        }

        // 根据当前页面决定写入的寄存器：第一页(0)->501，第四页(3)->600
        int currentPage = ui->StackedWidget ? ui->StackedWidget->currentIndex() : 0;
        if (currentPage == 0) {
            writeToMainDevice(501, 1);
            qCDebug(lcMainWindow) << "首页：切换到点动模式，地址501写入1";
        } else if (currentPage == 3) {
            writeToMainDevice(600, 1);
            qCDebug(lcMainWindow) << "第四页：切换到点动模式，地址600写入1";
        }

        ui->statusBar->showMessage("已切换到点动模式", 2000);
    }

    updateFunctionSwitchVisuals();
    updateStepTargetButtonsState();

}

void MainWindow::maybeClearFirstPageStepValueIfAllExternalKeysReleased()
{
    if (!m_stepModeEnabled || !ui || !ui->StackedWidget || ui->StackedWidget->currentIndex() != 0
        || !m_stepValueEdit) {
        return;
    }
    for (int k = 1; k <= 10; ++k) {
        if (m_robotExternalKeyPressed.value(k, false)) {
            return;
        }
    }
    m_stepValueEdit->clear();
}

// 步进模式下使能按钮按下
void MainWindow::onEnableButtonPressedStepMode()
{
    qCDebug(lcMainWindow) << "步进模式下使能按钮按下";

    m_pendingStepMotionStops.clear();

    // 需求变更：步进模式下使能按钮不再触发514或步进值写入，改由外部按键触发。

    if (m_stepValueEdit && !m_stepValueEdit->text().isEmpty()) {
        const int targetReg = selectedStepTargetRegister();
        const QString stepValue = m_stepValueEdit->text();
        const QString targetName = selectedStepTargetName();

        double currentValue = 0.0;
        if (targetReg == 500) currentValue = getSliderLabelValue("label_Value1");
        else if (targetReg == 501) currentValue = getSliderLabelValue("label_Value2");
        else if (targetReg == 502) currentValue = getSliderLabelValue("label_Value3");
        else if (targetReg == 503) currentValue = getSliderLabelValue("label_Value4");
        else if (targetReg == 504 && m_editAGV_MoveSpeed) currentValue = m_editAGV_MoveSpeed->value();

        recordStepMoveAction(targetName, currentValue, stepValue, true);
        ui->statusBar->showMessage(QString("步进模式：目标%1，步进值%2")
                                       .arg(targetName, stepValue), 2000);
        return;
    }

    // 检查各个输入框是否有内容，有则记录历史记录
    if (m_editJ1MoveStep && !m_editJ1MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value1"); // J1当前角度
        QString stepValue = m_editJ1MoveStep->text();
        recordStepMoveAction("悬臂组件(J1)", currentAngle, stepValue, true);
    }

    if (m_editJ2MoveStep && !m_editJ2MoveStep->text().isEmpty()) {
        double currentHeight = getSliderLabelValue("label_Value2"); // J2当前高度
        QString stepValue = m_editJ2MoveStep->text();
        recordStepMoveAction("升降组件(J2)", currentHeight, stepValue, true);
    }

    if (m_editJ3MoveStep && !m_editJ3MoveStep->text().isEmpty()) {
        double currentLength = getSliderLabelValue("label_Value3"); // J3当前长度
        QString stepValue = m_editJ3MoveStep->text();
        recordStepMoveAction("伸缩臂(J3)", currentLength, stepValue, true);
    }

    if (m_editJ4MoveStep && !m_editJ4MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value4"); // J4当前角度
        QString stepValue = m_editJ4MoveStep->text();
        recordStepMoveAction("柔顺组件(J4)", currentAngle, stepValue, true);
    }

    ui->statusBar->showMessage("步进模式：使能按钮按下（等待外部按键触发）", 2000);
}

// 步进模式下使能按钮释放
void MainWindow::onEnableButtonReleasedStepMode()
{
    qCDebug(lcMainWindow) << "步进模式下使能按钮释放";

    // 首页统一步进输入框：不在使能松开时清空，改由所有外部按键(○1~○10)均松开后清空。

    // 步进模式下使能按钮释放：不写514；首页 lineEdit_StepValue 清空见 maybeClearFirstPageStepValueIfAllExternalKeysReleased。

    const bool hadPendingStepStops = !m_pendingStepMotionStops.isEmpty();
    flushPendingStepMotionStopsOnEnableRelease();

    if (hadPendingStepStops) {
        ui->statusBar->showMessage("步进模式：使能按钮释放", 2000);
        return;
    }

    // 与 onEnableButtonPressedStepMode 一致：仅当首页统一步进输入框有内容时才按选中目标记录结束；
    // 否则仅判断 m_stepValueEdit 非空会在输入为空时仍走本分支，且 selectedStepTargetRegister()
    // 无选中按钮时默认 500，误记为「悬臂组件(J1)」步进结束。
    if (m_stepValueEdit && !m_stepValueEdit->text().isEmpty()) {
        const int targetReg = selectedStepTargetRegister();
        const QString targetName = selectedStepTargetName();

        double currentValue = 0.0;
        if (targetReg == 500) currentValue = getSliderLabelValue("label_Value1");
        else if (targetReg == 501) currentValue = getSliderLabelValue("label_Value2");
        else if (targetReg == 502) currentValue = getSliderLabelValue("label_Value3");
        else if (targetReg == 503) currentValue = getSliderLabelValue("label_Value4");
        else if (targetReg == 504 && m_editAGV_MoveSpeed) currentValue = m_editAGV_MoveSpeed->value();

        recordStepMoveEnd(targetName, currentValue);
        ui->statusBar->showMessage(QString("步进模式：目标%1，步进结束").arg(targetName), 2000);
        return;
    }

    // 检查各个输入框是否有内容，有则记录历史记录
    if (m_editJ1MoveStep && !m_editJ1MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value1"); // J1当前角度
        recordStepMoveEnd("悬臂组件(J1)", currentAngle);
    }

    if (m_editJ2MoveStep && !m_editJ2MoveStep->text().isEmpty()) {
        double currentHeight = getSliderLabelValue("label_Value2"); // J2当前高度
        recordStepMoveEnd("升降组件(J2)", currentHeight);
    }

    if (m_editJ3MoveStep && !m_editJ3MoveStep->text().isEmpty()) {
        double currentLength = getSliderLabelValue("label_Value3"); // J3当前长度
        recordStepMoveEnd("伸缩臂(J3)", currentLength);
    }

    if (m_editJ4MoveStep && !m_editJ4MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value4"); // J4当前角度
        recordStepMoveEnd("柔顺组件(J4)", currentAngle);
    }

    ui->statusBar->showMessage("步进模式：使能按钮释放", 2000);
}

int MainWindow::selectedStepTargetRegister() const
{
    if (!m_stepTargetGroup || !m_stepTargetGroup->checkedButton()) {
        return 500;
    }

    const QString name = m_stepTargetGroup->checkedButton()->objectName();
    if (name == "btnStepTargetAxis1") return 500;
    if (name == "btnStepTargetAxis2") return 501;
    if (name == "btnStepTargetAxis3") return 502;
    if (name == "btnStepTargetAxis4") return 503;
    if (name == "btnStepTargetAgv") return 504;
    return 500;
}

QString MainWindow::selectedStepTargetName() const
{
    const int reg = selectedStepTargetRegister();
    switch (reg) {
    case 500: return "悬臂组件(J1)";
    case 501: return "升降组件(J2)";
    case 502: return "伸缩臂(J3)";
    case 503: return "柔顺组件(J4)";
    case 504: return "底盘(AGV)";
    default: return "悬臂组件(J1)";
    }
}

void MainWindow::updateStepMoveGroupBoxState()
{
    if (!ui) {
        return;
    }

    bool isStepMode = (!m_stepModeUnknown && m_stepModeEnabled);
    if (!isStepMode && ui->TBtn_Stepmove) {
        isStepMode = (ui->TBtn_Stepmove->text().trimmed() == "步进模式");
    }

    const bool isFirstPage = ui->StackedWidget && ui->StackedWidget->currentIndex() == 0;
    const bool shouldDisable = (!isStepMode && isFirstPage);
    if (ui->groupBox_StepMove) {
        ui->groupBox_StepMove->setEnabled(!shouldDisable);
    }

    QGroupBox *sixAxisGroup = findChild<QGroupBox*>("groupBox_SixAxies_StepMove");
    if (sixAxisGroup) {
        const bool isSixAxisPage = ui->StackedWidget && ui->StackedWidget->currentIndex() == 3;
        const bool sixAxisShouldDisable = (!isStepMode && isSixAxisPage);
        sixAxisGroup->setEnabled(!sixAxisShouldDisable);
    }
}

void MainWindow::updateStepTargetButtonsState()
{
    updateStepMoveGroupBoxState();

    QToolButton *axis1Btn = findChild<QToolButton*>("btnStepTargetAxis1");
    QToolButton *axis2Btn = findChild<QToolButton*>("btnStepTargetAxis2");
    QToolButton *axis3Btn = findChild<QToolButton*>("btnStepTargetAxis3");
    QToolButton *axis4Btn = findChild<QToolButton*>("btnStepTargetAxis4");
    QToolButton *agvBtn = findChild<QToolButton*>("btnStepTargetAgv");
    const QList<QToolButton*> firstPageTargetButtons = {axis1Btn, axis2Btn, axis3Btn, axis4Btn, agvBtn};
    const bool isFirstPage = ui && ui->StackedWidget && ui->StackedWidget->currentIndex() == 0;
    const bool useCoordinateDisplay = isFirstPage && !m_stepModeEnabled && !m_moveModeUnknown && !m_isJointMode;

    for (QToolButton *btn : firstPageTargetButtons) {
        if (!btn) {
            continue;
        }
        if (!btn->property("stepTargetDefaultText").isValid()) {
            btn->setProperty("stepTargetDefaultText", btn->text());
        }
    }

    if (useCoordinateDisplay) {
        if (axis1Btn) axis1Btn->setText("X");
        if (axis2Btn) axis2Btn->setText("Y");
        if (axis3Btn) axis3Btn->setText("Z");
        if (axis4Btn) axis4Btn->setText("R");
        if (agvBtn) agvBtn->setText("无");
    } else {
        for (QToolButton *btn : firstPageTargetButtons) {
            if (!btn) {
                continue;
            }
            const QVariant defaultText = btn->property("stepTargetDefaultText");
            if (defaultText.isValid()) {
                btn->setText(defaultText.toString());
            }
        }
    }

    auto updateGroupState = [this](QButtonGroup *group, const QString &defaultButtonName) {
        if (!group) {
            return;
        }

        const auto btns = group->buttons();

        if (!m_stepModeUnknown && m_stepModeEnabled) {
            group->setExclusive(true);
            if (!group->checkedButton()) {
                for (QAbstractButton *btn : btns) {
                    if (btn && btn->objectName() == defaultButtonName) {
                        btn->setChecked(true);
                        break;
                    }
                }
            }
            for (QAbstractButton *btn : btns) {
                if (btn) {
                    btn->setEnabled(true);
                }
            }
            return;
        }

        // 点动/未选择模式：按钮保持亮显可见，不做互斥
        group->setExclusive(false);
        for (QAbstractButton *btn : btns) {
            if (!btn) {
                continue;
            }
            btn->setEnabled(true);
            btn->setChecked(false);
        }
    };

    updateGroupState(m_stepTargetGroup, "btnStepTargetAxis1");
    updateGroupState(m_sixAxisStepTargetGroup, "btnStepTargetSixAxis1");
}

// 设置步进模式控制
void MainWindow::setupStepMoveControl()
{
    if (!isFeatureEnabled("motion_control", "motion.step_mode")) {
        return;
    }

    m_btnStepMove = findChild<QToolButton*>("TBtn_Stepmove");

    QToolButton *axis1Btn = findChild<QToolButton*>("btnStepTargetAxis1");
    QToolButton *axis2Btn = findChild<QToolButton*>("btnStepTargetAxis2");
    QToolButton *axis3Btn = findChild<QToolButton*>("btnStepTargetAxis3");
    QToolButton *axis4Btn = findChild<QToolButton*>("btnStepTargetAxis4");
    QToolButton *agvBtn = findChild<QToolButton*>("btnStepTargetAgv");

    if (!m_stepTargetGroup) {
        m_stepTargetGroup = new QButtonGroup(this);
        m_stepTargetGroup->setExclusive(true);
    }

    for (QAbstractButton *btn : m_stepTargetGroup->buttons()) {
        m_stepTargetGroup->removeButton(btn);
    }

    const QList<QToolButton*> stepTargetButtons = {axis1Btn, axis2Btn, axis3Btn, axis4Btn, agvBtn};
    for (QToolButton *btn : stepTargetButtons) {
        if (!btn) {
            continue;
        }
        btn->setCheckable(true);
        btn->setStyleSheet(
            "QToolButton {"
            "    background-color: #6f7f8f;"
            "    color: #eaf3ff;"
            "    border: 1px solid #8698ab;"
            "    border-radius: 8px;"
            "    font-weight: bold;"
            "}"
            "QToolButton:hover {"
            "    border: 1px solid #b6c9de;"
            "}"
            "QToolButton:checked {"
            "    background-color: #00a8ff;"
            "    color: #ffffff;"
            "    border: 1px solid #7fd8ff;"
            "}");
        m_stepTargetGroup->addButton(btn);

        connect(btn, &QToolButton::clicked, this, [this, btn]() {
            if (!m_stepModeEnabled || m_stepModeUnknown || !btn || !btn->isChecked()) {
                return;
            }

            int targetCode = 1;
            const QString n = btn->objectName();
            if (n == "btnStepTargetAxis1") targetCode = 1;
            else if (n == "btnStepTargetAxis2") targetCode = 2;
            else if (n == "btnStepTargetAxis3") targetCode = 3;
            else if (n == "btnStepTargetAxis4") targetCode = 4;
            else if (n == "btnStepTargetAgv") targetCode = 5;

            writeToMainDevice(500, targetCode);
            ui->statusBar->showMessage(QString("步进目标切换：%1 (500=%2)")
                                           .arg(btn->text()).arg(targetCode), 1500);
        }, Qt::UniqueConnection);
    }

    QToolButton *axis1Btn2 = findChild<QToolButton*>("btnStepTargetSixAxis1");
    QToolButton *axis2Btn2 = findChild<QToolButton*>("btnStepTargetSixAxis2");
    QToolButton *axis3Btn2 = findChild<QToolButton*>("btnStepTargetSixAxis3");
    QToolButton *axis4Btn2 = findChild<QToolButton*>("btnStepTargetSixAxis4");
    QToolButton *axis5Btn2 = findChild<QToolButton*>("btnStepTargetSixAxis5");
    QLineEdit *sixStepValueEdit = findChild<QLineEdit*>("lineEdit_SixAxies_StepValue");

    if (axis1Btn2) axis1Btn2->setText("RX");
    if (axis2Btn2) axis2Btn2->setText("RY");
    if (axis3Btn2) axis3Btn2->setText("RZ");
    if (axis4Btn2) axis4Btn2->setText("X");
    if (axis5Btn2) axis5Btn2->setText("Y");

    QToolButton *axis6Btn2 = findChild<QToolButton*>("btnStepTargetSixAxis6");
    if (!axis6Btn2) {
        QWidget *stepTargetList2 = findChild<QWidget*>("widget_SixAxies_StepTargetList");
        QVBoxLayout *stepTargetLayout2 = findChild<QVBoxLayout*>("verticalLayout_SixAxies_StepTargetList");
        if (stepTargetList2 && stepTargetLayout2) {
            axis6Btn2 = new QToolButton(stepTargetList2);
            axis6Btn2->setObjectName("btnStepTargetSixAxis6");
            axis6Btn2->setText("Z");
            axis6Btn2->setCheckable(true);
            stepTargetLayout2->addWidget(axis6Btn2);
        }
    } else {
        axis6Btn2->setText("Z");
    }

    if (sixStepValueEdit) {
        QRegularExpression regExp("^-?\\d+(\\.\\d+)?$");
        sixStepValueEdit->setValidator(new QRegularExpressionValidator(regExp, sixStepValueEdit));
        connect(sixStepValueEdit, &QLineEdit::editingFinished, this, [this, sixStepValueEdit]() {
            const QString stepText = sixStepValueEdit->text().trimmed();
            if (stepText.isEmpty() || !m_recorder) {
                return;
            }
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = getControlPageName(sixStepValueEdit);
            record.controlName = QStringLiteral("步进值被设定为%1").arg(stepText);
            record.controlType = "";
            record.operation = "";
            record.oldValue = "";
            record.newValue = "";
            m_recorder->addRecord(record);
        }, Qt::UniqueConnection);
    }

    if (!m_sixAxisStepTargetGroup) {
        m_sixAxisStepTargetGroup = new QButtonGroup(this);
        m_sixAxisStepTargetGroup->setExclusive(true);
    }
    for (QAbstractButton *btn : m_sixAxisStepTargetGroup->buttons()) {
        m_sixAxisStepTargetGroup->removeButton(btn);
    }

    const QList<QToolButton*> sixAxisButtons = {axis1Btn2, axis2Btn2, axis3Btn2, axis4Btn2, axis5Btn2, axis6Btn2};
    for (QToolButton *btn : sixAxisButtons) {
        if (!btn) {
            continue;
        }
        btn->setCheckable(true);
        btn->setStyleSheet(
            "QToolButton {"
            "    background-color: #6f7f8f;"
            "    color: #eaf3ff;"
            "    border: 1px solid #8698ab;"
            "    border-radius: 8px;"
            "    font-weight: bold;"
            "}"
            "QToolButton:hover {"
            "    border: 1px solid #b6c9de;"
            "}"
            "QToolButton:checked {"
            "    background-color: #00a8ff;"
            "    color: #ffffff;"
            "    border: 1px solid #7fd8ff;"
            "}");
        m_sixAxisStepTargetGroup->addButton(btn);

        connect(btn, &QToolButton::clicked, this, [this, btn]() {
            if (!m_stepModeEnabled || m_stepModeUnknown || !btn || !btn->isChecked()) {
                return;
            }

            int targetCode = 1;
            const QString n = btn->objectName();
            if (n == "btnStepTargetSixAxis1") targetCode = 1;
            else if (n == "btnStepTargetSixAxis2") targetCode = 2;
            else if (n == "btnStepTargetSixAxis3") targetCode = 3;
            else if (n == "btnStepTargetSixAxis4") targetCode = 4;
            else if (n == "btnStepTargetSixAxis5") targetCode = 5;
            else if (n == "btnStepTargetSixAxis6") targetCode = 6;

            writeToMainDevice(614, targetCode);
            ui->statusBar->showMessage(QString("六轴步进目标切换：%1 (614=%2)")
                                           .arg(btn->text()).arg(targetCode), 1500);
        }, Qt::UniqueConnection);
    }

    if (axis1Btn2 && !m_sixAxisStepTargetGroup->checkedButton()) {
        axis1Btn2->setChecked(true);
    }

    if (axis1Btn && !m_stepTargetGroup->checkedButton()) {
        axis1Btn->setChecked(true);
    }

    if (m_btnStepMove) {
        // 启动时先显示未选择，随后由寄存器值回填
        m_btnStepMove->setText("未选择模式");
        m_btnStepMove->setToolTip("当前模式：未选择模式");

        // 样式设置
        m_btnStepMove->setStyleSheet(
            "QToolButton {"
            "    background-color: #3498DB;"
            "    color: white;"
            "    border: 2px solid #2980B9;"
            "    border-radius: 8px;"
            "    padding: 8px 16px;"
            "    font-weight: bold;"
            "    font-size: 14px;"
            "}"
            "QToolButton:hover {"
            "    background-color: #4A9EFF;"
            "}"
            "QToolButton:pressed {"
            "    background-color: #1A5FB4;"
            "}"
            );

        qCDebug(lcMainWindow) << "步进/点动模式按钮初始化完成";

        if (g_registerCache.contains(125)) {
            const quint16 value = g_registerCache.value(125);
            if (value == 2) {
                m_stepModeUnknown = false;
                m_stepModeEnabled = true;
                m_btnStepMove->setText("步进模式");
                m_btnStepMove->setToolTip("当前模式：步进模式");
            } else if (value == 1) {
                m_stepModeUnknown = false;
                m_stepModeEnabled = false;
                m_btnStepMove->setText("点动模式");
                m_btnStepMove->setToolTip("当前模式：点动模式");
            } else {
                m_stepModeUnknown = true;
                m_btnStepMove->setText("未选择模式");
                m_btnStepMove->setToolTip("当前模式：未选择模式");
            }
        }

        updateStepTargetButtonsState();
    } else {
        qWarning() << "未找到TBtn_Stepmove按钮";
    }

    updateStepTargetButtonsState();
}

// 设置步进值输入框
void MainWindow::setupStepMoveLineEdits()
{
    if (!isFeatureEnabled("motion_control", "motion.step_mode")) {
        return;
    }

    // 新版UI：统一步进输入框 + 轴/AGV互斥目标按钮
    m_stepValueEdit = findChild<QLineEdit*>("lineEdit_StepValue");

    QRegularExpression regExp("^-?\\d+(\\.\\d+)?$");  // 匹配整数/小数，可正可负
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regExp, this);

    if (m_stepValueEdit) {
        m_stepValueEdit->setValidator(validator);
        m_stepValueEdit->setPlaceholderText("...");

        connect(m_stepValueEdit, &QLineEdit::textChanged, this,
                [this](const QString &text) {
            if (text.isEmpty()) {
                return;
            }
            bool ok = false;
            const double value = text.toDouble(&ok);
            if (!ok) {
                return;
            }
            writeStepValueDoubleToMainDevice(value);
        }, Qt::UniqueConnection);
        connect(m_stepValueEdit, &QLineEdit::editingFinished, this, [this]() {
            if (!m_stepValueEdit || !m_recorder) {
                return;
            }
            const QString stepText = m_stepValueEdit->text().trimmed();
            if (stepText.isEmpty()) {
                return;
            }
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = getControlPageName(m_stepValueEdit);
            record.controlName = QStringLiteral("步进值被设定为%1").arg(stepText);
            record.controlType = "";
            record.operation = "";
            record.oldValue = "";
            record.newValue = "";
            m_recorder->addRecord(record);
        }, Qt::UniqueConnection);

        qCDebug(lcMainWindow) << "统一步进值输入框初始化完成";
        return;
    }

    // 查找四个步进值输入框
    m_editJ1MoveStep = findChild<QLineEdit*>("LEdit_HoriSupSec_J1MoveStep");
    m_editJ2MoveStep = findChild<QLineEdit*>("LEdit_VeSupSec_J2MoveStep");
    m_editJ3MoveStep = findChild<QLineEdit*>("LEdit_HoriSupSec_J3MoveStep");
    m_editJ4MoveStep = findChild<QLineEdit*>("LEdit_EOAT_J4MoveStep");

    if (m_editJ1MoveStep) {
        m_editJ1MoveStep->setValidator(validator);
        m_editJ1MoveStep->setPlaceholderText("输入J1步进值(整数)");
        connect(m_editJ1MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ1MoveStepChanged);
    }

    if (m_editJ2MoveStep) {
        m_editJ2MoveStep->setValidator(validator);
        m_editJ2MoveStep->setPlaceholderText("输入J2步进值(整数)");
        connect(m_editJ2MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ2MoveStepChanged);
    }

    if (m_editJ3MoveStep) {
        m_editJ3MoveStep->setValidator(validator);
        m_editJ3MoveStep->setPlaceholderText("输入J3步进值(整数)");
        connect(m_editJ3MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ3MoveStepChanged);
    }

    if (m_editJ4MoveStep) {
        m_editJ4MoveStep->setValidator(validator);
        m_editJ4MoveStep->setPlaceholderText("输入J4步进值(整数)");
        connect(m_editJ4MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ4MoveStepChanged);
    }

    qCDebug(lcMainWindow) << "步进值输入框初始化完成";
}

// J1步进值变化
void MainWindow::onJ1MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入500地址
            writeToMainDevice(500, value);
            qCDebug(lcMainWindow) << "J1步进值:" << value << "，写入地址500";
        }
    }
}

// J2步进值变化
void MainWindow::onJ2MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入501地址
            writeToMainDevice(501, value);
            qCDebug(lcMainWindow) << "J2步进值:" << value << "，写入地址501";
        }
    }
}

// J3步进值变化
void MainWindow::onJ3MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入502地址
            writeToMainDevice(502, value);
            qCDebug(lcMainWindow) << "J3步进值:" << value << "，写入地址502";
        }
    }
}

// J4步进值变化
void MainWindow::onJ4MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入503地址
            writeToMainDevice(503, value);
            qCDebug(lcMainWindow) << "J4步进值:" << value << "，写入地址503";
        }
    }
}

void MainWindow::writeStepValueDoubleToMainDevice(double value)
{
    const auto regs = doubleToRegistersGHEFCDAB(value);

    QVector<quint16> values;
    values.reserve(4);
    values << regs[0] << regs[1] << regs[2] << regs[3];

    const bool batchOk = MainDeviceModbusApi::writeRegisters(m_modbusManager, 502, values);
    if (!batchOk) {
        // 兼容回退：若批量写失败，则保持原有逐寄存器写行为。
        writeToMainDevice(502, static_cast<int>(regs[0]));
        writeToMainDevice(503, static_cast<int>(regs[1]));
        writeToMainDevice(504, static_cast<int>(regs[2]));
        writeToMainDevice(505, static_cast<int>(regs[3]));
    }

    qCDebug(lcMainWindow) << "步进值(double, GH EF CD AB)写入502~505:" << value
                          << "=>" << regs[0] << regs[1] << regs[2] << regs[3]
                          << "批量写=" << batchOk;
}

// 写入步进寄存器
void MainWindow::writeStepMoveRegisters()
{
    if (m_stepValueEdit && !m_stepValueEdit->text().isEmpty()) {
        bool ok = false;
        const double value = m_stepValueEdit->text().toDouble(&ok);
        if (!ok) {
            return;
        }

        const int targetRegister = selectedStepTargetRegister();
        if (targetRegister != 504) {
            writeStepValueDoubleToMainDevice(value);
            qCDebug(lcMainWindow) << "步进目标:" << selectedStepTargetName()
                                  << "步进值(double):" << value;
        } else {
            qCDebug(lcMainWindow) << "当前为AGV目标，跳过502~505双浮点写入";
        }
        return;
    }

    // 将当前输入框的值写入对应的寄存器
    if (m_editJ1MoveStep && !m_editJ1MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ1MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(500, value);
        }
    }

    if (m_editJ2MoveStep && !m_editJ2MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ2MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(501, value);
        }
    }

    if (m_editJ3MoveStep && !m_editJ3MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ3MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(502, value);
        }
    }

    if (m_editJ4MoveStep && !m_editJ4MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ4MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(503, value);
        }
    }
}

// 清空步进寄存器
void MainWindow::clearStepMoveRegisters()
{
    // 清空502-505地址（步进值双浮点区）
    for (int i = 502; i <= 505; i++) {
        writeToMainDevice(i, 0);
    }

    // 清空输入框内容
    if (m_stepValueEdit) m_stepValueEdit->clear();
    if (m_editJ1MoveStep) m_editJ1MoveStep->clear();
    if (m_editJ2MoveStep) m_editJ2MoveStep->clear();
    if (m_editJ3MoveStep) m_editJ3MoveStep->clear();
    if (m_editJ4MoveStep) m_editJ4MoveStep->clear();

    qCDebug(lcMainWindow) << "已清空步进寄存器(502-505)和输入框内容";
}

// 记录步进动作开始
void MainWindow::recordStepMoveAction(const QString &jointName, double currentValue, const QString &stepValue, bool start)
{
    Q_UNUSED(start);
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    QString msg;
    if (jointName == "RX" || jointName == "RY" || jointName == "RZ") {
        msg = QString("%1当前角度为%2，步进%3°")
                  .arg(jointName)
                  .arg(currentValue, 0, 'f', 3)
                  .arg(stepValue);
    } else if (jointName == "X" || jointName == "Y" || jointName == "Z") {
        msg = QString("%1轴当前位置为%2，步进%3mm")
                  .arg(jointName)
                  .arg(currentValue, 0, 'f', 3)
                  .arg(stepValue);
    } else if (jointName.contains("J1")) {
        msg = QString("立柱旋转当前角度为%1°，开始步进%2°").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J2")) {
        msg = QString("立柱升降当前高度为%1mm，开始步进%2mm").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J3")) {
        msg = QString("伸缩臂当前长度为%1mm，开始步进%2mm").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J4")) {
        msg = QString("末端组件当前角度为%1°，开始步进%2°").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else {
        msg = QString("%1当前%2为%3，步进%4°")
                  .arg(jointName)
                  .arg(jointName.contains("角度") ? "角度" :
                           jointName.contains("高度") ? "高度" : "长度")
                  .arg(currentValue, 0, 'f', 1)
                  .arg(stepValue);
    }

    // 六自由度步进：历史列表「操作详情」只显示约定文案，不出现 StepMove_* / 开始步进移动 等。
    const bool sixAxisStepDetailOnly = (jointName == "RX" || jointName == "RY" || jointName == "RZ"
                                        || jointName == "X" || jointName == "Y" || jointName == "Z");
    if (sixAxisStepDetailOnly) {
        record.controlName = msg;
        record.controlType = "";
        record.operation = "";
        record.oldValue = "";
        record.newValue = "";
    } else {
        record.controlName = "StepMove_" + jointName;
        record.controlType = "StepMove";
        record.operation = "step_move_start";
        record.oldValue = "";
        record.newValue = msg;
    }

    m_recorder->addRecord(record);
    showNotification(msg);
}

// 记录步进动作结束
void MainWindow::recordStepMoveEnd(const QString &jointName, double currentValue, bool motionStopStyle)
{
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "StepMove_" + jointName;
    record.controlType = "StepMove";
    record.operation = "step_move_end";
    record.oldValue = "";
    QString msg;
    if (motionStopStyle) {
        if (jointName.contains("J1")) {
            msg = QString("运动停止，当前立柱旋转角度为%1°").arg(currentValue, 0, 'f', 1);
        } else if (jointName.contains("J2")) {
            msg = QString("运动停止，当前立柱升降高度为%1mm").arg(currentValue, 0, 'f', 1);
        } else if (jointName.contains("J3")) {
            msg = QString("运动停止，当前伸缩臂长度为%1mm").arg(currentValue, 0, 'f', 1);
        } else if (jointName.contains("J4")) {
            msg = QString("运动停止，当前末端组件角度为%1°").arg(currentValue, 0, 'f', 1);
        } else if (jointName.contains("AGV") || jointName.contains("底盘")) {
            msg = QString("运动停止，当前底盘速度为%1").arg(currentValue, 0, 'f', 1);
        } else {
            const QString valueLabel = jointName.contains("角度") ? "角度"
                                         : jointName.contains("高度") ? "高度"
                                         : jointName.contains("长度") ? "长度"
                                         : "值";
            msg = QString("运动停止，当前%1%2为%3")
                      .arg(jointName)
                      .arg(valueLabel)
                      .arg(currentValue, 0, 'f', 1);
        }
    } else if (jointName.contains("J1")) {
        msg = QString("立柱旋转当前角度为%1°，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J2")) {
        msg = QString("立柱升降当前高度为%1mm，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J3")) {
        msg = QString("伸缩臂当前长度为%1mm，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J4")) {
        msg = QString("末端组件当前角度为%1°，步进结束").arg(currentValue, 0, 'f', 1);
    } else {
        msg = QString("%1当前%2为%3，步进结束")
                  .arg(jointName)
                  .arg(jointName.contains("角度") ? "角度" :
                           jointName.contains("高度") ? "高度" : "长度")
                  .arg(currentValue, 0, 'f', 1);
    }
    record.newValue = msg;

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

void MainWindow::markStepMotionPendingStop(StepMotionStopKind kind,
                                           const QString &targetName,
                                           int externalKeyNumber)
{
    PendingStepMotionStop pending;
    pending.kind = kind;
    pending.targetName = targetName;
    pending.externalKeyNumber = externalKeyNumber;
    m_pendingStepMotionStops.append(pending);
}

void MainWindow::flushPendingStepMotionStopsOnEnableRelease()
{
    if (m_pendingStepMotionStops.isEmpty()) {
        return;
    }

    const QVector<PendingStepMotionStop> pendingStops = m_pendingStepMotionStops;
    m_pendingStepMotionStops.clear();

    for (const PendingStepMotionStop &pending : pendingStops) {
        switch (pending.kind) {
        case StepMotionStopKind::RobotJoint: {
            double currentValue = 0.0;
            if (pending.targetName.contains("J1")) {
                currentValue = getSliderLabelValue("label_Value1");
            } else if (pending.targetName.contains("J2")) {
                currentValue = getSliderLabelValue("label_Value2");
            } else if (pending.targetName.contains("J3")) {
                currentValue = getSliderLabelValue("label_Value3");
            } else if (pending.targetName.contains("J4")) {
                currentValue = getSliderLabelValue("label_Value4");
            } else if (pending.targetName.contains("AGV") || pending.targetName.contains("底盘")) {
                currentValue = m_editAGV_MoveSpeed ? m_editAGV_MoveSpeed->value()
                                                   : getSliderEditValue("SEdit_AGV_MoveSpeed");
            }
            recordStepMoveEnd(pending.targetName, currentValue, true);
            break;
        }
        case StepMotionStopKind::SixAxis:
            if (pending.externalKeyNumber >= 1 && pending.externalKeyNumber <= 12) {
                recordSixAxisJogExternalKey(pending.externalKeyNumber, false);
            }
            break;
        case StepMotionStopKind::Agv:
            if (pending.externalKeyNumber >= 1) {
                appendAgvExternalKeyRecord(pending.externalKeyNumber, false);
            }
            break;
        }
    }
}

void MainWindow::recordSixAxisJogExternalKey(int keyNumber, bool pressed)
{
    if (!m_recorder || keyNumber < 1 || keyNumber > 12) {
        return;
    }

    const int axisIndex = (keyNumber + 1) / 2; // ○1/2->1 … ○11/12->6
    QString axisName;
    switch (axisIndex) {
    case 1: axisName = QStringLiteral("RX"); break;
    case 2: axisName = QStringLiteral("RY"); break;
    case 3: axisName = QStringLiteral("RZ"); break;
    case 4: axisName = QStringLiteral("X"); break;
    case 5: axisName = QStringLiteral("Y"); break;
    case 6: axisName = QStringLiteral("Z"); break;
    default: return;
    }

    double currentValue = 0.0;
    const QString sixAxisGaugeName = QStringLiteral("robot_ArcGauge_SixAxis%1").arg(axisIndex);
    if (TechArcGauge *gauge = m_arcGauges.value(sixAxisGaugeName, nullptr)) {
        currentValue = gauge->value();
    }

    const bool isRotationAxis = (axisIndex <= 3);
    QString msg;
    if (pressed) {
        if (isRotationAxis) {
            msg = QStringLiteral("%1当前角度为%2，开始运动")
                      .arg(axisName)
                      .arg(currentValue, 0, 'f', 3);
        } else {
            msg = QStringLiteral("%1轴当前位置为%2，开始运动")
                      .arg(axisName)
                      .arg(currentValue, 0, 'f', 3);
        }
    } else {
        if (isRotationAxis) {
            msg = QStringLiteral("%1当前角度为%2，运动停止")
                      .arg(axisName)
                      .arg(currentValue, 0, 'f', 3);
        } else {
            msg = QStringLiteral("%1轴当前位置为%2，运动停止")
                      .arg(axisName)
                      .arg(currentValue, 0, 'f', 3);
        }
    }

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = msg;
    record.controlType = QString();
    record.operation = QString();
    record.oldValue = QString();
    record.newValue = QString();
    m_recorder->addRecord(record);
    showNotification(msg);
}

void MainWindow::sendRemoveWarningModbusWrites()
{
    const QList<ModbusRegisterSpec> writes = buttonModbusBinding(QStringLiteral("TBtn_RemoveWarning")).writes;
    if (writes.isEmpty()) {
        writeToMainDevice(290, 1);
        writeToAGVDevice(290, 1);
        return;
    }
    executeConfiguredRegisterWrites(writes, 1, QStringLiteral("清除报警"));
}

// 修改：原有的清除报警按钮函数
void MainWindow::on_TBtn_RemoveWarning_clicked()
{
    qCDebug(lcMainWindow) << "用户点击清除报警按钮";

    sendRemoveWarningModbusWrites();

    // 清除所有报警状态
    if (m_emergencyStopAlarm) {
        m_emergencyStopAlarm = false;
        qCDebug(lcMainWindow) << "用户清除了紧急停止报警";
    }

    // 更新报警显示
    updateAlarmDisplay();

    // 显示通知
    showNotification("报警已清除");

}
// 新增：设置报警系统
void MainWindow::setupAlarmSystem()
{
    if (!isBigFeatureEnabled("alarm_system")) {
        qCDebug(lcMainWindow) << "报警系统已关闭，跳过初始化";
        return;
    }

    qCDebug(lcMainWindow) << "初始化报警系统...";

    // 初始化报警状态
    m_emergencyStopAlarm = false;
    m_emergencyStopColumnFlag = false;
    m_emergencyStopChassisFlag = false;
    m_robotArmEmergency150Flag = false;
    m_agvChassisEmergency51Bit5Flag = false;
    m_agvStationOffline51Bit1Flag = false;
    m_agvDriveFault51Bit2Flag = false;
    m_agvBatteryLow51Bit0Flag = false;
    m_robotWeightOverload150Bit3Flag = false;
    m_robotWeightLock150Bit7Flag = false;
    m_robotWeightLockUserAckedWhileActive = false;
    m_robotAxisSyncDeviation150Bit6Flag = false;
    m_robotPositiveLimit102Bit2Flag = false;
    m_robotNegativeLimit102Bit3Flag = false;
    m_robotLimitDialogTrigger = RobotLimitDialogTrigger::None;
    m_agvBatteryLowAcked = false;
    m_mainRegister150Valid = false;
    m_mainRegister150Shadow = 0;

    // 创建报警检测定时器
    if (!m_alarmCheckTimer) {
        m_alarmCheckTimer = new QTimer(this);
        connect(m_alarmCheckTimer, &QTimer::timeout, this, &MainWindow::checkAlarmConditions);
        m_alarmCheckTimer->start(500);  // 每500ms检查一次，更频繁
    }

    qCDebug(lcMainWindow) << "报警系统初始化完成，定时器已启动";
}
// 新增：检查报警条件
void MainWindow::checkAlarmConditions()
{
    if (!isBigFeatureEnabled("alarm_system")) {
        return;
    }

    const bool alarmStatusLogsEnabled = isFeatureEnabled("alarm_system", "alarm.status_logs");

    // 确保主设备关键报警/提示位持续被读取，避免因轮询覆盖不全导致状态丢失。
    if (MainDeviceModbusApi::isReady(m_modbusManager)) {
        if (isFeatureEnabled("alarm_system", "alarm.emergency_stop")) {
            MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 150, 1);
        }
        MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 102, 1);
        // 卷样机钢缆到位：151.bit0 完全收回 / 151.bit1 完全放出（不在默认 0~84 状态组内）
        MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 151, 1);
    }

    if (m_agvModbusManager && m_agvModbusManager->isConnected()
        && m_agvChassisEmergency51Bit5Flag
        && isFeatureEnabled("alarm_system", "alarm.emergency_stop")) {
        m_agvModbusManager->readMultipleRegisters(150, 1);
    }

    // 调试输出当前报警状态
    if (alarmStatusLogsEnabled) {
        qCDebug(lcMainWindow) << "=== 检查报警条件 ===";
        qCDebug(lcMainWindow) << "机械臂急停标志(150):" << m_robotArmEmergency150Flag;
        qCDebug(lcMainWindow) << "AGV急停标志(51.bit5):" << m_agvChassisEmergency51Bit5Flag;
        qCDebug(lcMainWindow) << "紧急停止报警:" << m_emergencyStopAlarm;
    }

    // 1. 仅检查两条急停报警源：主设备150 + AGV 51.bit5
    bool newEstopState = false;
    if (isFeatureEnabled("alarm_system", "alarm.emergency_stop")) {
        newEstopState = (m_robotArmEmergency150Flag
                         || m_agvChassisEmergency51Bit5Flag);
    }
    if (newEstopState != m_emergencyStopAlarm) {
        if (newEstopState) {
            qCDebug(lcMainWindow) << "!!! 触发紧急停止报警 !!!";
        } else {
            qCDebug(lcMainWindow) << "解除紧急停止报警";
        }
        m_emergencyStopAlarm = newEstopState;
    }

    // ods %MX0.6：1=正常，0=急停。命令源与报警一致：主控150 + AGV 51.bit5
    syncAgvHostEmergencyStopCommand(newEstopState);

    // 2. 统一更新显示
    updateAlarmDisplay();
    if (m_agvBatteryLow51Bit0Flag) {
        showAgvBatteryLowDialog();
    }

    if (alarmStatusLogsEnabled) {
        qCDebug(lcMainWindow) << "=== 报警检查结束 ===";
    }
}

// 新增：显示报警
void MainWindow::showAlarm(const QString &message, const QString &color, bool closable)
{
    if (!isFeatureEnabled("alarm_system", "alarm.popup")) {
        return;
    }
    if (!userPopupsAllowed()) {
        return;
    }

    qCDebug(lcMainWindow) << "showAlarm被调用，消息:" << message << "颜色:" << color << "可关闭:" << closable;

    // 确保在主线程中执行
    if (QThread::currentThread() != this->thread()) {
        qCDebug(lcMainWindow) << "showAlarm不在主线程，将使用invokeMethod";
        QMetaObject::invokeMethod(this, "showAlarm",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, message),
                                  Q_ARG(QString, color),
                                  Q_ARG(bool, closable));
        return;
    }

    // 如果报警窗口不存在，则创建它
    if (!m_alarmWidget) {
        qCDebug(lcMainWindow) << "创建新的报警窗口...";

        // 创建报警窗口
        m_alarmWidget = new QWidget(nullptr);  // 使用nullptr作为父窗口，使其独立显示
        m_alarmWidget->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                      Qt::WindowStaysOnTopHint);
        m_alarmWidget->setWindowModality(Qt::ApplicationModal);
        m_alarmWidget->setObjectName("alarmWidget");

        // 创建布局
        QVBoxLayout *layout = new QVBoxLayout(m_alarmWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        // 创建报警信息标签
        m_alarmLabel = new QLabel(m_alarmWidget);
        m_alarmLabel->setAlignment(Qt::AlignCenter);
        m_alarmLabel->setWordWrap(true);
        layout->addWidget(m_alarmLabel);

        // 创建清除报警按钮
        QPushButton *clearBtn = new QPushButton("清除报警", m_alarmWidget);
        clearBtn->setObjectName("clearAlarmBtn");
        layout->addWidget(clearBtn);

        connect(clearBtn, &QPushButton::clicked, this, &MainWindow::on_TBtn_RemoveWarning_clicked);

        // 初始大小；具体高度在 showAlarm 中按文案行数调整
        m_alarmWidget->setFixedSize(420, 120);

        qCDebug(lcMainWindow) << "报警窗口创建完成";
    }
    
    // 控制清除按钮的显示/隐藏
    QPushButton *clearBtn = m_alarmWidget->findChild<QPushButton*>("clearAlarmBtn");
    if (clearBtn) {
        clearBtn->setVisible(closable);
    }
    
    // 更新报警信息
    m_alarmLabel->setText(message);

    {
        const int lineCount = qMax(1, message.count(QLatin1Char('\n')) + 1);
        const int buttonReserve = closable ? 52 : 0;
        const int targetH = qBound(120, 56 + lineCount * 22 + buttonReserve, 520);
        m_alarmWidget->setFixedSize(420, targetH);
    }

    // 设置样式
    QString styleSheet = QString(
                             "#alarmWidget {"
                             "    background-color: rgba(30, 0, 0, 230);"
                             "    border: 3px solid %1;"
                             "    border-radius: 10px;"
                             "}"
                             "QLabel {"
                             "    color: %1;"
                             "    font-size: 18px;"
                             "    font-weight: bold;"
                             "    background-color: transparent;"
                             "}"
                             "#clearAlarmBtn {"
                             "    background-color: %1;"
                             "    color: #ffffff;"
                             "    border: 2px solid %2;"
                             "    border-radius: 6px;"
                             "    padding: 8px 16px;"
                             "    font-size: 14px;"
                             "    font-weight: bold;"
                             "    min-width: 100px;"
                             "}"
                             "#clearAlarmBtn:hover {"
                             "    background-color: %2;"
                             "    border-color: %1;"
                             "}"
                             ).arg(color)
                             .arg(color == "#ff5555" ? "#ff8888" : "#ffaa55");

    m_alarmWidget->setStyleSheet(styleSheet);

    // 计算显示位置（屏幕右下角，随窗口高度变化）
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = screenGeometry.width() - m_alarmWidget->width() - 50;
    int y = screenGeometry.height() - m_alarmWidget->height() - 50;

    // 显示报警窗口
    m_alarmWidget->move(x, y);
    m_alarmWidget->show();
    m_alarmWidget->raise();
    m_alarmWidget->activateWindow();

    qCDebug(lcMainWindow) << "报警窗口显示在位置: (" << x << "," << y << ")";
}
// 新增：隐藏报警
void MainWindow::hideAlarm()
{
    if (m_alarmWidget && m_alarmWidget->isVisible()) {
        m_alarmWidget->hide();
    }
}

QStringList MainWindow::robotArmTeachPendantEstopFromRegister150(quint16 reg150) const
{
    QStringList out;
    if (((reg150 >> 4) & 0x01) != 0) {
        out << QStringLiteral("下方示教器触发急停");
    }
    if (((reg150 >> 5) & 0x01) != 0) {
        out << QStringLiteral("上方示教器触发急停");
    }
    return out;
}

QStringList MainWindow::agvChassisEstopSourcesFromRegister150(quint16 reg150) const
{
    static const QString kNames[11] = {
        QStringLiteral("前面板急停"),
        QStringLiteral("后面板急停"),
        QStringLiteral("前面板左急停"),
        QStringLiteral("前面板右急停"),
        QStringLiteral("后面板左急停"),
        QStringLiteral("后面板右急停"),
        QStringLiteral("右面板左急停"),
        QStringLiteral("右面板右急停"),
        QStringLiteral("左面板左急停"),
        QStringLiteral("左面板右急停"),
        QStringLiteral("遥控器急停"),
    };
    QStringList out;
    for (int bit = 0; bit <= 10; ++bit) {
        if (((reg150 >> bit) & 0x01) != 0) {
            out << kNames[bit];
        }
    }
    return out;
}

// 新增：更新报警显示
void MainWindow::updateAlarmDisplay()
{
    // 如果有任何报警处于激活状态，显示相应的报警
    if (m_emergencyStopAlarm) {
        // 急停优先：出现急停时关闭其他类型提示窗，避免操作员被非急停信息干扰。
        hideNonEmergencyPopups();
        const bool robotEmergency = m_robotArmEmergency150Flag;
        const bool chassisEmergency = m_agvChassisEmergency51Bit5Flag;
        QString alarmMessage;
        if (robotEmergency && chassisEmergency) {
            alarmMessage = "机械臂触发急停，请解除急停。\n底盘触发急停，请解除急停。";
        } else if (robotEmergency) {
            alarmMessage = "机械臂触发急停，请解除急停。";
        } else {
            alarmMessage = "底盘触发急停，请解除急停。";
        }
        if (robotEmergency) {
            const QStringList teachLines = robotArmTeachPendantEstopFromRegister150(
                m_mainRegister150Shadow);
            if (!teachLines.isEmpty()) {
                alarmMessage += QLatin1Char('\n');
                alarmMessage += teachLines.join(QLatin1Char('\n'));
            }
        }
        if (chassisEmergency) {
            const QStringList sources = agvChassisEstopSourcesFromRegister150(
                m_agvRegisterShadow.value(150, 0));
            if (!sources.isEmpty()) {
                alarmMessage += QLatin1Char('\n');
                alarmMessage += sources.join(QLatin1Char('\n'));
            }
        }
        showAlarm(alarmMessage, "#ff5555", false);
    } else {
        // 没有报警时隐藏窗口
        // 只有在没有转向切换报警时才隐藏
        if (!m_isSteeringAlarmActive) {
            hideAlarm();
        }
    }
}

void MainWindow::hideNonEmergencyPopups()
{
    // 主副轴位置偏差提示窗（150.bit6）不在此列表中：急停全屏报警时仍保持可见，直至位6清零。
    // 急停触发时如果清理了驻车切换提示，也要同步释放驻车切换在途锁，
    // 避免按钮继续被 parkingSwitchWaiting 拦截到 90 秒超时。
    if (QTimer *parkingWaitTimer = findChild<QTimer*>("parkingSwitchWaitTimer")) {
        parkingWaitTimer->stop();
        parkingWaitTimer->deleteLater();
    }
    setProperty("parkingSwitchWaiting", false);
    setProperty("parkingTargetBit", -1);
    setProperty("parkingTargetEnabled", false);

    hideParkingSwitchHintDialog();
    hideLegOpenPathCheckDialog();
    hideParkingLegAbnormalDialog();
    hideAgvStationOfflineAlarm();
    hideAgvDriveFaultAlarm();
    hideAgvBatteryLowDialog();
    dismissOperationHintToasts();
    hideWirelessModeWarningDialog();
    hideRobotLimitReachedDialog();
    hideRobotWeightOverloadDialog();
    hideRobotWeightLockDialog();
    hideInclinometerTiltRiskDialog();
    hideInclinometerTiltLockDialog();
}

void MainWindow::handleAGVRegister51Alerts(quint16 value)
{
    const bool stationOffline = (((value >> 1) & 0x01) == 1);
    if (stationOffline != m_agvStationOffline51Bit1Flag) {
        m_agvStationOffline51Bit1Flag = stationOffline;
        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "报警系统";
            record.controlName = "站掉线报警";
            record.controlType = "报警监控";
            record.operation = stationOffline ? "报警触发" : "报警解除";
            record.oldValue = "";
            record.newValue = stationOffline ? "检测到有站掉线" : "站掉线报警已解除";
            m_recorder->addRecord(record);
        }
        if (stationOffline) {
            showAgvStationOfflineAlarm();
        } else {
            hideAgvStationOfflineAlarm();
        }
    }

    const bool driveFault = (((value >> 2) & 0x01) == 1);
    if (driveFault != m_agvDriveFault51Bit2Flag) {
        m_agvDriveFault51Bit2Flag = driveFault;
        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "报警系统";
            record.controlName = "驱动故障报警";
            record.controlType = "报警监控";
            record.operation = driveFault ? "报警触发" : "报警解除";
            record.oldValue = "";
            record.newValue = driveFault ? "检测到驱动故障" : "驱动故障报警已解除";
            m_recorder->addRecord(record);
        }
        if (driveFault) {
            showAgvDriveFaultAlarm();
        } else {
            hideAgvDriveFaultAlarm();
        }
    }

    const bool batteryLow = ((value & 0x01) == 1);
    if (batteryLow != m_agvBatteryLow51Bit0Flag) {
        m_agvBatteryLow51Bit0Flag = batteryLow;
        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "提示系统";
            record.controlName = "低电量提示";
            record.controlType = "提示窗口";
            record.operation = batteryLow ? "提示触发" : "提示解除";
            record.oldValue = "";
            record.newValue = batteryLow ? "电池电量低，请充电" : "低电量提示已解除";
            m_recorder->addRecord(record);
        }
        if (batteryLow) {
            showAgvBatteryLowDialog();
        } else {
            m_agvBatteryLowAcked = false;
            m_lastBatteryLowToastMs = 0;
            hideAgvBatteryLowDialog();
        }
    }
}

namespace {
void positionFloatingPopupTopRight(QWidget *widget, int topOffsetPx)
{
    if (!widget) {
        return;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    const QRect area = screen->availableGeometry();
    const int x = area.right() - widget->width() - 40;
    int y = area.top() + topOffsetPx;
    y = qBound(area.top() + 20, y, area.bottom() - widget->height() - 20);
    widget->move(x, y);
}
}

void MainWindow::showAgvStationOfflineAlarm()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_agvStationOfflineAlarmWidget) {
        m_agvStationOfflineAlarmWidget = new QWidget(nullptr);
        m_agvStationOfflineAlarmWidget->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                                       Qt::WindowStaysOnTopHint);
        m_agvStationOfflineAlarmWidget->setWindowModality(Qt::ApplicationModal);
        m_agvStationOfflineAlarmWidget->setObjectName("agvStationOfflineAlarmWidget");

        QVBoxLayout *layout = new QVBoxLayout(m_agvStationOfflineAlarmWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_agvStationOfflineAlarmLabel = new QLabel(m_agvStationOfflineAlarmWidget);
        m_agvStationOfflineAlarmLabel->setAlignment(Qt::AlignCenter);
        m_agvStationOfflineAlarmLabel->setWordWrap(true);
        m_agvStationOfflineAlarmLabel->setText("检测到有站掉线");
        layout->addWidget(m_agvStationOfflineAlarmLabel);

        m_agvStationOfflineAlarmWidget->setFixedSize(360, 120);
        m_agvStationOfflineAlarmWidget->setStyleSheet(
            "#agvStationOfflineAlarmWidget {"
            "  background-color: rgba(45, 0, 0, 232);"
            "  border: 3px solid #ff5555;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
            "  color: #ff5555;"
            "  font-size: 22px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}");
    }

    if (m_agvStationOfflineAlarmLabel) {
        m_agvStationOfflineAlarmLabel->setText("检测到有站掉线");
    }

    positionFloatingPopupTopRight(m_agvStationOfflineAlarmWidget, 60);
    m_agvStationOfflineAlarmWidget->show();
    m_agvStationOfflineAlarmWidget->raise();
}

void MainWindow::showParkingSwitchHintDialog(const QString &message)
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_parkingSwitchHintDialog) {
        m_parkingSwitchHintDialog = new QDialog(this);
        m_parkingSwitchHintDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_parkingSwitchHintDialog->setWindowModality(Qt::ApplicationModal);
        m_parkingSwitchHintDialog->setModal(true);
        m_parkingSwitchHintDialog->setObjectName("parkingSwitchHintDialog");

        auto *layout = new QVBoxLayout(m_parkingSwitchHintDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_parkingSwitchHintLabel = new QLabel(m_parkingSwitchHintDialog);
        m_parkingSwitchHintLabel->setObjectName("parkingSwitchHintLabel");
        m_parkingSwitchHintLabel->setAlignment(Qt::AlignCenter);
        m_parkingSwitchHintLabel->setWordWrap(true);
        layout->addWidget(m_parkingSwitchHintLabel);

        m_parkingSwitchHintDialog->setFixedSize(360, 120);
        m_parkingSwitchHintDialog->setStyleSheet(
            "#parkingSwitchHintDialog {"
            "  background-color: rgba(30, 0, 0, 230);"
            "  border: 3px solid #FFFF00;"
            "  border-radius: 10px;"
            "}"
            "#parkingSwitchHintLabel {"
            "  color: #FFFF00;"
            "  font-size: 20px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}");
    }

    if (m_parkingSwitchHintLabel) {
        m_parkingSwitchHintLabel->setText(message);
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    const QRect screenGeometry = screen->availableGeometry();
    const int x = screenGeometry.width() - m_parkingSwitchHintDialog->width() - 40;
    const int y = 820;
    m_parkingSwitchHintDialog->move(x, y);
    m_parkingSwitchHintDialog->show();
    m_parkingSwitchHintDialog->raise();
    m_parkingSwitchHintDialog->activateWindow();
}

void MainWindow::hideParkingSwitchHintDialog()
{
    if (m_parkingSwitchHintDialog && m_parkingSwitchHintDialog->isVisible()) {
        m_parkingSwitchHintDialog->hide();
    }
}

bool MainWindow::isLegOpenPathCheckActive() const
{
    return m_legOpenPathCheckDialog && m_legOpenPathCheckDialog->isVisible();
}

void MainWindow::showLegOpenPathCheckDialog(int legLengthMm)
{
    if (!userPopupsAllowed()) {
        return;
    }

    // 已在检查中：仅抬升窗口，不重置倒计时，避免重复点击干扰检查过程。
    if (isLegOpenPathCheckActive()) {
        m_legOpenPathCheckLengthMm = legLengthMm;
        m_legOpenPathCheckDialog->raise();
        m_legOpenPathCheckDialog->activateWindow();
        return;
    }

    m_legOpenPathCheckLengthMm = legLengthMm;

    // 与支腿异常操作窗、驻车切换等待窗互斥，避免同区域叠加。
    hideParkingLegAbnormalDialog();
    hideParkingSwitchHintDialog();

    if (!m_legOpenPathCheckDialog) {
        m_legOpenPathCheckDialog = new QDialog(this);
        m_legOpenPathCheckDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_legOpenPathCheckDialog->setWindowModality(Qt::ApplicationModal);
        m_legOpenPathCheckDialog->setModal(true);
        m_legOpenPathCheckDialog->setObjectName(QStringLiteral("legOpenPathCheckDialog"));

        auto *layout = new QVBoxLayout(m_legOpenPathCheckDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        m_legOpenPathCheckMessageLabel = new QLabel(
            QStringLiteral("请绕车检查支腿伸出路径中有无干涉"), m_legOpenPathCheckDialog);
        m_legOpenPathCheckMessageLabel->setObjectName(QStringLiteral("legOpenPathCheckMessageLabel"));
        m_legOpenPathCheckMessageLabel->setAlignment(Qt::AlignCenter);
        m_legOpenPathCheckMessageLabel->setWordWrap(true);
        layout->addWidget(m_legOpenPathCheckMessageLabel);

        m_legOpenPathCheckCountdownLabel = new QLabel(m_legOpenPathCheckDialog);
        m_legOpenPathCheckCountdownLabel->setObjectName(QStringLiteral("legOpenPathCheckCountdownLabel"));
        m_legOpenPathCheckCountdownLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_legOpenPathCheckCountdownLabel);

        m_legOpenPathCheckConfirmBtn = new QPushButton(QStringLiteral("确认"), m_legOpenPathCheckDialog);
        m_legOpenPathCheckConfirmBtn->setObjectName(QStringLiteral("legOpenPathCheckConfirmBtn"));
        m_legOpenPathCheckConfirmBtn->setAutoDefault(false);
        m_legOpenPathCheckConfirmBtn->setDefault(false);
        m_legOpenPathCheckConfirmBtn->setVisible(false);
        layout->addWidget(m_legOpenPathCheckConfirmBtn);

        connect(m_legOpenPathCheckConfirmBtn, &QPushButton::clicked, this, [this]() {
            const int pendingLengthMm = m_legOpenPathCheckLengthMm;
            hideLegOpenPathCheckDialog();
            executeAGVParkingSwitch(true, pendingLengthMm);
        });

        m_legOpenPathCheckDialog->setFixedSize(420, 190);
        m_legOpenPathCheckDialog->setStyleSheet(
            "#legOpenPathCheckDialog {"
            "  background-color: rgba(30, 0, 0, 230);"
            "  border: 3px solid #FFFF00;"
            "  border-radius: 10px;"
            "}"
            "#legOpenPathCheckMessageLabel,"
            "#legOpenPathCheckCountdownLabel {"
            "  color: #FFFF00;"
            "  font-size: 18px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}"
            "#legOpenPathCheckConfirmBtn {"
            "  background-color: #FFFF00;"
            "  color: #202020;"
            "  border: 2px solid #FFEE55;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 100px;"
            "}"
            "#legOpenPathCheckConfirmBtn:hover {"
            "  background-color: #FFEE55;"
            "  border-color: #FFFF00;"
            "}");
    }

    if (!m_legOpenPathCheckTimer) {
        m_legOpenPathCheckTimer = new QTimer(this);
        m_legOpenPathCheckTimer->setObjectName(QStringLiteral("legOpenPathCheckTimer"));
        m_legOpenPathCheckTimer->setInterval(1000);
        connect(m_legOpenPathCheckTimer, &QTimer::timeout, this, [this]() {
            if (!isLegOpenPathCheckActive()) {
                m_legOpenPathCheckTimer->stop();
                return;
            }
            if (m_legOpenPathCheckRemainSec > 0) {
                --m_legOpenPathCheckRemainSec;
            }
            if (m_legOpenPathCheckCountdownLabel) {
                m_legOpenPathCheckCountdownLabel->setText(
                    QStringLiteral("剩余时间：%1 秒").arg(m_legOpenPathCheckRemainSec));
            }
            if (m_legOpenPathCheckRemainSec <= 0) {
                m_legOpenPathCheckTimer->stop();
                if (m_legOpenPathCheckConfirmBtn) {
                    m_legOpenPathCheckConfirmBtn->setVisible(true);
                }
                if (m_legOpenPathCheckDialog) {
                    m_legOpenPathCheckDialog->setFixedSize(420, 230);
                }
            }
        });
    }

    m_legOpenPathCheckRemainSec = 30;
    if (m_legOpenPathCheckCountdownLabel) {
        m_legOpenPathCheckCountdownLabel->setText(
            QStringLiteral("剩余时间：%1 秒").arg(m_legOpenPathCheckRemainSec));
    }
    if (m_legOpenPathCheckConfirmBtn) {
        m_legOpenPathCheckConfirmBtn->setVisible(false);
    }
    m_legOpenPathCheckDialog->setFixedSize(420, 190);

    // 与底盘模式切换提示同风格：右下区域；y 与驻车切换提示错开，避免瞬时叠影。
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    const QRect screenGeometry = screen->availableGeometry();
    const int x = screenGeometry.width() - m_legOpenPathCheckDialog->width() - 40;
    const int y = 700;
    m_legOpenPathCheckDialog->move(x, y);
    m_legOpenPathCheckDialog->show();
    m_legOpenPathCheckDialog->raise();
    m_legOpenPathCheckDialog->activateWindow();
    m_legOpenPathCheckTimer->start();
}

void MainWindow::hideLegOpenPathCheckDialog()
{
    if (m_legOpenPathCheckTimer) {
        m_legOpenPathCheckTimer->stop();
    }
    m_legOpenPathCheckRemainSec = 0;
    m_legOpenPathCheckLengthMm = -1;
    if (m_legOpenPathCheckConfirmBtn) {
        m_legOpenPathCheckConfirmBtn->setVisible(false);
    }
    if (m_legOpenPathCheckDialog && m_legOpenPathCheckDialog->isVisible()) {
        m_legOpenPathCheckDialog->hide();
    }
}

bool MainWindow::isEstimatedWeightEmpty() const
{
    return !ui || !ui->LEdit_AGV_EstimatedWeight
           || ui->LEdit_AGV_EstimatedWeight->text().trimmed().isEmpty();
}

// 仅 onAGVParkBtnClicked 调用；支腿异常弹窗开/关驻车豁免预计负载检查
void MainWindow::showExpectedLoadEmptyDialog()
{
    showToast(QStringLiteral("预计负载不能为空"), ToastKind::Warning);
}

void MainWindow::updateParkingLegAbnormalDialogVisibility()
{
    // 驻车切换等待中、绕车检查倒计时中：不弹支腿异常窗，避免三窗互抢。
    if (m_agvLegAbnormal51Bit7Flag
        && !property("parkingSwitchWaiting").toBool()
        && !isLegOpenPathCheckActive()) {
        if (m_parkingLegAbnormalDialog && m_parkingLegAbnormalDialog->isVisible()) {
            return;
        }
        QTimer::singleShot(0, this, [this]() {
            if (!m_agvLegAbnormal51Bit7Flag
                || property("parkingSwitchWaiting").toBool()
                || isLegOpenPathCheckActive()) {
                return;
            }
            if (m_parkingLegAbnormalDialog && m_parkingLegAbnormalDialog->isVisible()) {
                return;
            }
            showParkingLegAbnormalDialog();
        });
    } else {
        hideParkingLegAbnormalDialog();
    }
}

void MainWindow::showParkingLegAbnormalDialog()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_parkingLegAbnormalDialog) {
        m_parkingLegAbnormalDialog = new QDialog(this);
        m_parkingLegAbnormalDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_parkingLegAbnormalDialog->setWindowModality(Qt::ApplicationModal);
        m_parkingLegAbnormalDialog->setModal(true);
        m_parkingLegAbnormalDialog->setObjectName(QStringLiteral("parkingLegAbnormalDialog"));

        auto *layout = new QVBoxLayout(m_parkingLegAbnormalDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        auto *titleLabel = new QLabel(QStringLiteral("支腿异常"), m_parkingLegAbnormalDialog);
        titleLabel->setObjectName(QStringLiteral("parkingLegAbnormalTitleLabel"));
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);

        auto *lengthLabel = new QLabel(QStringLiteral("支腿长度设置"), m_parkingLegAbnormalDialog);
        lengthLabel->setObjectName(QStringLiteral("parkingLegAbnormalLengthLabel"));
        lengthLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(lengthLabel);

        m_parkingLegAbnormalLengthEdit = new QLineEdit(m_parkingLegAbnormalDialog);
        m_parkingLegAbnormalLengthEdit->setObjectName(QStringLiteral("LEdit_AGV_ParkOutTriggerLenght"));
        if (m_virtualKeyboard) {
            m_parkingLegAbnormalLengthEdit->installEventFilter(this);
        }
        layout->addWidget(m_parkingLegAbnormalLengthEdit);

        auto *enableBtn = new QPushButton(QStringLiteral("驻车开启"), m_parkingLegAbnormalDialog);
        enableBtn->setObjectName(QStringLiteral("parkingLegAbnormalEnableBtn"));
        enableBtn->setAutoDefault(false);
        enableBtn->setDefault(false);
        layout->addWidget(enableBtn);

        auto *disableBtn = new QPushButton(QStringLiteral("驻车关闭"), m_parkingLegAbnormalDialog);
        disableBtn->setObjectName(QStringLiteral("parkingLegAbnormalDisableBtn"));
        disableBtn->setAutoDefault(false);
        disableBtn->setDefault(false);
        layout->addWidget(disableBtn);

        connect(m_parkingLegAbnormalLengthEdit, &QLineEdit::editingFinished, this, [this]() {
            if (!m_parkingLegAbnormalLengthEdit) {
                return;
            }
            const QPair<int, int> lim = parkOutTriggerLengthLimitsFromSettings();
            bool ok = false;
            int v = m_parkingLegAbnormalLengthEdit->text().trimmed().toInt(&ok);
            if (!ok) {
                v = 1100;
            }
            const int clampedMm = qBound(lim.first, v, lim.second);
            if (clampedMm != v || !ok) {
                m_parkingLegAbnormalLengthEdit->setText(QString::number(clampedMm));
            }
        });

        connect(enableBtn, &QPushButton::clicked, this, [this]() {
            if (!m_parkingLegAbnormalLengthEdit) {
                return;
            }
            const QPair<int, int> lim = parkOutTriggerLengthLimitsFromSettings();
            bool ok = false;
            int v = m_parkingLegAbnormalLengthEdit->text().trimmed().toInt(&ok);
            if (!ok) {
                v = 1100;
            }
            const int clampedMm = qBound(lim.first, v, lim.second);
            m_parkingLegAbnormalLengthEdit->setText(QString::number(clampedMm));
            hideParkingLegAbnormalDialog();
            showLegOpenPathCheckDialog(clampedMm);
        });

        connect(disableBtn, &QPushButton::clicked, this, [this]() {
            hideParkingLegAbnormalDialog();
            if (isLegOpenPathCheckActive()) {
                hideLegOpenPathCheckDialog();
            }
            executeAGVParkingSwitch(false);
        });

        m_parkingLegAbnormalDialog->setFixedSize(360, 280);
        m_parkingLegAbnormalDialog->setStyleSheet(
            "#parkingLegAbnormalDialog {"
            "  background-color: rgba(30, 0, 0, 230);"
            "  border: 3px solid #FF6600;"
            "  border-radius: 10px;"
            "}"
            "#parkingLegAbnormalTitleLabel,"
            "#parkingLegAbnormalLengthLabel {"
            "  color: #FF6600;"
            "  font-size: 18px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}"
            "#LEdit_AGV_ParkOutTriggerLenght {"
            "  color: #FFFFFF;"
            "  background-color: rgba(0, 0, 0, 120);"
            "  border: 2px solid #FF6600;"
            "  border-radius: 6px;"
            "  padding: 6px;"
            "  font-size: 16px;"
            "}"
            "QPushButton {"
            "  background-color: #FF6600;"
            "  color: #202020;"
            "  border: 2px solid #FF9933;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #FF9933;"
            "  border-color: #FF6600;"
            "}");
    }

    const QPair<int, int> lim = parkOutTriggerLengthLimitsFromSettings();
    if (!m_parkOutTriggerLengthValidator) {
        m_parkOutTriggerLengthValidator = new QIntValidator(this);
    }
    m_parkOutTriggerLengthValidator->setRange(lim.first, lim.second);
    if (m_parkingLegAbnormalLengthEdit) {
        m_parkingLegAbnormalLengthEdit->setValidator(m_parkOutTriggerLengthValidator);
        if (m_parkingLegAbnormalLengthEdit->text().trimmed().isEmpty()) {
            m_parkingLegAbnormalLengthEdit->setText(QStringLiteral("1100"));
        }
        applyParkOutTriggerLengthRuntimeSettings();
    }

    positionFloatingPopupTopRight(m_parkingLegAbnormalDialog, 500);
    m_parkingLegAbnormalDialog->show();
    m_parkingLegAbnormalDialog->raise();
    m_parkingLegAbnormalDialog->activateWindow();
}

void MainWindow::hideParkingLegAbnormalDialog()
{
    if (m_parkingLegAbnormalDialog && m_parkingLegAbnormalDialog->isVisible()) {
        m_parkingLegAbnormalDialog->hide();
    }
}

void MainWindow::hideAgvStationOfflineAlarm()
{
    if (m_agvStationOfflineAlarmWidget && m_agvStationOfflineAlarmWidget->isVisible()) {
        m_agvStationOfflineAlarmWidget->hide();
    }
}

void MainWindow::showAgvDriveFaultAlarm()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_agvDriveFaultAlarmWidget) {
        m_agvDriveFaultAlarmWidget = new QWidget(nullptr);
        m_agvDriveFaultAlarmWidget->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                                   Qt::WindowStaysOnTopHint);
        m_agvDriveFaultAlarmWidget->setWindowModality(Qt::ApplicationModal);
        m_agvDriveFaultAlarmWidget->setObjectName("agvDriveFaultAlarmWidget");

        QVBoxLayout *layout = new QVBoxLayout(m_agvDriveFaultAlarmWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_agvDriveFaultAlarmLabel = new QLabel(m_agvDriveFaultAlarmWidget);
        m_agvDriveFaultAlarmLabel->setAlignment(Qt::AlignCenter);
        m_agvDriveFaultAlarmLabel->setWordWrap(true);
        m_agvDriveFaultAlarmLabel->setText("检测到驱动故障");
        layout->addWidget(m_agvDriveFaultAlarmLabel);

        m_agvDriveFaultAlarmWidget->setFixedSize(360, 120);
        m_agvDriveFaultAlarmWidget->setStyleSheet(
            "#agvDriveFaultAlarmWidget {"
            "  background-color: rgba(45, 10, 0, 232);"
            "  border: 3px solid #ff7f50;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
            "  color: #ff7f50;"
            "  font-size: 22px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}");
    }

    if (m_agvDriveFaultAlarmLabel) {
        m_agvDriveFaultAlarmLabel->setText("检测到驱动故障");
    }

    positionFloatingPopupTopRight(m_agvDriveFaultAlarmWidget, 200);
    m_agvDriveFaultAlarmWidget->show();
    m_agvDriveFaultAlarmWidget->raise();
}

void MainWindow::hideAgvDriveFaultAlarm()
{
    if (m_agvDriveFaultAlarmWidget && m_agvDriveFaultAlarmWidget->isVisible()) {
        m_agvDriveFaultAlarmWidget->hide();
    }
}

void MainWindow::showAgvBatteryLowDialog()
{
    constexpr qint64 kBatteryLowToastIntervalMs = 30000;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastBatteryLowToastMs > 0 && (nowMs - m_lastBatteryLowToastMs) < kBatteryLowToastIntervalMs) {
        return;
    }

    m_lastBatteryLowToastMs = nowMs;
    showToast(QStringLiteral("电池电量低，请充电"), ToastKind::Warning, 5000);
}

void MainWindow::hideAgvBatteryLowDialog()
{
    if (m_agvBatteryLowDialog && m_agvBatteryLowDialog->isVisible()) {
        m_agvBatteryLowDialog->hide();
    }
}

void MainWindow::showInclinometerTiltRiskDialog()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (m_inclinometerTiltRiskAcked) {
        return;
    }
    if (m_inclinometerTiltRiskDialog && m_inclinometerTiltRiskDialog->isVisible()) {
        return;
    }

    if (!m_inclinometerTiltRiskDialog) {
        m_inclinometerTiltRiskDialog = new QDialog(this);
        m_inclinometerTiltRiskDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_inclinometerTiltRiskDialog->setWindowModality(Qt::ApplicationModal);
        m_inclinometerTiltRiskDialog->setModal(true);
        m_inclinometerTiltRiskDialog->setObjectName(QStringLiteral("inclinometerTiltRiskDialog"));

        auto *layout = new QVBoxLayout(m_inclinometerTiltRiskDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        auto *msgLabel = new QLabel(QStringLiteral("倾覆风险提示！设备倾角过大。"), m_inclinometerTiltRiskDialog);
        msgLabel->setObjectName(QStringLiteral("inclinometerTiltRiskLabel"));
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);

        auto *confirmBtn = new QPushButton(QStringLiteral("确认"), m_inclinometerTiltRiskDialog);
        layout->addWidget(confirmBtn, 0, Qt::AlignCenter);
        connect(confirmBtn, &QPushButton::clicked, this, [this]() {
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = QStringLiteral("报警系统");
                record.controlName = QStringLiteral("倾覆风险提示");
                record.controlType = QStringLiteral("提示窗口");
                record.operation = QStringLiteral("用户确认");
                record.oldValue = QStringLiteral("倾覆风险提示！设备倾角过大。");
                record.newValue = QStringLiteral("用户点击确认，倾覆风险提示窗口隐藏");
                m_recorder->addRecord(record);
            }
            m_inclinometerTiltRiskAcked = true;
            hideInclinometerTiltRiskDialog();
        });

        m_inclinometerTiltRiskDialog->setFixedSize(400, 150);
        m_inclinometerTiltRiskDialog->setStyleSheet(
            QStringLiteral(
                "#inclinometerTiltRiskDialog {"
                "  background-color: rgba(50, 35, 0, 235);"
                "  border: 3px solid #ffb000;"
                "  border-radius: 10px;"
                "}"
                "#inclinometerTiltRiskLabel {"
                "  color: #ffcc33;"
                "  font-size: 20px;"
                "  font-weight: bold;"
                "  background-color: transparent;"
                "}"
                "QPushButton {"
                "  background-color: #ffb000;"
                "  color: #1f1f1f;"
                "  border: 2px solid #ffd166;"
                "  border-radius: 6px;"
                "  padding: 8px 16px;"
                "  font-size: 14px;"
                "  font-weight: bold;"
                "  min-width: 90px;"
                "}"
                "QPushButton:hover {"
                "  background-color: #ffd166;"
                "}"));
    }

    positionFloatingPopupTopRight(m_inclinometerTiltRiskDialog, 1140);
    m_inclinometerTiltRiskDialog->show();
    m_inclinometerTiltRiskDialog->raise();
    m_inclinometerTiltRiskDialog->activateWindow();
}

void MainWindow::hideInclinometerTiltRiskDialog()
{
    if (m_inclinometerTiltRiskDialog && m_inclinometerTiltRiskDialog->isVisible()) {
        m_inclinometerTiltRiskDialog->hide();
    }
}

void MainWindow::presentInclinometerTiltLockUnlocked()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_inclinometerTiltLockDialog) {
        return;
    }

    m_inclinometerTiltLockDialog->hide();

    if (m_inclinometerTiltLockPasswordHint) {
        m_inclinometerTiltLockPasswordHint->hide();
    }
    if (m_inclinometerTiltLockPasswordEdit) {
        m_inclinometerTiltLockPasswordEdit->hide();
    }
    if (m_inclinometerTiltLockErrorLabel) {
        m_inclinometerTiltLockErrorLabel->hide();
    }
    if (m_inclinometerTiltLockConfirmBtn) {
        m_inclinometerTiltLockConfirmBtn->hide();
    }

    m_inclinometerTiltLockDialog->setWindowModality(Qt::ApplicationModal);
    m_inclinometerTiltLockDialog->setModal(true);
    m_inclinometerTiltLockDialog->setFixedSize(420, 120);
    positionFloatingPopupBottomRight(m_inclinometerTiltLockDialog, 50, m_alarmWidget);
    m_inclinometerTiltLockDialog->show();
    m_inclinometerTiltLockDialog->raise();
}

void MainWindow::presentInclinometerTiltLockModal()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_inclinometerTiltLockDialog) {
        return;
    }

    m_inclinometerTiltLockDialog->hide();

    if (m_inclinometerTiltLockPasswordHint) {
        m_inclinometerTiltLockPasswordHint->show();
    }
    if (m_inclinometerTiltLockPasswordEdit) {
        m_inclinometerTiltLockPasswordEdit->clear();
        m_inclinometerTiltLockPasswordEdit->show();
    }
    if (m_inclinometerTiltLockErrorLabel) {
        m_inclinometerTiltLockErrorLabel->hide();
    }
    if (m_inclinometerTiltLockConfirmBtn) {
        m_inclinometerTiltLockConfirmBtn->show();
    }

    m_inclinometerTiltLockDialog->setWindowModality(Qt::ApplicationModal);
    m_inclinometerTiltLockDialog->setModal(true);
    m_inclinometerTiltLockDialog->setFixedSize(420, 240);
    positionFloatingPopupCenter(m_inclinometerTiltLockDialog);
    m_inclinometerTiltLockDialog->show();
    m_inclinometerTiltLockDialog->raise();
    m_inclinometerTiltLockDialog->activateWindow();
    if (m_inclinometerTiltLockPasswordEdit) {
        m_inclinometerTiltLockPasswordEdit->setFocus();
    }
}

void MainWindow::showInclinometerTiltLockDialog()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_inclinometerTiltLockDialog) {
        m_inclinometerTiltLockDialog = new QDialog(nullptr);
        m_inclinometerTiltLockDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                                     Qt::WindowStaysOnTopHint);
        m_inclinometerTiltLockDialog->setWindowModality(Qt::ApplicationModal);
        m_inclinometerTiltLockDialog->setModal(true);
        m_inclinometerTiltLockDialog->setObjectName(QStringLiteral("inclinometerTiltLockDialog"));

        auto *layout = new QVBoxLayout(m_inclinometerTiltLockDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        auto *msgLabel = new QLabel(QStringLiteral("高倾覆风险报警！！！设备倾角过大锁定。"),
                                    m_inclinometerTiltLockDialog);
        msgLabel->setObjectName(QStringLiteral("inclinometerTiltLockLabel"));
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);

        m_inclinometerTiltLockPasswordHint = new QLabel(QStringLiteral("请输入管理员密码"),
                                                        m_inclinometerTiltLockDialog);
        m_inclinometerTiltLockPasswordHint->setObjectName(QStringLiteral("inclinometerTiltLockPasswordHint"));
        m_inclinometerTiltLockPasswordHint->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_inclinometerTiltLockPasswordHint);

        m_inclinometerTiltLockPasswordEdit = new QLineEdit(m_inclinometerTiltLockDialog);
        m_inclinometerTiltLockPasswordEdit->setObjectName(QStringLiteral("inclinometerTiltLockPasswordEdit"));
        m_inclinometerTiltLockPasswordEdit->setEchoMode(QLineEdit::Password);
        m_inclinometerTiltLockPasswordEdit->setAlignment(Qt::AlignCenter);
        m_inclinometerTiltLockPasswordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
        if (m_virtualKeyboard) {
            m_inclinometerTiltLockPasswordEdit->installEventFilter(this);
        }
        layout->addWidget(m_inclinometerTiltLockPasswordEdit);

        m_inclinometerTiltLockErrorLabel = new QLabel(m_inclinometerTiltLockDialog);
        m_inclinometerTiltLockErrorLabel->setObjectName(QStringLiteral("inclinometerTiltLockErrorLabel"));
        m_inclinometerTiltLockErrorLabel->setAlignment(Qt::AlignCenter);
        m_inclinometerTiltLockErrorLabel->setStyleSheet(QStringLiteral("color: #ff8888; font-size: 13px;"));
        m_inclinometerTiltLockErrorLabel->hide();
        layout->addWidget(m_inclinometerTiltLockErrorLabel);

        m_inclinometerTiltLockConfirmBtn = new QPushButton(QStringLiteral("确认"), m_inclinometerTiltLockDialog);
        m_inclinometerTiltLockConfirmBtn->setObjectName(QStringLiteral("inclinometerTiltLockConfirmBtn"));
        layout->addWidget(m_inclinometerTiltLockConfirmBtn, 0, Qt::AlignCenter);

        connect(m_inclinometerTiltLockConfirmBtn, &QPushButton::clicked, this, [this]() {
            if (!m_inclinometerTiltLockPasswordEdit) {
                return;
            }
            const QString password = m_inclinometerTiltLockPasswordEdit->text();
            if (password != QStringLiteral("123")) {
                if (m_inclinometerTiltLockErrorLabel) {
                    m_inclinometerTiltLockErrorLabel->setText(QStringLiteral("密码错误，请重新输入"));
                    m_inclinometerTiltLockErrorLabel->show();
                }
                m_inclinometerTiltLockPasswordEdit->clear();
                m_inclinometerTiltLockPasswordEdit->setFocus();
                return;
            }

            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = QStringLiteral("报警系统");
                record.controlName = QStringLiteral("高倾覆风险锁定");
                record.controlType = QStringLiteral("提示窗口");
                record.operation = QStringLiteral("用户确认");
                record.oldValue = QStringLiteral("高倾覆风险报警！！！设备倾角过大锁定。");
                record.newValue = QStringLiteral("管理员密码验证通过，锁定窗转为非模态并移至右下角");
                m_recorder->addRecord(record);
            }

            m_inclinometerTiltLockUnlocked = true;
            presentInclinometerTiltLockUnlocked();
        });

        connect(m_inclinometerTiltLockPasswordEdit, &QLineEdit::returnPressed,
                m_inclinometerTiltLockConfirmBtn, &QPushButton::click);
        connect(m_inclinometerTiltLockPasswordEdit, &QLineEdit::textChanged, this, [this]() {
            if (m_inclinometerTiltLockErrorLabel) {
                m_inclinometerTiltLockErrorLabel->hide();
            }
        });

        m_inclinometerTiltLockDialog->setFixedSize(420, 240);
        m_inclinometerTiltLockDialog->setStyleSheet(
            QStringLiteral(
                "#inclinometerTiltLockDialog {"
                "  background-color: rgba(45, 0, 0, 235);"
                "  border: 3px solid #ff5555;"
                "  border-radius: 10px;"
                "}"
                "#inclinometerTiltLockLabel {"
                "  color: #ff8888;"
                "  font-size: 20px;"
                "  font-weight: bold;"
                "  background-color: transparent;"
                "}"
                "#inclinometerTiltLockPasswordHint {"
                "  color: #ffcccc;"
                "  font-size: 14px;"
                "  background-color: transparent;"
                "}"
                "#inclinometerTiltLockPasswordEdit {"
                "  background-color: rgba(20, 0, 0, 200);"
                "  color: #ffffff;"
                "  border: 2px solid #ff8888;"
                "  border-radius: 6px;"
                "  padding: 6px;"
                "  font-size: 14px;"
                "}"
                "#inclinometerTiltLockConfirmBtn {"
                "  background-color: #ff5555;"
                "  color: #ffffff;"
                "  border: 2px solid #ff8888;"
                "  border-radius: 6px;"
                "  padding: 8px 16px;"
                "  font-size: 14px;"
                "  font-weight: bold;"
                "  min-width: 90px;"
                "}"
                "#inclinometerTiltLockConfirmBtn:hover {"
                "  background-color: #ff8888;"
                "}"));
    }

    if (m_inclinometerTiltLockUnlocked) {
        presentInclinometerTiltLockUnlocked();
        return;
    }

    if (m_inclinometerTiltLockDialog->isVisible() && m_inclinometerTiltLockDialog->isModal()) {
        return;
    }

    presentInclinometerTiltLockModal();
}

void MainWindow::hideInclinometerTiltLockDialog()
{
    if (m_inclinometerTiltLockDialog && m_inclinometerTiltLockDialog->isVisible()) {
        m_inclinometerTiltLockDialog->hide();
    }
}

void MainWindow::showRobotOperationHintDialog(const QString &message)
{
    static const QString kHeightInterlockText =
        QStringLiteral("重心偏高安全风险警告！！！请将立柱高度调整至1000mm以内。");
    static const QString kLengthInterlockText =
        QStringLiteral("高倾覆风险报警！！！请将伸缩臂长度调整至1000mm以内。");
    if (message == kHeightInterlockText || message == kLengthInterlockText) {
        showRobotInterlockModalDialog(message);
        return;
    }

    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "提示系统";
        record.controlName = "外部按键操作提示";
        record.controlType = "提示窗口";
        record.operation = "提示触发";
        record.oldValue = "";
        record.newValue = message;
        m_recorder->addRecord(record);
    }

    showToast(message, ToastKind::Warning);
}

void MainWindow::hideRobotOperationHintDialog()
{
    if (m_robotInterlockDialog && m_robotInterlockDialog->isVisible()) {
        m_robotInterlockDialog->done(QDialog::Rejected);
    }
    dismissToastByMessage(QStringLiteral("重心偏高安全风险警告！！！请将立柱高度调整至1000mm以内。"));
    dismissToastByMessage(QStringLiteral("高倾覆风险报警！！！请将伸缩臂长度调整至1000mm以内。"));
}

void MainWindow::showRobotInterlockModalDialog(const QString &message)
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_robotInterlockDialog) {
        m_robotInterlockDialog = new QDialog(this);
        m_robotInterlockDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_robotInterlockDialog->setWindowModality(Qt::ApplicationModal);
        m_robotInterlockDialog->setModal(true);
        m_robotInterlockDialog->setObjectName(QStringLiteral("robotInterlockDialog"));

        auto *layout = new QVBoxLayout(m_robotInterlockDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        m_robotInterlockLabel = new QLabel(m_robotInterlockDialog);
        m_robotInterlockLabel->setObjectName(QStringLiteral("robotInterlockLabel"));
        m_robotInterlockLabel->setAlignment(Qt::AlignCenter);
        m_robotInterlockLabel->setWordWrap(true);
        layout->addWidget(m_robotInterlockLabel);

        auto *confirmBtn = new QPushButton(QStringLiteral("确认"), m_robotInterlockDialog);
        layout->addWidget(confirmBtn);
        connect(confirmBtn, &QPushButton::clicked, m_robotInterlockDialog, &QDialog::accept);

        m_robotInterlockDialog->setFixedSize(480, 160);
        m_robotInterlockDialog->setStyleSheet(
            "#robotInterlockDialog {"
            "  background-color: rgba(30, 0, 0, 230);"
            "  border: 3px solid #FFFF00;"
            "  border-radius: 10px;"
            "}"
            "#robotInterlockLabel {"
            "  color: #FFFF00;"
            "  font-size: 20px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}"
            "QPushButton {"
            "  background-color: #FFFF00;"
            "  color: #202020;"
            "  border: 2px solid #FFD65A;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #FFD65A;"
            "  border-color: #FFFF00;"
            "}");
    }

    if (m_robotInterlockLabel) {
        m_robotInterlockLabel->setText(message);
    }

    positionFloatingPopupTopRight(m_robotInterlockDialog, 660);
    m_robotInterlockDialog->exec();
}

void MainWindow::maybeShowZeroSpeedHintForHomePageExternalKey(int keyNumber, bool pressed)
{
    if (!pressed) {
        return;
    }
    if (!ui || !ui->TBtn_HomePage || !ui->TBtn_HomePage->isChecked()) {
        return;
    }
    if (!ui->StackedWidget) {
        return;
    }
    const QWidget *currentPageWidget = ui->StackedWidget->currentWidget();
    if (!currentPageWidget
        || currentPageWidget->objectName() != QStringLiteral("page_Robot")) {
        return;
    }
    if (m_stepModeUnknown) {
        return;
    }

    bool speedIsZero = false;
    if (keyNumber >= 1 && keyNumber <= 8) {
        speedIsZero = (getSliderEditValue(QStringLiteral("TechSliderEdit_Robot_RobotSpeed")) <= 0.0);
    } else if (keyNumber == 9 || keyNumber == 10) {
        const double agvSpeed = m_editAGV_MoveSpeed
                                      ? m_editAGV_MoveSpeed->value()
                                      : getSliderEditValue(QStringLiteral("SEdit_AGV_MoveSpeed"));
        speedIsZero = (agvSpeed <= 0.0);
    } else {
        return;
    }

    if (!speedIsZero) {
        return;
    }

    if (keyNumber >= 1 && keyNumber <= 8) {
        showZeroSpeedOperationHintDialog(kRobotZeroSpeedHintText,
                                         kRobotZeroSpeedHistoryText);
    } else {
        showZeroSpeedOperationHintDialog(kAgvZeroSpeedHintText,
                                         kAgvZeroSpeedHistoryText);
    }
}

void MainWindow::showZeroSpeedOperationHintDialog(const QString &hintText,
                                                  const QString &historyText)
{
    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("速度为0操作提示");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("提示触发");
        record.oldValue = QString();
        record.newValue = historyText;
        m_recorder->addRecord(record);
    }

    showToast(hintText, ToastKind::Warning);
}

void MainWindow::hideZeroSpeedOperationHintDialog()
{
    dismissToastByMessage(kRobotZeroSpeedHintText);
    dismissToastByMessage(kAgvZeroSpeedHintText);
}

namespace {

const QString kUnselectedStepModeHintText =
    QStringLiteral("未选择步进或者点动模式，将自动选择点动模式");
const QString kUnselectedMoveModeHintText =
    QStringLiteral("未选择坐标或者关节模式，将自动选择关节模式");

bool stepEditValueIsEmptyOrZero(const QLineEdit *edit)
{
    if (!edit) {
        return true;
    }
    const QString text = edit->text().trimmed();
    if (text.isEmpty()) {
        return true;
    }
    bool ok = false;
    const double value = text.toDouble(&ok);
    return !ok || qFuzzyIsNull(value);
}

} // namespace

void MainWindow::maybeShowUnconfiguredStepValueHintForExternalKey(int keyNumber, bool pressed)
{
    if (!pressed) {
        return;
    }
    if (m_stepModeUnknown || !m_stepModeEnabled) {
        return;
    }
    if (!ui || !ui->StackedWidget) {
        return;
    }

    const QWidget *currentPageWidget = ui->StackedWidget->currentWidget();
    if (!currentPageWidget) {
        return;
    }

    const QLineEdit *stepEdit = nullptr;
    const QString pageObjectName = currentPageWidget->objectName();
    if (pageObjectName == QStringLiteral("page_Robot")) {
        if (!ui->TBtn_HomePage || !ui->TBtn_HomePage->isChecked()) {
            return;
        }
        if (keyNumber < 1 || keyNumber > 10) {
            return;
        }
        stepEdit = m_stepValueEdit;
    } else if (pageObjectName == QStringLiteral("page_SixAxies")) {
        if (keyNumber < 1 || keyNumber > 12) {
            return;
        }
        stepEdit = findChild<QLineEdit*>(QStringLiteral("lineEdit_SixAxies_StepValue"));
    } else {
        return;
    }

    if (!stepEditValueIsEmptyOrZero(stepEdit)) {
        return;
    }

    showUnconfiguredStepValueHintDialog();
}

void MainWindow::showUnconfiguredStepValueHintDialog()
{
    static const QString kHintText = QStringLiteral("当前未设置步进值");
    static const QString kHistoryText = QStringLiteral("操作者在未设置步进值时操作");

    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("步进值未设置操作提示");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("提示触发");
        record.oldValue = QString();
        record.newValue = kHistoryText;
        m_recorder->addRecord(record);
    }

    showToast(kHintText, ToastKind::Warning);
}

void MainWindow::hideUnconfiguredStepValueHintDialog()
{
    dismissToastByMessage(QStringLiteral("当前未设置步进值"));
}

void MainWindow::showStepTargetMismatchHintDialog(int keyNumber, const QString &selectedTargetName)
{
    static const QString kHintText = QStringLiteral("外部按键与当前选中的步进目标不匹配");

    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("步进目标不匹配提示");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("提示触发");
        record.oldValue = QString();
        record.newValue = QStringLiteral("操作者按下按键○%1，与当前步进目标%2不匹配")
                              .arg(keyNumber)
                              .arg(selectedTargetName);
        m_recorder->addRecord(record);
    }

    showToast(kHintText, ToastKind::Warning);
}

void MainWindow::hideStepTargetMismatchHintDialog()
{
    dismissToastByMessage(QStringLiteral("外部按键与当前选中的步进目标不匹配"));
}

void MainWindow::applyDefaultJogStepModeFromExternalKey()
{
    m_stepModeUnknown = false;
    m_stepModeEnabled = false;

    if (ui && ui->TBtn_Stepmove) {
        ui->TBtn_Stepmove->setText(QStringLiteral("点动模式"));
        ui->TBtn_Stepmove->setToolTip(QStringLiteral("当前模式：点动模式"));
    }

    QLabel *runModeLabel = ui && ui->statusBar
                               ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarRunModeLabel"))
                               : nullptr;
    if (runModeLabel) {
        runModeLabel->setText(QStringLiteral("点动模式"));
        runModeLabel->setStyleSheet(QStringLiteral("color: #00ccff; font-weight: bold; font-size: 11px;"));
    }

    if (ui && ui->StackedWidget) {
        const int currentPage = ui->StackedWidget->currentIndex();
        if (currentPage == 0) {
            writeToMainDevice(501, 1);
            qCDebug(lcMainWindow) << "外部按键自动选择：首页切换到点动模式，地址501写入1";
        } else if (currentPage == 3) {
            writeToMainDevice(600, 1);
            qCDebug(lcMainWindow) << "外部按键自动选择：第四页切换到点动模式，地址600写入1";
        }
    }

    updateFunctionSwitchVisuals();
    updateStepTargetButtonsState();
}

void MainWindow::applyDefaultJointMoveModeFromExternalKey()
{
    m_moveModeUnknown = false;
    m_isJointMode = true;

    const ButtonModbusMapping::Binding moveBinding = buttonModbusBinding(QStringLiteral("TBtn_MoveMode"));
    const ModbusRegisterSpec writeSpec = moveBinding.writes.isEmpty()
        ? ModbusRegisterSpec{}
        : moveBinding.writes.first();
    const int writeAddr = ButtonModbusMapping::addressOr(writeSpec, 525);
    const int jointValue = ButtonModbusMapping::stateValueOr(writeSpec, 1, 2);
    if (writeSpec.device == QStringLiteral("AGV")) {
        writeToAGVDevice(writeAddr, jointValue, true);
    } else {
        writeToMainDevice(writeAddr, jointValue);
    }

    if (ui && ui->TBtn_MoveMode) {
        ui->TBtn_MoveMode->setText(QStringLiteral("关节模式"));
    }

    QLabel *moveModeLabel = ui && ui->statusBar
                                ? ui->statusBar->findChild<QLabel*>(QStringLiteral("statusBarMoveModeLabel"))
                                : nullptr;
    if (moveModeLabel) {
        moveModeLabel->setText(QStringLiteral("关节模式"));
        moveModeLabel->setStyleSheet(QStringLiteral("color: #55ff55; font-weight: bold; font-size: 11px;"));
    }

    updateFunctionSwitchVisuals();
    updateStepTargetButtonsState();
}

void MainWindow::showUnselectedStepModeHintDialog()
{
    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("步进点动未选择提示");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("提示触发");
        record.oldValue = QString();
        record.newValue = kUnselectedStepModeHintText;
        m_recorder->addRecord(record);
    }

    showToast(kUnselectedStepModeHintText, ToastKind::Warning);
}

void MainWindow::hideUnselectedStepModeHintDialog()
{
    dismissToastByMessage(kUnselectedStepModeHintText);
}

bool MainWindow::maybeShowUnselectedStepModeHintForExternalKey(int keyNumber, bool pressed)
{
    if (!pressed || !m_stepModeUnknown) {
        return false;
    }
    if (!ui || !ui->StackedWidget) {
        return false;
    }

    const QWidget *currentPageWidget = ui->StackedWidget->currentWidget();
    if (!currentPageWidget) {
        return false;
    }

    const QString pageObjectName = currentPageWidget->objectName();
    if (pageObjectName == QStringLiteral("page_Robot")) {
        if (keyNumber < 1 || keyNumber > 10) {
            return false;
        }
    } else if (pageObjectName == QStringLiteral("page_SixAxies")) {
        if (keyNumber < 1 || keyNumber > 14) {
            return false;
        }
    } else if (ui->StackedWidget->currentIndex() == 4) {
        if (keyNumber != 13 && keyNumber != 14) {
            return false;
        }
    } else {
        return false;
    }

    applyDefaultJogStepModeFromExternalKey();
    showUnselectedStepModeHintDialog();
    return true;
}

void MainWindow::showUnselectedMoveModeHintDialog()
{
    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("坐标关节未选择提示");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("提示触发");
        record.oldValue = QString();
        record.newValue = kUnselectedMoveModeHintText;
        m_recorder->addRecord(record);
    }

    showToast(kUnselectedMoveModeHintText, ToastKind::Warning);
}

void MainWindow::hideUnselectedMoveModeHintDialog()
{
    dismissToastByMessage(kUnselectedMoveModeHintText);
}

bool MainWindow::maybeShowUnselectedMoveModeHintForExternalKey(int keyNumber, bool pressed)
{
    if (!pressed || !m_moveModeUnknown) {
        return false;
    }
    if (!ui || !ui->StackedWidget) {
        return false;
    }

    const QWidget *currentPageWidget = ui->StackedWidget->currentWidget();
    if (!currentPageWidget) {
        return false;
    }

    const QString pageObjectName = currentPageWidget->objectName();
    if (pageObjectName == QStringLiteral("page_Robot")) {
        if (keyNumber < 1 || keyNumber > 10) {
            return false;
        }
    } else if (pageObjectName == QStringLiteral("page_SixAxies")) {
        if (keyNumber < 1 || keyNumber > 14) {
            return false;
        }
    } else if (ui->StackedWidget->currentIndex() == 4) {
        if (keyNumber != 13 && keyNumber != 14) {
            return false;
        }
    } else {
        return false;
    }

    applyDefaultJointMoveModeFromExternalKey();
    showUnselectedMoveModeHintDialog();
    return true;
}

void MainWindow::showTeachingWriteGateDeniedDialog()
{
    showToast(ModbusWriteGate::teachingGateUserDialogMessage(), ToastKind::Warning);
}

void MainWindow::hideTeachingWriteGateDeniedDialog()
{
    dismissToastByMessage(ModbusWriteGate::teachingGateUserDialogMessage());
}

bool MainWindow::verifyTeachingWriteGateOrShowDialog()
{
    ModbusThreadManager *mgr = m_modbusManager ? m_modbusManager : ModbusThreadManager::instance();
    if (ModbusWriteGate::verifyWriteAllowed(mgr)) {
        return true;
    }
    showTeachingWriteGateDeniedDialog();
    return false;
}

void MainWindow::showWirelessModeWarningDialog()
{
    showToast(kWirelessModeWarningText, ToastKind::Warning);
}

void MainWindow::hideWirelessModeWarningDialog()
{
    dismissToastByMessage(kWirelessModeWarningText);
}

QString MainWindow::robotLimitToastMessage(bool positiveLimit) const
{
    // 限位 Toast 文案仅对应 J1~J4（寄存器 500=1~4）；500=5 六自由度等其它值不生成文案
    const int axisCode = static_cast<int>(g_registerCache.value(500, 0));
    if (axisCode < 1 || axisCode > 4) {
        return {};
    }

    const QString limitDir = positiveLimit ? QStringLiteral("正") : QStringLiteral("负");
    return QStringLiteral("J%1轴到达%2限位").arg(axisCode).arg(limitDir);
}

void MainWindow::showRobotLimitReachedDialog(bool positiveLimit)
{
    // 正负限位 Toast 仅在首页（机械臂页）显示
    if (!ui || !ui->StackedWidget || ui->StackedWidget->currentIndex() != 0) {
        return;
    }

    const QString message = robotLimitToastMessage(positiveLimit);
    if (message.isEmpty()) {
        return;
    }

    m_robotLimitDialogTrigger = positiveLimit ? RobotLimitDialogTrigger::Positive
                                              : RobotLimitDialogTrigger::Negative;

    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "提示系统";
        record.controlName = "主控限位提示";
        record.controlType = "提示窗口";
        record.operation = "提示触发";
        record.oldValue = "";
        record.newValue = message;
        m_recorder->addRecord(record);
    }

    const int toastCountBefore = m_toasts.size();
    showToast(message, ToastKind::Warning, 5000, [this]() {
        m_robotLimitToastWidget = nullptr;
        m_robotLimitDialogTrigger = RobotLimitDialogTrigger::None;
    });
    if (m_toasts.size() > toastCountBefore) {
        m_robotLimitToastWidget = m_toasts.last().widget;
    }
}

void MainWindow::hideRobotLimitReachedDialog()
{
    if (m_robotLimitToastWidget) {
        dismissToast(m_robotLimitToastWidget);
        m_robotLimitToastWidget = nullptr;
    }
    m_robotLimitDialogTrigger = RobotLimitDialogTrigger::None;
}

void MainWindow::showRobotWeightOverloadDialog()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_robotWeightOverloadWidget) {
        m_robotWeightOverloadWidget = new QWidget(nullptr);
        m_robotWeightOverloadWidget->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                                    Qt::WindowStaysOnTopHint);
        m_robotWeightOverloadWidget->setWindowModality(Qt::ApplicationModal);
        m_robotWeightOverloadWidget->setObjectName("robotWeightOverloadWidget");

        QVBoxLayout *layout = new QVBoxLayout(m_robotWeightOverloadWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_robotWeightOverloadLabel = new QLabel(m_robotWeightOverloadWidget);
        m_robotWeightOverloadLabel->setAlignment(Qt::AlignCenter);
        m_robotWeightOverloadLabel->setWordWrap(true);
        m_robotWeightOverloadLabel->setText(QStringLiteral("负载超限，注意倾覆风险！"));
        layout->addWidget(m_robotWeightOverloadLabel);

        m_robotWeightOverloadConfirmBtn = new QPushButton(QStringLiteral("确认"), m_robotWeightOverloadWidget);
        m_robotWeightOverloadConfirmBtn->setObjectName(QStringLiteral("robotWeightOverloadConfirmBtn"));
        layout->addWidget(m_robotWeightOverloadConfirmBtn, 0, Qt::AlignCenter);
        connect(m_robotWeightOverloadConfirmBtn, &QPushButton::clicked,
                this, &MainWindow::onRobotWeightOverloadConfirmClicked);

        m_robotWeightOverloadWidget->setFixedSize(360, 168);
        m_robotWeightOverloadWidget->setStyleSheet(
            "#robotWeightOverloadWidget {"
            "  background-color: rgba(45, 0, 0, 232);"
            "  border: 3px solid #ff5555;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
            "  color: #ffb3b3;"
            "  font-size: 22px;"
            "  font-weight: bold;"
            "  background: transparent;"
            "}"
            "#robotWeightOverloadConfirmBtn {"
            "  background-color: #ffaa00;"
            "  color: #2b1800;"
            "  border: 2px solid #ffd166;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 90px;"
            "}"
            "#robotWeightOverloadConfirmBtn:hover {"
            "  background-color: #ffd166;"
            "}");
    }

    positionFloatingPopupTopRight(m_robotWeightOverloadWidget, 980);
    m_robotWeightOverloadWidget->show();
    m_robotWeightOverloadWidget->raise();
    m_robotWeightOverloadWidget->activateWindow();
}

void MainWindow::onRobotWeightOverloadConfirmClicked()
{
    writeToMainDevice(290, 1);
    m_robotWeightOverloadUserAckedWhileActive = true;
    hideRobotWeightOverloadDialog();
    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("负载超限提示");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("用户确认");
        record.oldValue = QString();
        record.newValue = QStringLiteral("用户确认负载超限提示，已向主控(192.168.1.13)寄存器290写入1");
        m_recorder->addRecord(record);
    }
}

void MainWindow::hideRobotWeightOverloadDialog()
{
    if (m_robotWeightOverloadWidget && m_robotWeightOverloadWidget->isVisible()) {
        m_robotWeightOverloadWidget->hide();
    }
}

bool MainWindow::isRobotWeightLockGateActive() const
{
    return m_robotWeightLock150Bit7Flag;
}

void MainWindow::blockRobotWeightLockOperation(const QString &hint)
{
    m_robotWeightLockUserAckedWhileActive = false;
    showRobotWeightLockDialog();
    if (ui && ui->statusBar) {
        ui->statusBar->showMessage(hint, 3000);
    }
}

void MainWindow::showRobotWeightLockDialog()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (m_robotWeightLockUserAckedWhileActive) {
        return;
    }

    if (!m_robotWeightLockWidget) {
        m_robotWeightLockWidget = new QWidget(nullptr);
        m_robotWeightLockWidget->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                                  Qt::WindowStaysOnTopHint);
        m_robotWeightLockWidget->setWindowModality(Qt::ApplicationModal);
        m_robotWeightLockWidget->setObjectName(QStringLiteral("robotWeightLockWidget"));

        QVBoxLayout *layout = new QVBoxLayout(m_robotWeightLockWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_robotWeightLockLabel = new QLabel(m_robotWeightLockWidget);
        m_robotWeightLockLabel->setAlignment(Qt::AlignCenter);
        m_robotWeightLockLabel->setWordWrap(true);
        m_robotWeightLockLabel->setText(QStringLiteral("负载超重锁定，需应急解锁！"));
        layout->addWidget(m_robotWeightLockLabel);

        m_robotWeightLockConfirmBtn = new QPushButton(QStringLiteral("确认"), m_robotWeightLockWidget);
        m_robotWeightLockConfirmBtn->setObjectName(QStringLiteral("robotWeightLockConfirmBtn"));
        layout->addWidget(m_robotWeightLockConfirmBtn, 0, Qt::AlignCenter);
        connect(m_robotWeightLockConfirmBtn, &QPushButton::clicked,
                this, &MainWindow::onRobotWeightLockConfirmClicked);

        m_robotWeightLockWidget->setFixedSize(360, 168);
        m_robotWeightLockWidget->setStyleSheet(
            "#robotWeightLockWidget {"
            "  background-color: rgba(45, 0, 0, 232);"
            "  border: 3px solid #ff5555;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
            "  color: #ffb3b3;"
            "  font-size: 22px;"
            "  font-weight: bold;"
            "  background: transparent;"
            "}"
            "#robotWeightLockConfirmBtn {"
            "  background-color: #ffaa00;"
            "  color: #2b1800;"
            "  border: 2px solid #ffd166;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 90px;"
            "}"
            "#robotWeightLockConfirmBtn:hover {"
            "  background-color: #ffd166;"
            "}");
    }

    positionFloatingPopupTopRight(m_robotWeightLockWidget, 1020);
    m_robotWeightLockWidget->show();
    m_robotWeightLockWidget->raise();
    m_robotWeightLockWidget->activateWindow();
}

void MainWindow::onRobotWeightLockConfirmClicked()
{
    writeToMainDevice(290, 1);
    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("负载超重锁定");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("用户确认");
        record.oldValue = QString();
        record.newValue = QStringLiteral("用户确认负载超重锁定提示，已向主控(192.168.1.13)寄存器290写入1");
        m_recorder->addRecord(record);
    }
    m_robotWeightLockUserAckedWhileActive = true;
    hideRobotWeightLockDialog();
}

void MainWindow::hideRobotWeightLockDialog()
{
    if (m_robotWeightLockWidget && m_robotWeightLockWidget->isVisible()) {
        m_robotWeightLockWidget->hide();
    }
}

void MainWindow::showRobotAxisSyncDeviationDialog()
{
    if (!userPopupsAllowed()) {
        return;
    }
    if (!m_robotAxisSyncDeviationWidget) {
        m_robotAxisSyncDeviationWidget = new QWidget(nullptr);
        m_robotAxisSyncDeviationWidget->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                                                       Qt::WindowStaysOnTopHint);
        m_robotAxisSyncDeviationWidget->setWindowModality(Qt::ApplicationModal);
        m_robotAxisSyncDeviationWidget->setObjectName(QStringLiteral("robotAxisSyncDeviationWidget"));

        QVBoxLayout *layout = new QVBoxLayout(m_robotAxisSyncDeviationWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_robotAxisSyncDeviationLabel = new QLabel(m_robotAxisSyncDeviationWidget);
        m_robotAxisSyncDeviationLabel->setAlignment(Qt::AlignCenter);
        m_robotAxisSyncDeviationLabel->setWordWrap(true);
        m_robotAxisSyncDeviationLabel->setText(QStringLiteral("主副轴位置偏差过大"));
        layout->addWidget(m_robotAxisSyncDeviationLabel);

        m_robotAxisSyncDeviationStartBtn = new QPushButton(QStringLiteral("开始同步"), m_robotAxisSyncDeviationWidget);
        m_robotAxisSyncDeviationStartBtn->setObjectName(QStringLiteral("robotAxisSyncDeviationStartBtn"));
        layout->addWidget(m_robotAxisSyncDeviationStartBtn, 0, Qt::AlignCenter);
        connect(m_robotAxisSyncDeviationStartBtn, &QPushButton::clicked,
                this, &MainWindow::onRobotAxisSyncStartClicked);

        m_robotAxisSyncDeviationWidget->setFixedSize(360, 188);
        m_robotAxisSyncDeviationWidget->setStyleSheet(
            QStringLiteral(
                "#robotAxisSyncDeviationWidget {"
                "  background-color: rgba(45, 0, 0, 232);"
                "  border: 3px solid #ff5555;"
                "  border-radius: 10px;"
                "}"
                "QLabel {"
                "  color: #ffb3b3;"
                "  font-size: 22px;"
                "  font-weight: bold;"
                "  background: transparent;"
                "}"
                "#robotAxisSyncDeviationStartBtn {"
                "  background-color: #ffaa00;"
                "  color: #2b1800;"
                "  border: 2px solid #ffd166;"
                "  border-radius: 6px;"
                "  padding: 8px 16px;"
                "  font-size: 14px;"
                "  font-weight: bold;"
                "  min-width: 90px;"
                "}"
                "#robotAxisSyncDeviationStartBtn:hover {"
                "  background-color: #ffd166;"
                "}"));
    }

    positionFloatingPopupTopRight(m_robotAxisSyncDeviationWidget, 940);
    m_robotAxisSyncDeviationWidget->show();
    m_robotAxisSyncDeviationWidget->raise();
    m_robotAxisSyncDeviationWidget->activateWindow();
}

void MainWindow::onRobotAxisSyncStartClicked()
{
    if (!isFeatureEnabled("modbus_main", "modbus_main.write_enabled")) {
        showModbusWriteDisabledToast();
        return;
    }
    if (!isFeatureEnabled("modbus_main", "modbus_main.read_enabled")) {
        showNotification(QStringLiteral("Main Modbus 读功能已关闭"));
        return;
    }
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        showNotification(QStringLiteral("主控 Modbus 未连接"));
        return;
    }
    writeToMainDevice(290, 1);
    quint16 cur527 = 0;
    if (!m_modbusManager->readSingleRegister(527, cur527)) {
        qWarning() << "[主副轴同步] 读取寄存器527失败";
        showNotification(QStringLiteral("读取寄存器527失败"));
        return;
    }
    constexpr int kBit = 5;
    const quint16 next527 = static_cast<quint16>(cur527 | (static_cast<quint16>(1u) << kBit));
    writeToMainDevice(527, next527);
    qCDebug(lcMainWindow) << "[主副轴同步] 527: 原值" << cur527 << "→ 写入" << next527 << "(bit" << kBit << "=1)";
    if (m_recorder) {
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = QStringLiteral("提示系统");
        record.controlName = QStringLiteral("主副轴位置偏差");
        record.controlType = QStringLiteral("提示窗口");
        record.operation = QStringLiteral("开始同步");
        record.oldValue = QString();
        record.newValue = QStringLiteral(
            "用户点击开始同步，已向主控(192.168.1.13)寄存器290写入1，并向寄存器527位5写入1");
        m_recorder->addRecord(record);
    }
}

void MainWindow::hideRobotAxisSyncDeviationDialog()
{
    if (m_robotAxisSyncDeviationWidget && m_robotAxisSyncDeviationWidget->isVisible()) {
        m_robotAxisSyncDeviationWidget->hide();
    }
}

void MainWindow::onTestAlarmButtonClicked()
{
    qCDebug(lcMainWindow) << "=== 开始报警系统测试 ===";

    // 测试1：直接调用showAlarm函数，测试窗口是否能显示
    qCDebug(lcMainWindow) << "测试1：直接调用showAlarm函数...";
    showAlarm("手动测试报警 - 紧急停止", "#ff5555");

    // 等待3秒
    QTimer::singleShot(3000, this, [this]() {
        qCDebug(lcMainWindow) << "测试2：测试一般报警显示...";
        showAlarm("手动测试报警 - 一般提示", "#ff8800");
    });

    // 等待6秒，测试报警条件检查
    QTimer::singleShot(6000, this, [this]() {
        qCDebug(lcMainWindow) << "测试3：通过设置标志位触发报警检查...";

        // 设置报警标志
        m_emergencyStopColumnFlag = true;
        m_emergencyStopChassisFlag = true;
        qCDebug(lcMainWindow) << "立柱急停标志:" << m_emergencyStopColumnFlag;
        qCDebug(lcMainWindow) << "底盘急停标志:" << m_emergencyStopChassisFlag;

        // 手动触发报警检查
        checkAlarmConditions();
    });

    // 等待9秒，清除报警
    QTimer::singleShot(9000, this, [this]() {
        qCDebug(lcMainWindow) << "测试4：清除报警...";

        // 清除报警标志
        m_emergencyStopColumnFlag = false;
        m_emergencyStopChassisFlag = false;
        // 触发报警检查
        checkAlarmConditions();

        qCDebug(lcMainWindow) << "=== 报警系统测试完成 ===";
        ui->statusBar->showMessage("报警系统测试完成", 3000);
    });
}

void MainWindow::on_Btn_test_clicked()
{
    if (!m_poseProvider) return;

    static double angle = 0;
    static double distance = 0;
    
    // 模拟角度变化
    angle += 5.0;
    if (angle > 360.0) angle = 0;
    
    // 模拟平移变化 (往复运动)
    distance += 10.0;
    if (distance > 200.0) distance = -200.0;

    m_poseProvider->setX(distance);
    m_poseProvider->setY(distance * 0.8);
    m_poseProvider->setZ(distance * 0.5);

    m_poseProvider->setRoll(angle);
    m_poseProvider->setPitch(angle * 0.5);
    m_poseProvider->setYaw(angle * 1.2);

    qCDebug(lcMainWindow) << "Test Pose Update - Angle:" << angle << "Distance:" << distance;
}

// ============ 转向模式切换报警逻辑 ============

void MainWindow::checkSteeringSwitchCompletion(int address, quint16 value)
{
    // 如果没有在等待或者地址不对，直接返回
    if (!m_isSwitchingSteeringMode || address != 50) {
        return;
    }

    if (m_targetSteeringWaitBit < 0 || m_targetSteeringWaitBit > 15) {
        return;
    }

    const bool targetBitSet = ((value >> m_targetSteeringWaitBit) & 0x01);
    if (targetBitSet) {
        qCDebug(lcMainWindow) << "[转向切换] 检测到地址50满足条件: Bit" << m_targetSteeringWaitBit << "=1，切换完成";
        
        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "AGV控制";
            record.controlName = "底盘模式切换";
            record.controlType = "模式控制";
            record.operation = "切换完成";
            record.oldValue = "";
            record.newValue = "底盘模式切换成功";
            m_recorder->addRecord(record);
        }

        m_isSwitchingSteeringMode = false;
        m_targetSteeringWaitBit = -1;
        m_isSteeringAlarmActive = false;
        hideAlarm();
    }
}
