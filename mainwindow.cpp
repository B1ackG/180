#include "mainwindow.h"
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
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QDialog>
#include <QFileDialog>
#include <QSettings>
#include <QToolTip>
#include <QButtonGroup>
#include <QGuiApplication>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <QtMath>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <QSocketNotifier>

namespace {
SteeringMode steeringModeFromRegisterValue(quint16 value)
{
    switch (value) {
    case 1: return STEER_FRONT_BACK;
    case 2: return STEER_FRONT_ONLY;
    case 3: return STEER_PARALLEL;
    case 4: return STEER_LATERAL;
    case 5: return STEER_ROTATE;
    // 兼容旧控制器枚举（0~4）
    case 0: return STEER_FRONT_BACK;
    default: return STEER_FRONT_BACK;
    }
}

bool resolveSteeringModeFromStatus50(quint16 value, SteeringMode *mode, QString *modeText)
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
        if (mode) *mode = STEER_FRONT_BACK;
        if (modeText) *modeText = QStringLiteral("前后转向");
        return true;
    }
    return false;
}

void applyTransparentQuickWidgetBackground(QQuickWidget *widget)
{
    if (!widget) {
        return;
    }

    const QColor panelBlue(26, 95, 180);
    widget->setStyleSheet(QStringLiteral("background-color: rgb(26, 95, 180); border: 0px;"));
    widget->setClearColor(panelBlue);
    if (widget->quickWindow()) {
        widget->quickWindow()->setColor(panelBlue);
    }
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
}

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

    m_uiStateSyncEnabled = settings.value("ui_state_sync_enabled", true).toBool();
    m_mainModbusPollIntervalMs = settings.value("main_modbus_poll_ms", 500).toInt();
    m_mainUiPollIntervalMs = settings.value("main_ui_poll_ms", 200).toInt();
    m_mainDeviceStatusPollIntervalMs = settings.value("main_device_status_poll_ms", 2000).toInt();
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
}

void MainWindow::savePollingRuntimeSettings() const
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    settings.setValue("ui_state_sync_enabled", m_uiStateSyncEnabled);
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
    if (ui->TBtn_HomePage) {
        ui->TBtn_HomePage->setChecked(true);
    }

    connect(ui->TBtn_HomePage, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(0);
        ui->TBtn_HomePage->setChecked(true);
        updateNavButtonStyles(nullptr);
    });

    connect(ui->TBtn_SixAxies, &QPushButton::clicked, [this]() {
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
            showNotification(tip);
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

        if (m_isJointMode) {
            writeToMainDevice(525, 2);
            ui->TBtn_MoveMode->setText("关节模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("关节模式");
                moveModeLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
            }
            showNotification("已切换至关节模式");
        } else {
            writeToMainDevice(525, 1);
            ui->TBtn_MoveMode->setText("坐标模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("坐标模式");
                moveModeLabel->setStyleSheet("color: #ffaa00; font-weight: bold; font-size: 11px;");
            }
            showNotification("已切换至坐标模式");
        }

        updateFunctionSwitchVisuals();
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

    m_btnForceControl = findChild<TechPushButton*>("btn_ForceControl");
    if (m_btnForceControl) {
        m_btnForceControl->setText(m_forcecontrolMode ? "力控开启" : "力控关闭");
        if (m_forcecontrolMode) {
            m_btnForceControl->setPrimaryColor(QColor("#00C8FF"));
            m_btnForceControl->setGlowColor(QColor(0, 200, 255, 180));
        } else {
            m_btnForceControl->setPrimaryColor(QColor("#7F8C8D"));
            m_btnForceControl->setGlowColor(QColor(127, 140, 141, 100));
        }
        connect(m_btnForceControl, &TechPushButton::clicked,
                this, &MainWindow::toggleForceControl);
        qCDebug(lcMainWindow) << "力控按钮初始化完成，初始状态:"
                 << (m_forcecontrolMode ? "开启" : "关闭");
    } else {
        qWarning() << "未找到btn_ForceControl按钮";
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

void MainWindow::updateFunctionSwitchVisuals()
{
    if (!ui) {
        return;
    }

    auto applyModeStyle = [](QToolButton *btn, const QString &color, const QString &labelSuffix) {
        if (!btn) {
            return;
        }
        btn->setStyleSheet(QString(
            "QToolButton {"
            "  color: #f4fbff;"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "  padding: 3px 8px;"
            "  min-height: 36px;"
            "}"
            "QToolButton:hover {"
            "  border: 1px solid #d9f4ff;"
            "}")
            .arg(color, labelSuffix));
    };

    const QString unknownBg = "rgba(96, 102, 114, 0.86)";
    const QString unknownBorder = "#8f99a8";

    // 点动/步进：增强颜色对比，状态一眼可分
    if (m_stepModeUnknown) {
        applyModeStyle(ui->TBtn_Stepmove, unknownBg, unknownBorder);
    } else if (m_stepModeEnabled) {
        applyModeStyle(ui->TBtn_Stepmove, "rgba(30, 148, 84, 0.90)", "#a9ffd0");
    } else {
        applyModeStyle(ui->TBtn_Stepmove, "rgba(172, 108, 26, 0.90)", "#ffd7a1");
    }

    // 关节/坐标：未选择与步进按钮保持同灰色
    if (m_moveModeUnknown) {
        applyModeStyle(ui->TBtn_MoveMode, unknownBg, unknownBorder);
    } else if (m_isJointMode) {
        applyModeStyle(ui->TBtn_MoveMode, "rgba(32, 140, 86, 0.88)", "#9dffd3");
    } else {
        applyModeStyle(ui->TBtn_MoveMode, "rgba(166, 104, 24, 0.88)", "#ffd29a");
    }

    // 有线/无线：白/黄差异
    if (m_controlMode == WIRED_MODE) {
        applyModeStyle(ui->TBtn_ControlMode, "rgba(30, 126, 150, 0.90)", "#a8f0ff");
    } else {
        applyModeStyle(ui->TBtn_ControlMode, "rgba(158, 122, 16, 0.88)", "#ffe28f");
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
    // 处理LineEdit点击事件
    if (event->type() == QEvent::MouseButtonPress) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(obj);
        if (lineEdit && lineEdit->isEnabled() && this->isAncestorOf(lineEdit)) {
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
        QString suffix;
        double min;
        double max;
        int precision;
    };

    QList<ArcConfig> configs = {
        {ui->widget_test1, "robot_ArcGauge_J1Angle", "悬臂角度", "°", -170, 170, 1},
        {ui->widget_test2, "robot_ArcGauge_J2Height", "升降高度", "mm", -850, 1150, 0},
        {ui->widget_test3, "robot_ArcGauge_J3Length", "总伸展长度", "mm", 0, 1600, 0},
        {ui->widget_test4, "robot_ArcGauge_J4Angle", "末端角度", "°", -180, 180, 1},
        {ui->widget_SixAxies_1, "robot_ArcGauge_SixAxis1", "RX", "°", -15, 15, 2},
        {ui->widget_SixAxies_2, "robot_ArcGauge_SixAxis2", "RY", "°", -15, 15, 2},
        {ui->widget_SixAxies_3, "robot_ArcGauge_SixAxis3", "RZ", "°", -12, 12, 2},
        {ui->widget_SixAxies_4, "robot_ArcGauge_SixAxis4", "X", "mm", -110, 110, 2},
        {ui->widget_SixAxies_5, "robot_ArcGauge_SixAxis5", "Y", "mm", -110, 110, 2},
        {ui->widget_SixAxies_6, "robot_ArcGauge_SixAxis6", "Z", "mm", -90, 90, 2}
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
            arcGauge->setSuffix(cfg.suffix);
            arcGauge->setPrecision(cfg.precision);
            
            cfg.placeholder->hide();
            arcGauge->show();

            // 存入映射表，Key 使用规范化后的名称
            m_arcGauges[cfg.name] = arcGauge;
        }
    }

    // 1. 创建 QQuickWidget 来承载 QML
    m_speedGaugeQml = new QQuickWidget(this);
    m_speedGaugeQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_speedGaugeQml->setSource(QUrl("qrc:/TechSpeedGauge.qml"));
    m_speedGaugeQml->setAttribute(Qt::WA_AlwaysStackOnTop);
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
    // 更新 QML 属性实现平滑过渡（QML 内部有动画逻辑）
    if (m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
        m_speedGaugeQml->rootObject()->setProperty("currentValue", newSpeed);
    }
}

void MainWindow::initRobotTotalPowerCard()
{
    m_robotTotalPowerQml = findChild<QQuickWidget*>("quickWidget_RobotTotalPower");
    if (!m_robotTotalPowerQml) {
        qCWarning(lcMainWindow) << "未找到 quickWidget_RobotTotalPower，跳过总功率卡片初始化";
        return;
    }

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
    }
}

void MainWindow::updateRobotTotalPower(quint16 powerValue)
{
    if (!(m_robotTotalPowerQml && m_robotTotalPowerQml->rootObject())) {
        return;
    }

    QQuickItem *root = m_robotTotalPowerQml->rootObject();
    const qreal numericPower = static_cast<qreal>(powerValue);
    root->setProperty("currentPower", numericPower);
    // 即使值不变也追加样本，保证趋势图持续可见。
    QMetaObject::invokeMethod(root, "appendSample", Q_ARG(QVariant, numericPower));
}

void MainWindow::initInclinometerCards()
{
    m_inclinometerXCard = findChild<QWidget*>("quickWidget_Inclinometer_X");
    m_inclinometerYCard = findChild<QWidget*>("quickWidget_Inclinometer_Y");

    auto initOne = [](QWidget *widget, const QString &axisTitle, QLabel *&valueLabel) {
        if (!widget) {
            return;
        }

        widget->setStyleSheet(QStringLiteral(
            "background-color: #1A5FB4;"
            "border: 1px solid #4FAFE8;"
            "border-radius: 14px;"));

        if (QLayout *oldLayout = widget->layout()) {
            QLayoutItem *item = nullptr;
            while ((item = oldLayout->takeAt(0)) != nullptr) {
                if (item->widget()) {
                    item->widget()->deleteLater();
                }
                delete item;
            }
            delete oldLayout;
        }

        auto *layout = new QVBoxLayout(widget);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(4);

        auto *axisLabel = new QLabel(axisTitle, widget);
        axisLabel->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        axisLabel->setStyleSheet(QStringLiteral(
            "color: #A8DAFF;"
            "font: 700 14px 'Noto Sans CJK SC';"
            "border: none;"
            "background: transparent;"));

        valueLabel = new QLabel(QStringLiteral("0.00°"), widget);
        valueLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        valueLabel->setStyleSheet(QStringLiteral(
            "color: #EAF7FF;"
            "font: 700 28px 'Noto Sans CJK SC';"
            "border: none;"
            "background: transparent;"));

        auto *thresholdLabel = new QLabel(QStringLiteral("阈值：1°"), widget);
        thresholdLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        thresholdLabel->setWordWrap(true);
        thresholdLabel->setStyleSheet(QStringLiteral(
            "color: #8BC4EA;"
            "font: 500 12px 'Noto Sans CJK SC';"
            "border: none;"
            "background: transparent;"));

        layout->addWidget(axisLabel, 0);
        layout->addWidget(valueLabel, 1);
        layout->addWidget(thresholdLabel, 0);
    };

    initOne(m_inclinometerXCard, QStringLiteral("X轴倾角"), m_inclinometerXValueLabel);
    initOne(m_inclinometerYCard, QStringLiteral("Y轴倾角"), m_inclinometerYValueLabel);
}

void MainWindow::updateInclinometerValue(bool isXAxis, quint16 rawValue)
{
    QLabel *targetLabel = isXAxis ? m_inclinometerXValueLabel : m_inclinometerYValueLabel;
    if (!targetLabel) {
        return;
    }

    const qint16 signedRaw = static_cast<qint16>(rawValue);
    const qreal degree = static_cast<qreal>(signedRaw) / 100.0;
    targetLabel->setText(QString::number(degree, 'f', 2) + QStringLiteral("°"));
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
                this, [this](double /*oldValue*/, double newValue) {
                    const double speedPercent = qBound(0.0, newValue, 100.0);
                    const int speedValue = qBound(0, static_cast<int>(qRound(speedPercent)), 100);
                    writeToMainDevice(5001, speedValue);

                    qCDebug(lcMainWindow) << "RobotSpeed(%)直写地址5001, 值:" << speedValue;
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
                    this, [this, slider, pageIndex](double oldValue, double newValue) {
                        OperationRecord record;
                        record.timestamp = QDateTime::currentDateTime();
                        record.pageName = MappingConfig::instance()->mapPageName(QString::number(pageIndex));
                        record.controlName = MappingConfig::instance()->mapControlName(slider->objectName());
                        record.controlType = MappingConfig::instance()->mapControlType("TechSliderEdit");
                        record.operation = MappingConfig::instance()->mapOperation("valueChanged");
                        record.oldValue = QString::number(oldValue);
                        record.newValue = QString::number(newValue);

                        m_recorder->addRecord(record);
                    });
        }
    }
    // 连接所有TechPushButton的记录信号
    QList<TechPushButton*> allButtons = this->findChildren<TechPushButton*>();
    for (TechPushButton* button : allButtons) {
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

                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = MappingConfig::instance()->mapPageName(QString::number(pageIndex));
                    record.controlName = MappingConfig::instance()->mapControlName(button->objectName());
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

                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = MappingConfig::instance()->mapPageName(pageName);
                    record.controlName = MappingConfig::instance()->mapControlName(toolButton->objectName());
                    record.controlType = MappingConfig::instance()->mapControlType("QToolButton");
                    record.operation = MappingConfig::instance()->mapOperation("clicked");
                    record.oldValue = "";
                    record.newValue = MappingConfig::instance()->mapValue(toolButton->text().isEmpty() ? toolButton->toolTip() : toolButton->text());

                    m_recorder->addRecord(record);

                    // 在状态栏显示通知
                    showNotification(QString("工具按钮点击: %1").arg(record.controlName));
                });

        // 如果按钮有toggle状态，也记录
        if (toolButton->isCheckable()) {
            connect(toolButton, &QToolButton::toggled,
                    this, [this, toolButton](bool checked) {
                        QString pageName = getControlPageName(toolButton);

                        OperationRecord record;
                        record.timestamp = QDateTime::currentDateTime();
                        record.pageName = pageName;
                        record.controlName = toolButton->objectName();
                        record.controlType = "QToolButton";
                        record.operation = "toggled";
                        record.oldValue = !checked;
                        record.newValue = checked;

                        m_recorder->addRecord(record);

                        showNotification(QString("工具按钮切换: %1 -> %2")
                                             .arg(record.controlName)
                                             .arg(checked ? "选中" : "未选中"));
                    });
        }
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
            QMessageBox::information(this, "成功", "操作记录已保存");
        } else {
            QMessageBox::warning(this, "错误", "保存失败");
        }
    }
}

void MainWindow::onExportRecords()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "导出操作报告", "operation_report.txt", "文本文件 (*.txt)");

    if (!filename.isEmpty()) {
        if (m_recorder->exportToText(filename)) {
            QMessageBox::information(this, "成功", "操作报告已导出");
        } else {
            QMessageBox::warning(this, "错误", "导出失败");
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
    QLabel *titleLabel = new QLabel("权限验证", container);
    titleLabel->setObjectName("adminTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    // 角色选择下拉框
    QComboBox *roleComboBox = new QComboBox(container);
    roleComboBox->setObjectName("roleComboBox");
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

    // 网络配置 (左右排布：WIN7_IP 和 远程模拟器)
    QWidget *netConfigSection = new QWidget(container);
    netConfigSection->setObjectName("netConfigSection");
    QVBoxLayout *netMainLayout = new QVBoxLayout(netConfigSection);
    netMainLayout->setContentsMargins(0, 0, 0, 0);
    netMainLayout->setSpacing(10);

    // 第一行：WIN7_IP
    QHBoxLayout *row1Layout = new QHBoxLayout();
    QLabel *ipPrefix = new QLabel("WIN7_IP: 192.168.1.", netConfigSection);
    ipPrefix->setStyleSheet("color: #00ffff; font-family: 'Microsoft YaHei UI'; font-size: 14px;");
    QLineEdit *ipHostEdit = new QLineEdit(netConfigSection);
    ipHostEdit->setObjectName("ipHostEdit");
    ipHostEdit->setPlaceholderText("100");
    ipHostEdit->setText("100");
    ipHostEdit->setFixedWidth(40);
    ipHostEdit->setAlignment(Qt::AlignCenter);
    ipHostEdit->setValidator(new QIntValidator(0, 255, ipHostEdit));
    ipHostEdit->setStyleSheet("QLineEdit { background: rgba(0, 0, 0, 100); border: 1px solid #00c8ff; color: #ffaa00; border-radius: 4px; }");
    
    QPushButton *ipApplyBtn = new QPushButton("确认", netConfigSection);
    ipApplyBtn->setFixedWidth(50);
    ipApplyBtn->setStyleSheet("QPushButton { background-color: #004466; border: 1px solid #00c8ff; color: white; border-radius: 4px; font-size: 12px; }");

    row1Layout->addWidget(ipPrefix);
    row1Layout->addWidget(ipHostEdit);
    row1Layout->addWidget(ipApplyBtn);
    row1Layout->addStretch();

    // 第二行：远程模拟器
    QHBoxLayout *row2Layout = new QHBoxLayout();
    QLabel *simPrefix = new QLabel("远程模拟器: 192.168.1.", netConfigSection);
    simPrefix->setStyleSheet("color: #00ffff; font-family: 'Microsoft YaHei UI'; font-size: 14px;");
    QLineEdit *simHostEdit = new QLineEdit(netConfigSection);
    simHostEdit->setObjectName("simHostEdit");
    simHostEdit->setPlaceholderText("70");
    simHostEdit->setText("70");
    simHostEdit->setFixedWidth(40);
    simHostEdit->setAlignment(Qt::AlignCenter);
    simHostEdit->setValidator(new QIntValidator(0, 255, simHostEdit));
    simHostEdit->setStyleSheet("QLineEdit { background: rgba(0, 0, 0, 100); border: 1px solid #00c8ff; color: #ffaa00; border-radius: 4px; }");

    QPushButton *simApplyBtn = new QPushButton("确认", netConfigSection);
    simApplyBtn->setFixedWidth(50);
    simApplyBtn->setStyleSheet("QPushButton { background-color: #004466; border: 1px solid #00c8ff; color: white; border-radius: 4px; font-size: 12px; }");

    row2Layout->addWidget(simPrefix);
    row2Layout->addWidget(simHostEdit);
    row2Layout->addWidget(simApplyBtn);
    row2Layout->addStretch();

    netMainLayout->addLayout(row1Layout);
    netMainLayout->addLayout(row2Layout);
    netConfigSection->setVisible(false); // 默认隐藏

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
    containerLayout->addWidget(netConfigSection); // 厂家登录后可见（包含本地IP及模拟器配置）
    containerLayout->addWidget(logoutButton);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(errorLabel);
    containerLayout->addStretch(3);

    // 设置容器大小和居中
    container->setFixedSize(480, 560);

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

    // 连接 IP 确认按钮
    connect(ipApplyBtn, &QPushButton::clicked, this, [this, ipHostEdit]() {
        QString text = ipHostEdit->text();
        if (!text.isEmpty()) {
            updateTcpServerHost(text);
            showNotification("WIN7_IP 已更新: 192.168.1." + text);
        }
    });

    // 连接 模拟器 IP 确认按钮
    connect(simApplyBtn, &QPushButton::clicked, this, [this, simHostEdit]() {
        QString text = simHostEdit->text();
        if (!text.isEmpty()) {
            updateSimulatorHost(text);
            showNotification("模拟器 IP 已更新: 192.168.1." + text);
        }
    });

    // 连接登录按钮
    connect(loginButton, &QPushButton::clicked, this, [this, roleComboBox, passwordEdit, errorLabel, titleLabel, loginButton, logoutButton, hintLabel, featureButton, netConfigSection, ipHostEdit, simHostEdit]() {
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

        // 验证明文口令
        bool loginSuccess = false;
        if (selectedRole == UserRole::Admin && password == "123") {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Engineer && password == "456") {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Manufacturer && password == "8888") {
            loginSuccess = true;
        }

        if (loginSuccess) {
            // 设置登录状态
            m_currentUserRole = selectedRole;

            // 记录成功登录
            OperationRecord successRecord;
            successRecord.timestamp = QDateTime::currentDateTime();
            successRecord.pageName = "权限验证";
            successRecord.controlName = "loginButton";
            successRecord.controlType = "LoginSuccess";
            successRecord.operation = "login_success";
            successRecord.oldValue = "";
            successRecord.newValue = QString("登录成功: %1").arg(roleName);
            m_recorder->addRecord(successRecord);

            // 更新界面状态
            titleLabel->setText(QString("当前权限: %1").arg(roleName));
            roleComboBox->setVisible(false);
            passwordEdit->setVisible(false);
            hintLabel->setVisible(false);
            loginButton->setVisible(false);
            logoutButton->setVisible(true);
            featureButton->setVisible(m_currentUserRole == UserRole::Manufacturer);
            netConfigSection->setVisible(m_currentUserRole == UserRole::Manufacturer);
            errorLabel->setVisible(false);
            passwordEdit->clear();

            // 登录成功后仅显示提示，不自动跳转到操作记录页面
            showNotification(QString("%1 登录成功").arg(roleName));
        } else {
            errorLabel->setText("密码错误！请重试。");
            errorLabel->setVisible(true);

            // 记录失败登录
            OperationRecord failRecord;
            failRecord.timestamp = QDateTime::currentDateTime();
            failRecord.pageName = "权限验证";
            failRecord.controlName = "loginButton";
            failRecord.controlType = "LoginFail";
            failRecord.operation = "login_fail";
            failRecord.oldValue = "";
            failRecord.newValue = QString("登录失败: %1").arg(password.isEmpty() ? "(空)" : "******");
            m_recorder->addRecord(failRecord);

            // 清空密码框并设置焦点
            passwordEdit->clear();
            passwordEdit->setFocus();
        }
    });

    // 连接注销按钮
    connect(logoutButton, &QPushButton::clicked, this, [this, titleLabel, roleComboBox, passwordEdit, hintLabel, loginButton, logoutButton, errorLabel, featureButton, netConfigSection]() {
        // 记录注销
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "权限验证";
        record.controlName = "logoutButton";
        record.controlType = "Logout";
        record.operation = "logout";
        record.oldValue = "";
        record.newValue = "注销，返回操作员权限";
        m_recorder->addRecord(record);

        // 恢复为操作员权限
        m_currentUserRole = UserRole::Operator;

        // 恢复界面状态
        titleLabel->setText("权限验证");
        roleComboBox->setVisible(true);
        passwordEdit->setVisible(true);
        hintLabel->setVisible(true);
        loginButton->setVisible(true);
        logoutButton->setVisible(false);
        featureButton->setVisible(false);
        netConfigSection->setVisible(false);
        errorLabel->setVisible(false);
        
        showNotification("已注销，当前为操作员权限");
    });

    // 回车键登录
    connect(passwordEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);


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
                    loadSliderLabelRuntimeSettings();
                    applySliderLabelRuntimeSettings();
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
            record.newValue = QString("升降组件当前高度为%1mm，当前以%2mm/s速度下降")
                                  .arg(currentHeight, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("升降组件当前高度为%1mm，下降完成")
                                  .arg(currentHeight, 0, 'f', 1);
        }
    } else if (keyNumber == 2) {  // ○2 升降上升
        if (pressed) {
            record.newValue = QString("升降组件当前高度为%1mm，当前以%2mm/s速度上升")
                                  .arg(currentHeight, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("升降组件当前高度为%1mm，上升完成")
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
            record.newValue = QString("悬臂组件当前角度为%1°，当前以%2°/s速度负方向旋转")
                                  .arg(currentAngle, 0, 'f', 1)
                                  .arg(rotationSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前角度为%1°，旋转完成")
                                  .arg(currentAngle, 0, 'f', 1);
        }
    } else if (keyNumber == 4) {  // ○4 正方向旋转
        if (pressed) {
            record.newValue = QString("悬臂组件当前角度为%1°，当前以%2°/s速度正方向旋转")
                                  .arg(currentAngle, 0, 'f', 1)
                                  .arg(rotationSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前角度为%1°，旋转完成")
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
            record.newValue = QString("悬臂组件当前长度为%1mm，当前以%2mm/s速度缩短")
                                  .arg(currentLength, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前长度为%1mm，缩短完成")
                                  .arg(currentLength, 0, 'f', 1);
        }
    } else if (keyNumber == 2) {  // ○2 伸长
        if (pressed) {
            record.newValue = QString("悬臂组件当前长度为%1mm，当前以%2mm/s速度伸长")
                                  .arg(currentLength, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前长度为%1mm，伸长完成")
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
        return QStringLiteral("高度互锁，请降低高度后操作");
    }
    if (((status150 >> 2) & 0x01) == 1) {
        return QStringLiteral("长度互锁，请缩小长度后操作");
    }
    return QString();
}

// 在 handleMatrixKeyAction 函数中修改 ○1 按键的处理
void MainWindow::handleMatrixKeyAction(int keyNumber, bool pressed)
{
    // 获取当前页面
    int currentPage = ui->StackedWidget->currentIndex();
    QString pageName = m_pageNames.value(currentPage, "未知");
    const bool isRobotPage = (currentPage == 0 || pageName == "机械臂" || pageName == "page_Robot");
    const bool isSixAxisPage = (currentPage == 3 || pageName == "六自由度" || pageName == "page_SixAxies");

    // 首页外部按键提示：
    // ○1/○2 检查主设备地址150的bit1；○3/○4 检查bit2。
    if (isRobotPage && pressed && keyNumber >= 1 && keyNumber <= 4) {
        if (!m_mainRegister150Valid && MainDeviceModbusApi::isReady(m_modbusManager)) {
            MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 150, 1);
        }

        const QString interlockHint = robotInterlockHintMessage();
        if ((keyNumber == 1 || keyNumber == 2) && !interlockHint.isEmpty()
            && interlockHint.contains(QStringLiteral("高度互锁"))) {
            showRobotOperationHintDialog(interlockHint);
            return;
        }
        if (keyNumber == 4 && !interlockHint.isEmpty()
            && interlockHint.contains(QStringLiteral("长度互锁"))) {
            showRobotOperationHintDialog(interlockHint);
            return;
        }
    }

    // 外部按钮逻辑门禁：
    // - 点动模式：仅允许在[关节]模式下执行；
    // - 步进模式：允许执行（由按键映射与目标选择进一步约束）。
    // 释放事件仍继续处理，避免切模后寄存器保持在按下态。
    if ((!m_stepModeEnabled && !m_isJointMode) && pressed) {
        qCDebug(lcMainWindow) << "外部按键忽略：当前未处于可执行模式，按键○" << keyNumber
                 << (pressed ? "按下" : "释放");
        return;
    }

    // 六自由度页面（第4页）外部键逻辑：
    // - ○13/○14：固定写500=5，514=4/2；释放时514回0；
    // - 点动模式：按键○1~○12映射到613写入±1~±6；
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

            qCDebug(lcMainWindow) << "六自由度页面外部按键○" << keyNumber
                                  << "写入 500=5, 514=" << value514;
            return;
        }

        if (keyNumber < 1 || keyNumber > 12) {
            return;
        }

        if (!m_stepModeUnknown && m_stepModeEnabled) {
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
                return;
            }

            const bool isOddKey = ((keyNumber % 2) == 1);    // 奇数键写相反数
            if (isOddKey) {
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
            return;
        }

        if (m_stepModeUnknown) {
            if (pressed) {
                qCDebug(lcMainWindow) << "六自由度外部按键忽略：当前模式未确定，按键○" << keyNumber;
            }
            return;
        }

        if (!pressed) {
            writeToMainDevice(613, 0);
            return;
        }

        const int groupIndex = (keyNumber + 1) / 2;       // ○1/2->1 ... ○11/12->6
        const int signedCommand = (keyNumber % 2 == 1) ? -groupIndex : groupIndex;
        const quint16 encoded = static_cast<quint16>(signedCommand); // 负数按补码写入
        writeToMainDevice(613, static_cast<int>(encoded));

        qCDebug(lcMainWindow) << "六自由度外部按键○" << keyNumber
                              << "-> 地址613写入" << signedCommand
                              << "(补码:" << encoded << ")";
        return;
    }

    // 除第1页（机械臂）、第4页（六自由度）以及第5页（AGV控制）外，其他页面外部按键统一不响应。
    bool isAgvPage = (currentPage == 4);
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

        if (keyNumber >= 1 && keyNumber <= 8) {
            if (m_stepModeEnabled) {
                const int axisIndex = (keyNumber - 1) / 2;   // 0~3
                const bool isOddKey = (keyNumber % 2) == 1;  // 奇数键为反向
                const int keyMappedTargetReg = 500 + axisIndex;

                if (!pressed) {
                    // 需求：步进模式由外部键按下触发，释放不再回写514。
                    m_robotExternalKeyPressed[keyNumber] = false;
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
                ui->statusBar->showMessage(
                    QString("步进触发：按键○%1，目标%2，步进值%3")
                        .arg(keyNumber)
                        .arg(targetName)
                        .arg(stepValue, 0, 'f', 3),
                    2000);
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

    if (m_stepModeEnabled) {
        if (!pressed) {
            return;
        }

        if (!m_stepValueEdit) {
            qCDebug(lcMainWindow) << "首页○9(步进)：未找到 lineEdit_StepValue，仅写 bit4/bit5";
            writeAGVRegisterBits(0, { qMakePair(4, true) }, QStringLiteral("○9步进按下(1)：寄存器0 bit4=1"));
            writeAGVRegisterBits(0, { qMakePair(5, true) }, QStringLiteral("○9步进按下(3)：寄存器0 bit5=1"));
            appendAgvExternalKeyRecord(keyNumber, pressed);
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
        appendAgvExternalKeyRecord(keyNumber, pressed);
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
        isFeatureEnabled("tcp_transmission", "tcp.remote_simulator"));
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
        ui->TBtn_Interlocking->setText(QStringLiteral("--"));
        return;
    }
    quint16 v = 0;
    const int addr = ModbusWriteGate::interlockRegisterAddress();
    if (!mgr->readSingleRegister(addr, v)) {
        ui->TBtn_Interlocking->setText(QStringLiteral("--"));
        return;
    }
    ui->TBtn_Interlocking->setText(v == 1 ? QStringLiteral("上方示教器") : QStringLiteral("下方示教器"));
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

    const int addr = ModbusWriteGate::interlockRegisterAddress();
    quint16 valueToWrite = 0;
    QString nextLabel;
    if (label == upper) {
        valueToWrite = 1;
        nextLabel = lower;
    } else {
        valueToWrite = 0;
        nextLabel = upper;
    }

    if (!mgr->writeSingleRegister(addr, valueToWrite)) {
        showNotification(QStringLiteral("写入联锁寄存器失败"));
        return;
    }
    ui->TBtn_Interlocking->setText(nextLabel);
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

static QMap<int, quint16> g_registerCache; 

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

    const bool allowUiStateSync = m_uiStateSyncEnabled;

    // 根据当前页面决定读取的寄存器：第一页(0)读125，第四页(3)读600
    bool shouldSyncStepMode = false;
    if (ui && ui->StackedWidget) {
        int currentPage = ui->StackedWidget->currentIndex();
        if ((currentPage == 0 && address == 125) || 
            (currentPage == 3 && address == 600)) {
            shouldSyncStepMode = true;
        }
    }

    if (allowUiStateSync && shouldSyncStepMode) {
        Q_UNUSED(value);
        syncStepModeUiByCurrentPage();
        updateStepTargetButtonsState();
    }

    if (allowUiStateSync && address == 126 && ui && ui->TBtn_MoveMode) {
        if (value == 2) {
            m_moveModeUnknown = false;
            m_isJointMode = true;
            ui->TBtn_MoveMode->setText("关节模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("关节模式");
                moveModeLabel->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 11px;");
            }
        } else if (value == 1) {
            m_moveModeUnknown = false;
            m_isJointMode = false;
            ui->TBtn_MoveMode->setText("坐标模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("坐标模式");
                moveModeLabel->setStyleSheet("color: #ffaa00; font-weight: bold; font-size: 11px;");
            }
        } else {
            m_moveModeUnknown = true;
            ui->TBtn_MoveMode->setText("未选择模式");
            QLabel *moveModeLabel = ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarMoveModeLabel") : nullptr;
            if (moveModeLabel) {
                moveModeLabel->setText("运动未选择");
                moveModeLabel->setStyleSheet("color: #aaaaaa; font-weight: bold; font-size: 11px;");
            }
        }

        updateFunctionSwitchVisuals();
    }

    if (allowUiStateSync && address == 130) {
        if (TechSliderEdit *robotSpeedSlider = findChild<TechSliderEdit*>("TechSliderEdit_Robot_RobotSpeed")) {
            const QSignalBlocker blocker(robotSpeedSlider);
            const double clamped = qBound(robotSpeedSlider->minimum(), static_cast<double>(value), robotSpeedSlider->maximum());
            robotSpeedSlider->setValue(clamped);
        }
    }

    if (address == 134) {
        updateRobotTotalPower(value);
    }

    const QStringList targetLabels = {
        "robot_ArcGauge_J1Angle", "robot_ArcGauge_J2Height", "robot_ArcGauge_J3Length", "robot_ArcGauge_J4Angle"
    };

    // J1~J4 数值显示应始终跟随主寄存器数据，不受“控件状态同步”开关影响。
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
            updateSliderLabelValue(gaugeName, axisValue);
        }
    }

    // ============ 仅保留主设备150急停报警源 ============
    if (address == 150) {
        m_mainRegister150Shadow = value;
        m_mainRegister150Valid = true;
        const bool emergencyStop = (value == 1);
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
                record.controlName = "重量超载提示";
                record.controlType = "提示窗口";
                record.operation = weightOverload ? "提示触发" : "提示解除";
                record.oldValue = "";
                record.newValue = weightOverload ? "检测到重量超载" : "重量超载提示已解除";
                m_recorder->addRecord(record);
            }
            if (weightOverload) {
                showRobotWeightOverloadDialog();
            } else {
                hideRobotWeightOverloadDialog();
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
    }

    if (address == 102) {
        const bool positiveLimitReached = (((value >> 2) & 0x01) == 1);
        const bool negativeLimitReached = (((value >> 3) & 0x01) == 1);

        if (positiveLimitReached != m_robotPositiveLimit102Bit2Flag) {
            m_robotPositiveLimit102Bit2Flag = positiveLimitReached;
            if (positiveLimitReached) {
                showRobotLimitReachedDialog(QStringLiteral("正限位到达"));
            }
        }

        if (negativeLimitReached != m_robotNegativeLimit102Bit3Flag) {
            m_robotNegativeLimit102Bit3Flag = negativeLimitReached;
            if (negativeLimitReached) {
                showRobotLimitReachedDialog(QStringLiteral("负限位到达"));
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



    // ============ 新增：处理大六维力寄存器（612-623） ============
    if (address >= 612 && address <= 623) {
        // 调试输出：确认收到大六维力数据
         qCDebug(lcMainWindow) << "收到大六维力寄存器数据 - 地址:" << address << "值:" << value;
        
        // 定义地址到大六维力标签的映射
        static QMap<int, QString> bigForceAddressToLabelMap = {
            {612, "FX"}, {613, "FX"},
            {614, "FY"}, {615, "FY"},
            {616, "FZ"}, {617, "FZ"},
            {618, "MX"}, {619, "MX"},
            {620, "MY"}, {621, "MY"},
            {622, "MZ"}, {623, "MZ"}
        };

        if (bigForceAddressToLabelMap.contains(address)) {
            QString labelName = bigForceAddressToLabelMap[address];

            // 获取对应的寄存器对地址
            int highAddr, lowAddr;
            if (labelName == "FX") {
                highAddr = 612; lowAddr = 613;
            } else if (labelName == "FY") {
                highAddr = 614; lowAddr = 615;
            } else if (labelName == "FZ") {
                highAddr = 616; lowAddr = 617;
            } else if (labelName == "MX") {
                highAddr = 618; lowAddr = 619;
            } else if (labelName == "MY") {
                highAddr = 620; lowAddr = 621;
            } else {  // MZ
                highAddr = 622; lowAddr = 623;
            }

            // 更新寄存器缓存
            static QMap<int, quint16> bigForceRegisterCache;
            bigForceRegisterCache[address] = value;

            // 只有当两个寄存器都有值时才计算
            if (bigForceRegisterCache.contains(highAddr) && bigForceRegisterCache.contains(lowAddr)) {
                quint16 high = bigForceRegisterCache[highAddr];
                quint16 low = bigForceRegisterCache[lowAddr];

                float floatValue = registersToFloat(high, low);

                // 调试输出
                static int debugCount = 0;
                if (debugCount++ % 6 == 0) // 减少刷屏，每6次（大约一组）打印一次
                   qCDebug(lcMainWindow) << "【大" << labelName << "】" << "值:" << floatValue << " (Raw: " << high << "," << low << ")";

                // 更新标签显示
                updateBigForceLabel(labelName, floatValue);
            }
        }
    }

    // ============ 新增：处理小六维力寄存器（624-635） ============
    if (address >= 624 && address <= 635) {
        
        // 调试输出：确认收到小六维力数据
        // qCDebug(lcMainWindow) << "收到小六维力寄存器数据 - 地址:" << address << "值:" << value;

        // 定义地址到小六维力标签的映射
        static QMap<int, QString> smallForceAddressToLabelMap = {
            {624, "FX"}, {625, "FX"},
            {626, "FY"}, {627, "FY"},
            {628, "FZ"}, {629, "FZ"},
            {630, "MX"}, {631, "MX"},
            {632, "MY"}, {633, "MY"},
            {634, "MZ"}, {635, "MZ"}
        };

        if (smallForceAddressToLabelMap.contains(address)) {
            QString labelName = smallForceAddressToLabelMap[address];

            // 获取对应的寄存器对地址
            int highAddr, lowAddr;
            if (labelName == "FX") {
                highAddr = 624; lowAddr = 625;
            } else if (labelName == "FY") {
                highAddr = 626; lowAddr = 627;
            } else if (labelName == "FZ") {
                highAddr = 628; lowAddr = 629;
            } else if (labelName == "MX") {
                highAddr = 630; lowAddr = 631;
            } else if (labelName == "MY") {
                highAddr = 632; lowAddr = 633;
            } else {  // MZ
                highAddr = 634; lowAddr = 635;
            }

            // 更新寄存器缓存
            static QMap<int, quint16> smallForceRegisterCache;
            smallForceRegisterCache[address] = value;

            // 只有当两个寄存器都有值时才计算
            if (smallForceRegisterCache.contains(highAddr) && smallForceRegisterCache.contains(lowAddr)) {
                quint16 high = smallForceRegisterCache[highAddr];
                quint16 low = smallForceRegisterCache[lowAddr];

                float floatValue = registersToFloat(high, low);

                // 调试输出
                static int smallDebugCount = 0;
                if (smallDebugCount++ % 6 == 0)
                   qCDebug(lcMainWindow) << "【小" << labelName << "】"
                            << "高位(0x" << QString::number(high, 16).toUpper() << ")"
                            << "低位(0x" << QString::number(low, 16).toUpper() << ")"
                            << "值:" << floatValue;

                // 更新标签显示
                updateSmallForceLabel(labelName, floatValue);
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
}
// 配置所有TechSliderLabel的参数
void MainWindow::setupSliderLabelConfigs()
{
    qWarning() << "[配置中心] 正在初始化 J1-J4 地址映射...";
    m_sliderLabelConfigs.clear();

    // 所有四个控件都需要在三个页面中查找匹配
    QStringList allTargetPages = {"回转升降", "伸缩臂", "EOAT控制"};

    m_sliderLabelConfigs["robot_ArcGauge_J1Angle"] = {
        "悬臂组件当前角度:",           // labelText
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
        "升降组件当前高度:",           // labelText
        "mm",                // unit
        4400.0,              // minValue
        8000.0,              // maxValue
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
        "悬臂组件当前长度:",           // labelText
        "mm",                // unit
        3765.0,                 // minValue
        6805.0,              // maxValue
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
        "末端组件当前角度:",    // labelText (修改为末端组件)
        "°",                  // unit
        -90.0,              // minValue
        90.0,               // maxValue
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

                if (address == 100 && m_controlModeBtn) {
                    if (value == 1) {
                        m_controlMode = WIRELESS_MODE;
                        m_controlModeBtn->setText("遥控器控制");
                    } else if (value == 2) {
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

                if (address == 50) {
                    // 约定：bit13=0 表示避障开启，bit13=1 表示避障关闭
                    const bool oaEnabled = (((value >> 13) & 0x01) == 0);
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

                if (m_uiStateSyncEnabled && address == 153 && m_editAGV_MoveSpeed) {
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

                if (m_uiStateSyncEnabled && address == 154 && m_editAGV_Angle) {
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
                        QTimer::singleShot(0, this, &MainWindow::checkAlarmConditions);
                    }

                    handleAGVRegister51Alerts(value);

                    syncAGVParkingStateFromRegister51(value);
                }

                if (address == 155) {
                    syncAGVSteeringModeFromRegister155(value);
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
        agvHost = "192.168.1.70";
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
        agvIndicator->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
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
            m_agvModbusManager->readMultipleRegisters(155, 1);
            m_agvModbusManager->readMultipleRegisters(50, 1);
            m_agvModbusManager->readMultipleRegisters(51, 1);
            m_agvModbusManager->readMultipleRegisters(153, 2);
        });
    }
}

/**
 * @brief 处理 AGV Modbus 断开连接事件
 *
 * 更新 UI 中的连接状态显示并记录断开事件。
 */
void MainWindow::onAGVModbusDisconnected()
{
    qCDebug(lcMainWindow) << "AGV Modbus连接断开";
    ui->statusBar->showMessage("AGV Modbus连接断开", 3000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
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
        agvIndicator->setStyleSheet("color: #ffaa00; font-weight: bold; font-size: 10px;"); // 橙色表示错误/警告
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

    if (m_uiStateSyncEnabled && address == 153 && m_editAGV_MoveSpeed) {
        const QSignalBlocker blocker(m_editAGV_MoveSpeed);
        const double clamped = qBound(m_editAGV_MoveSpeed->minimum(), static_cast<double>(value), m_editAGV_MoveSpeed->maximum());
        m_editAGV_MoveSpeed->setValue(clamped);
    }

    if (m_uiStateSyncEnabled && address == 154 && m_editAGV_Angle) {
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

void MainWindow::syncAGVParkingStateFromRegister51(quint16 value)
{
    const bool bit3 = (((value >> 3) & 0x01) == 1);
    const bool bit4 = (((value >> 4) & 0x01) == 1);

    const bool parkingSwitchWaiting = property("parkingSwitchWaiting").toBool();
    const int pendingBit = property("parkingTargetBit").toInt();
    const bool pendingTargetReached = (pendingBit >= 0 && pendingBit <= 15)
                                          ? ((((value >> pendingBit) & 0x01) == 1))
                                          : false;

    if (parkingSwitchWaiting && !pendingTargetReached) {
        return;
    }

    if (bit3 != bit4) {
        const bool parkingEnabled = bit3 && !bit4;
        m_agvParkingEnabled = parkingEnabled;
        if (m_techBtnAGV_Park) {
            m_techBtnAGV_Park->setText(parkingEnabled ? "驻车开启" : "驻车关闭");
            m_techBtnAGV_Park->setPrimaryColor(parkingEnabled ? QColor("#00C8FF") : QColor("#7F8C8D"));
            m_techBtnAGV_Park->setGlowColor(parkingEnabled ? QColor(0, 200, 255, 180) : QColor(127, 140, 141, 100));
        }

        QLabel *parkLabel = ui && ui->statusBar
                                 ? ui->statusBar->findChild<QLabel*>("statusBarParkLabel")
                                 : nullptr;
        if (parkLabel) {
            parkLabel->setText(parkingEnabled ? "驻车开" : "驻车关");
            parkLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                         .arg(parkingEnabled ? "#00C8FF" : "#7F8C8D"));
        }
    }
}

void MainWindow::syncAGVSteeringModeFromRegister155(quint16 value)
{
    if (!m_steeringModeSelector) {
        return;
    }

    const SteeringMode mode = steeringModeFromRegisterValue(value);
    const QSignalBlocker blocker(m_steeringModeSelector);
    m_steeringModeSelector->setCurrentMode(mode);

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
    if (!m_agvModbusManager || !m_agvModbusManager->isConnected()) {
        if (!m_agvDisconnectedWarnedAddresses.contains(address)) {
            qWarning() << "AGV Modbus未连接，无法写入地址" << address;
            m_agvDisconnectedWarnedAddresses.insert(address);
        }
        return false;
    }

    m_agvDisconnectedWarnedAddresses.remove(address);

    if (!bypassWirelessWarning && m_controlMode == WIRELESS_MODE) {
        QDialog warnDialog(this);
        warnDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        warnDialog.setModal(true);

        QVBoxLayout *layout = new QVBoxLayout(&warnDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        QLabel *msgLabel = new QLabel("当前处于无线模式", &warnDialog);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);

        QPushButton *confirmBtn = new QPushButton("确认", &warnDialog);
        layout->addWidget(confirmBtn);
        connect(confirmBtn, &QPushButton::clicked, &warnDialog, &QDialog::accept);

        warnDialog.setStyleSheet(
            "QDialog {"
            "  background-color: rgba(30, 0, 0, 230);"
            "  border: 3px solid #FFFF00;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
            "  color: #FFFF00;"
            "  font-size: 18px;"
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

        warnDialog.setFixedSize(350, 120);
        warnDialog.exec();
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
                                      const QString &scene)
{
    if (address < 0 || address > 65535) {
        qWarning() << "[AGV按位写入] 非法寄存器地址:" << address;
        return false;
    }

    if (bitUpdates.isEmpty()) {
        qWarning() << "[AGV按位写入] 未提供任何位更新，已拒绝写入";
        return false;
    }

    if (m_controlMode == WIRELESS_MODE) {
        QDialog warnDialog(this);
        warnDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        warnDialog.setModal(true);

        QVBoxLayout *layout = new QVBoxLayout(&warnDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        QLabel *msgLabel = new QLabel("当前处于无线模式", &warnDialog);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);

        QPushButton *confirmBtn = new QPushButton("确认", &warnDialog);
        layout->addWidget(confirmBtn);
        connect(confirmBtn, &QPushButton::clicked, &warnDialog, &QDialog::accept);

        warnDialog.setStyleSheet(
            "QDialog {"
            "  background-color: rgba(30, 0, 0, 230);"
            "  border: 3px solid #FFFF00;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
            "  color: #FFFF00;"
            "  font-size: 18px;"
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

        warnDialog.setFixedSize(350, 120);
        warnDialog.exec();
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







/**
 * @brief 向主设备写入单个寄存器
 * @param address 寄存器地址
 * @param value 要写入的数值
 */
void MainWindow::writeToMainDevice(int address, int value)
{
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
    // 切换控制模式
    if (m_controlMode == WIRED_MODE) {
        m_controlMode = WIRELESS_MODE;
        m_controlModeBtn->setText("遥控器控制");

        // 无线控制 -> AGV设备500写1
        writeToAGVDevice(500, 1);

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

        // 有线控制 -> AGV设备500写2
        writeToAGVDevice(500, 2);

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

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "TBtn_ControlMode";
    record.controlType = "QToolButton";
    record.operation = "mode_switch";
    record.oldValue = (m_controlMode == WIRED_MODE) ? "无线控制" : "有线控制";
    record.newValue = (m_controlMode == WIRED_MODE) ? "有线控制" : "无线控制";
    m_recorder->addRecord(record);
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
        m_techBtnAGV_Park->setText("驻车关闭");
        m_techBtnAGV_Park->setPrimaryColor(QColor("#7F8C8D"));
        m_techBtnAGV_Park->setGlowColor(QColor(127, 140, 141, 100));

        connect(m_techBtnAGV_Park, &TechPushButton::clicked,
            this, &MainWindow::onAGVParkBtnClicked,
            Qt::UniqueConnection);

        qCDebug(lcMainWindow) << "AGV驻车按钮初始化完成";
    } else {
        qWarning() << "未找到techBtn_AGV_Park按钮";
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
        // 设置参数 - 值域 0-100 mm/s
        m_editAGV_MoveSpeed->setLabelText("全向平台速度");
        m_editAGV_MoveSpeed->setRange(0, 100);
        m_editAGV_MoveSpeed->setValue(0);
        m_editAGV_MoveSpeed->setSuffix("mm/s");
        m_editAGV_MoveSpeed->setPrecision(0);

        // 连接值变化信号
        connect(m_editAGV_MoveSpeed, &TechSliderEdit::valueChanged,
                this, &MainWindow::onAGVMoveSpeedChanged);

        qCDebug(lcMainWindow) << "AGV运动速度控件初始化完成，范围:0-100，初始值:0";
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
        // 设置参数
        m_editAGV_Angle->setLabelText("底盘当前角度");
        m_editAGV_Angle->setRange(-25, 25);
        m_editAGV_Angle->setValue(0);
        m_editAGV_Angle->setSuffix("°");
        m_editAGV_Angle->setPrecision(0);  // 改为0，只显示整数
        m_editAGV_Angle->setPresetButtonsVisible(false); // 禁用预设按钮

        // 连接值变化信号
        connect(m_editAGV_Angle, &TechSliderEdit::valueChanged,
                this, &MainWindow::onAGVAngleChanged);

        qCDebug(lcMainWindow) << "AGV转向角度控件初始化完成，范围:-25-25，初始值:0，精度:整数";
    } else {
        qWarning() << "未找到SEdit_AGV_Angle控件";
    }
}

// AGV避障开关按钮点击槽函数
void MainWindow::onAGVOABtnClicked()
{
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

    const bool writeOk = writeAGVRegisterBits(0,
                                              {
                                                  qMakePair(1, targetOaEnabled),
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
        qWarning() << "AGV避障切换失败：地址0写入请求发送失败，目标bit1=" << (targetOaEnabled ? 1 : 0);
        setProperty("oaSwitchBusy", false);
        return;
    }

    if (targetOaEnabled) {
        qCDebug(lcMainWindow) << "AGV避障开启，地址0按位更新(bit1=1)";
        ui->statusBar->showMessage("AGV避障开启", 2000);
    } else {
        qCDebug(lcMainWindow) << "AGV避障关闭，地址0按位更新(bit1=0)";
        ui->statusBar->showMessage("AGV避障关闭", 2000);
    }

    QLabel *oaLabel = ui && ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarOaLabel") : nullptr;
    if (oaLabel) {
        oaLabel->setText(m_agvOaEnabled ? "避障开" : "避障关");
        oaLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                   .arg(m_agvOaEnabled ? "#00C8FF" : "#7F8C8D"));
    }

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "techBtn_AGV_OA";
    record.controlType = "TechPushButton";
    record.operation = "oa_mode_changed";
    record.oldValue = m_agvOaEnabled ? "避障关闭" : "避障开启";
    record.newValue = m_agvOaEnabled ? "避障开启" : "避障关闭";
    m_recorder->addRecord(record);

    // 写后做一次回读确认，目标设备偶发丢响应时自动补发一次。
    QTimer::singleShot(250, this, [this]() {
        if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
            m_agvModbusManager->readMultipleRegisters(0, 1);
        }
    });

    QTimer::singleShot(650, this, [this, targetOaEnabled]() {
        if (property("oaTargetEnabled").toBool() != targetOaEnabled) {
            // 已有更新的目标请求。
            return;
        }

        const quint16 reg0 = m_agvRegisterShadow.value(0, 0);
        const bool actualOaEnabled = (((reg0 >> 1) & 0x01) == 1);
        if (actualOaEnabled == targetOaEnabled) {
            setProperty("oaSwitchBusy", false);
            return;
        }

        qWarning() << "OA回读不一致，准备自动重试。目标bit1=" << (targetOaEnabled ? 1 : 0)
                   << "实际寄存器0=" << reg0;

        const bool retryOk = writeAGVRegisterBits(0,
                                                  {
                                                      qMakePair(1, targetOaEnabled),
                                                  },
                                                  targetOaEnabled ? "OA开启自动重试" : "OA关闭自动重试");
        if (!retryOk) {
            qWarning() << "OA自动重试发送失败";
            ui->statusBar->showMessage("AGV避障自动重试失败，请检查网络/控制器状态", 4000);
            setProperty("oaSwitchBusy", false);
            return;
        }

        if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
            m_agvModbusManager->readMultipleRegisters(0, 1);
        }

        QTimer::singleShot(350, this, [this, targetOaEnabled]() {
            if (property("oaTargetEnabled").toBool() != targetOaEnabled) {
                return;
            }
            const quint16 reg0AfterRetry = m_agvRegisterShadow.value(0, 0);
            const bool actualAfterRetry = (((reg0AfterRetry >> 1) & 0x01) == 1);
            if (actualAfterRetry != targetOaEnabled) {
                qWarning() << "OA自动重试后仍未达到目标。目标bit1=" << (targetOaEnabled ? 1 : 0)
                           << "当前寄存器0=" << reg0AfterRetry;
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

    const bool oldParkingEnabled = m_agvParkingEnabled;
    const bool targetParkingEnabled = !oldParkingEnabled;
    const int targetWaitBit = targetParkingEnabled ? 3 : 4;
    const bool writeOk = targetParkingEnabled
                             ? writeAGVRegisterBits(0,
                                                    {
                                                        qMakePair(9, true),
                                                        qMakePair(10, false),
                                                    },
                                                    "驻车开启")
                             : writeAGVRegisterBits(0,
                                                    {
                                                        qMakePair(9, false),
                                                        qMakePair(10, true),
                                                    },
                                                    "驻车关闭");

    if (!writeOk) {
        m_agvParkingEnabled = oldParkingEnabled;
        if (m_techBtnAGV_Park) {
            m_techBtnAGV_Park->setText(oldParkingEnabled ? "驻车开启" : "驻车关闭");
            m_techBtnAGV_Park->setPrimaryColor(oldParkingEnabled ? QColor("#00C8FF") : QColor("#7F8C8D"));
            m_techBtnAGV_Park->setGlowColor(oldParkingEnabled ? QColor(0, 200, 255, 180)
                                                              : QColor(127, 140, 141, 100));
        }

        ui->statusBar->showMessage("AGV驻车指令发送失败", 3000);
        OperationRecord failRecord;
        failRecord.timestamp = QDateTime::currentDateTime();
        failRecord.pageName = "AGV控制";
        failRecord.controlName = "techBtn_AGV_Park";
        failRecord.controlType = "TechPushButton";
        failRecord.operation = "parking_mode_write_failed";
        failRecord.oldValue = oldParkingEnabled ? "驻车开启" : "驻车关闭";
        failRecord.newValue = "指令发送失败，状态保持不变";
        m_recorder->addRecord(failRecord);
        return;
    }

    m_agvParkingEnabled = targetParkingEnabled;

    if (targetParkingEnabled) {
        m_techBtnAGV_Park->setText("驻车开启");
        m_techBtnAGV_Park->setPrimaryColor(QColor("#00C8FF"));
        m_techBtnAGV_Park->setGlowColor(QColor(0, 200, 255, 180));
        ui->statusBar->showMessage("AGV驻车开启", 2000);
    } else {
        m_techBtnAGV_Park->setText("驻车关闭");
        m_techBtnAGV_Park->setPrimaryColor(QColor("#7F8C8D"));
        m_techBtnAGV_Park->setGlowColor(QColor(127, 140, 141, 100));
        ui->statusBar->showMessage("AGV驻车关闭", 2000);
    }

    QLabel *parkLabel = ui && ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarParkLabel") : nullptr;
    if (parkLabel) {
        parkLabel->setText(targetParkingEnabled ? "驻车开" : "驻车关");
        parkLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                     .arg(targetParkingEnabled ? "#00C8FF" : "#7F8C8D"));
    }

    OperationRecord modeRecord;
    modeRecord.timestamp = QDateTime::currentDateTime();
    modeRecord.pageName = "AGV控制";
    modeRecord.controlName = "techBtn_AGV_Park";
    modeRecord.controlType = "TechPushButton";
    modeRecord.operation = "parking_mode_changed";
    modeRecord.oldValue = oldParkingEnabled ? "驻车开启" : "驻车关闭";
    modeRecord.newValue = targetParkingEnabled ? "驻车模式已开启" : "驻车模式已关闭";
    m_recorder->addRecord(modeRecord);

    // 与底盘模式切换一致：驻车切换期间显示等待提示，直到51寄存器目标位为1。
    if (QTimer *oldTimer = findChild<QTimer*>("parkingSwitchWaitTimer")) {
        oldTimer->stop();
        oldTimer->deleteLater();
    }

    setProperty("parkingSwitchWaiting", true);
    setProperty("parkingTargetBit", targetWaitBit);
    setProperty("parkingTargetEnabled", targetParkingEnabled);

    m_isSteeringAlarmActive = true;
    showAlarm("正在切换驻车模式", "#FFFF00", false);

    auto *parkingWaitTimer = new QTimer(this);
    parkingWaitTimer->setObjectName("parkingSwitchWaitTimer");
    parkingWaitTimer->setInterval(300);
    const qint64 parkingSwitchBeginMs = QDateTime::currentMSecsSinceEpoch();

    connect(parkingWaitTimer, &QTimer::timeout, this,
            [this, parkingWaitTimer, targetWaitBit, targetParkingEnabled, parkingSwitchBeginMs]() {
                if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
                    m_agvModbusManager->readMultipleRegisters(51, 1);
                }

                if (m_agvRegisterShadow.contains(51)) {
                    const quint16 reg51 = m_agvRegisterShadow.value(51);
                    const bool targetBitSet = (((reg51 >> targetWaitBit) & 0x01) == 1);
                    if (targetBitSet) {
                        parkingWaitTimer->stop();
                        parkingWaitTimer->deleteLater();
                        setProperty("parkingSwitchWaiting", false);
                        setProperty("parkingTargetBit", -1);
                        m_isSteeringAlarmActive = false;
                        hideAlarm();

                        OperationRecord okRecord;
                        okRecord.timestamp = QDateTime::currentDateTime();
                        okRecord.pageName = "AGV控制";
                        okRecord.controlName = "techBtn_AGV_Park";
                        okRecord.controlType = "TechPushButton";
                        okRecord.operation = "parking_mode_confirmed";
                        okRecord.oldValue = "";
                        okRecord.newValue = targetParkingEnabled ? "驻车模式已开启" : "驻车模式已关闭";
                        m_recorder->addRecord(okRecord);
                        return;
                    }
                }

                if (QDateTime::currentMSecsSinceEpoch() - parkingSwitchBeginMs >= 90000) {
                    parkingWaitTimer->stop();
                    parkingWaitTimer->deleteLater();
                    setProperty("parkingSwitchWaiting", false);
                    setProperty("parkingTargetBit", -1);
                    m_isSteeringAlarmActive = false;
                    hideAlarm();

                    OperationRecord timeoutRecord;
                    timeoutRecord.timestamp = QDateTime::currentDateTime();
                    timeoutRecord.pageName = "AGV控制";
                    timeoutRecord.controlName = "techBtn_AGV_Park";
                    timeoutRecord.controlType = "TechPushButton";
                    timeoutRecord.operation = "parking_switch_timeout";
                    timeoutRecord.oldValue = QString("等待寄存器51 bit%1=1").arg(targetWaitBit);
                    if (m_agvRegisterShadow.contains(51)) {
                        timeoutRecord.newValue = QString("90秒超时，读值=%1，期望状态=%2")
                                                     .arg(m_agvRegisterShadow.value(51))
                                                     .arg(targetParkingEnabled ? "驻车开启" : "驻车关闭");
                    } else {
                        timeoutRecord.newValue = QString("90秒超时，未读到寄存器51，期望状态=%1")
                                                     .arg(targetParkingEnabled ? "驻车开启" : "驻车关闭");
                    }
                    m_recorder->addRecord(timeoutRecord);
                }
            });
    parkingWaitTimer->start();
}

// AGV运动速度变化槽函数
void MainWindow::onAGVMoveSpeedChanged(double value)
{
    const int intValue = qBound(0, static_cast<int>(qRound(value)), 100);

    // 按需求直接写入地址3（单位:mm/s）
    // 速度控件在遥控器控制下也允许直接下发，不触发无线模式门禁弹窗。
    writeToAGVDevice(3, intValue, true);

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "SEdit_AGV_MoveSpeed";
    record.controlType = "TechSliderEdit";
    record.operation = "move_speed_changed";
    record.oldValue = "";
    record.newValue = QString("%1 mm/s").arg(intValue);
    m_recorder->addRecord(record);
}

// AGV转向角度变化槽函数
void MainWindow::onAGVAngleChanged(double value)
{
    const qint16 signedValue = static_cast<qint16>(qBound(-25, static_cast<int>(qRound(value)), 25));
    const quint16 rawUintValue = static_cast<quint16>(signedValue);

    // 地址4为UINT寄存器：通过qint16->quint16显式转换，负数按16位补码发送。
    writeToAGVDevice(4, static_cast<int>(signedValue));

    qCDebug(lcMainWindow) << "AGV转向角度:" << value
                          << "°，地址4写入(有符号):" << signedValue
                          << "原始UINT:" << rawUintValue;

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "SEdit_AGV_Angle";
    record.controlType = "TechSliderEdit";
    record.operation = "angle_changed";
    record.oldValue = "";
    record.newValue = QString::number(value);
    m_recorder->addRecord(record);
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

    if (m_stepModeEnabled) {
        if (!pressed) {
            return;
        }

        if (!m_stepValueEdit) {
            qCDebug(lcMainWindow) << "首页○10(步进)：未找到 lineEdit_StepValue，仅写 bit4/bit5";
            writeAGVRegisterBits(0, { qMakePair(4, true) }, QStringLiteral("○10步进按下(1)：寄存器0 bit4=1"));
            writeAGVRegisterBits(0, { qMakePair(5, true) }, QStringLiteral("○10步进按下(3)：寄存器0 bit5=1"));
            appendAgvExternalKeyRecord(keyNumber, pressed);
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
        appendAgvExternalKeyRecord(keyNumber, pressed);
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

    switch (steeringModeFromRegisterValue(m_agvRegisterShadow.value(155, 1))) {
    case STEER_FRONT_BACK: return "前后轮转向";
    case STEER_FRONT_ONLY: return "前轮转向";
    case STEER_PARALLEL: return "平移模式";
    case STEER_LATERAL: return "横向移动";
    case STEER_ROTATE: return "原地旋转";
    default: return "未知模式";
    }
}

void MainWindow::appendAgvExternalKeyRecord(int keyNumber, bool pressed)
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
    record.newValue = pressed
                          ? QString("当前模式为%1，设置速度为%2，设置角度为%3，开始运动")
                                .arg(currentSteeringModeText())
                                .arg(speedValue, 0, 'f', 0)
                                .arg(angleValue, 0, 'f', 0)
                          : QString("运动完成，当前模式为%1，设置速度为%2，设置角度为%3")
                                .arg(currentSteeringModeText())
                                .arg(speedValue, 0, 'f', 0)
                                .arg(angleValue, 0, 'f', 0);
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

    // 向192.168.1.88的2地址写入对应值
    const bool steerWriteOk = writeToAGVDevice(2, modbusValue);

    QLabel *steeringLabel = ui && ui->statusBar ? ui->statusBar->findChild<QLabel*>("statusBarSteeringLabel") : nullptr;
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

        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "AGV控制";
            record.controlName = "底盘模式切换";
            record.controlType = "模式控制";
            record.operation = "切换开始";
            record.oldValue = "";
            record.newValue = QString("正在向模式 %1 切换").arg(static_cast<int>(mode));
            m_recorder->addRecord(record);
        }

        if (m_recorder) {
            OperationRecord record;
            record.timestamp = QDateTime::currentDateTime();
            record.pageName = "AGV控制";
            record.controlName = "底盘模式切换";
            record.controlType = "模式控制";
            record.operation = "切换开始";
            record.oldValue = "";
            record.newValue = QString("正在向模式 %1 切换").arg(static_cast<int>(mode));
            m_recorder->addRecord(record);
        }

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
                    const bool modeResolved = resolveSteeringModeFromStatus50(regValue, &currentMode, &currentModeText);
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

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "steeringModeSelector";
    record.controlType = "SteeringModeSelector";
    record.operation = "steering_mode_changed";
    record.oldValue = "";
    record.newValue = QString("%1 (值:%2)").arg(m_steeringModeSelector->modeText(mode)).arg(modbusValue);
    m_recorder->addRecord(record);

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
            label->setStyleSheet("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
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
            mainModbusStatusIndicator->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
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
        switch(m_currentUserRole) {
            case UserRole::Operator: color = "#aaaaaa"; text = "操作员"; break;
            case UserRole::Engineer: color = "#55aaff"; text = "工程师"; break;
            case UserRole::Admin: color = "#55ff55"; text = "管理员"; break;
            case UserRole::Manufacturer: color = "#ffaa00"; text = "厂家"; break;
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
        m_recorder->setTcpServer(WIN7_IP, WIN7_PORT);

        if (enabled) {
            qCDebug(lcMainWindow) << "启用TCP传输，服务器:" << WIN7_IP << ":" << WIN7_PORT;
            ui->statusBar->showMessage("TCP传输已启用，正在连接服务器...", 3000);
        } else {
            qCDebug(lcMainWindow) << "禁用TCP传输";
            ui->statusBar->showMessage("TCP传输已禁用", 3000);
        }
    }
}

void MainWindow::updateTcpServerHost(const QString &hostSuffix)
{
    if (m_recorder) {
        QString newIp = "192.168.1." + hostSuffix;
        m_recorder->setTcpServer(newIp, WIN7_PORT);
        qCDebug(lcMainWindow) << "更新TCP服务器IP为:" << newIp;
    }
}

void MainWindow::updateSimulatorHost(const QString &hostSuffix)
{
    QString newIp = "192.168.1." + hostSuffix;
    qCDebug(lcMainWindow) << "尝试更新模拟器IP为:" << newIp;

    // 如果当前处于远程模拟器模式，则重新连接以使新 IP 生效
    if (isFeatureEnabled("tcp_transmission", "tcp.remote_simulator")) {
        if (m_modbusManager) {
            m_modbusManager->disconnectFromDevice();
            MainModbusConnector::connectAndConfigure(
                m_modbusManager,
                MainModbusEndpoint{newIp, 5020},
                m_mainModbusPollIntervalMs,
                m_mainReconnectIntervalMs);
            qCDebug(lcMainWindow) << "[MainModbus] 已切换模拟器并重新连接:" << newIp << ":5020";
        }
        if (m_agvModbusManager) {
            m_agvModbusManager->disconnectFromDevice();
            m_agvModbusManager->connectToDevice(newIp, 5021);
            qCDebug(lcMainWindow) << "[AGVModbus] 已切换模拟器并重新连接:" << newIp << ":5021";
        }
    } else {
        qCDebug(lcMainWindow) << "当前未启用远程模拟器模式，仅打印新 IP 会在下次切换模式时生效";
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
        QMessageBox::warning(this, "警告", "请先启用TCP传输");
        return;
    }

    if (!m_recorder->isTcpConnected()) {
        QMessageBox::warning(this, "警告", "TCP连接未建立，无法发送记录");
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
            tcpStatusIndicator->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
            tcpStatusIndicator->setToolTip("TCP连接正常");
        } else {
            tcpStatusIndicator->setText("TCP: ●");
            tcpStatusIndicator->setStyleSheet("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
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
//六维力
// 在mainwindow.cpp中添加以下函数实现
void MainWindow::setupBigForceLabels()
{
    if (!isFeatureEnabled("force_sensor", "force.big_sensor")) {
        return;
    }

    // 查找六个大六维力标签
    m_labelBigFX = findChild<QLabel*>("labelBigFX");
    m_labelBigFY = findChild<QLabel*>("labelBigFY");
    m_labelBigFZ = findChild<QLabel*>("labelBigFZ");
    m_labelBigMX = findChild<QLabel*>("labelBigMX");
    m_labelBigMY = findChild<QLabel*>("labelBigMY");
    m_labelBigMZ = findChild<QLabel*>("labelBigMZ");

    // 创建标签名称到控件指针的映射
    if (m_labelBigFX) {
        m_bigForceLabels["FX"] = m_labelBigFX;
        m_labelBigFX->setText("原始值FX: 0.00");
        m_labelBigFX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigFX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigFY) {
        m_bigForceLabels["FY"] = m_labelBigFY;
        m_labelBigFY->setText("原始值FY: 0.00");
        m_labelBigFY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigFY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigFZ) {
        m_bigForceLabels["FZ"] = m_labelBigFZ;
        m_labelBigFZ->setText("原始值FZ: 0.00");
        m_labelBigFZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigFZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigMX) {
        m_bigForceLabels["MX"] = m_labelBigMX;
        m_labelBigMX->setText("原始值MX: 0.00");
        m_labelBigMX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigMX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigMY) {
        m_bigForceLabels["MY"] = m_labelBigMY;
        m_labelBigMY->setText("原始值MY: 0.00");
        m_labelBigMY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigMY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigMZ) {
        m_bigForceLabels["MZ"] = m_labelBigMZ;
        m_labelBigMZ->setText("原始值MZ: 0.00");
        m_labelBigMZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigMZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    qCDebug(lcMainWindow) << "大六维力标签初始化完成，找到" << m_bigForceLabels.size() << "个标签";
}
void MainWindow::setupSmallForceLabels()
{
    if (!isFeatureEnabled("force_sensor", "force.small_sensor")) {
        return;
    }

    // 查找六个小六维力标签
    m_labelSmallFX = findChild<QLabel*>("labelSmallFX");
    m_labelSmallFY = findChild<QLabel*>("labelSmallFY");
    m_labelSmallFZ = findChild<QLabel*>("labelSmallFZ");
    m_labelSmallMX = findChild<QLabel*>("labelSmallMX");
    m_labelSmallMY = findChild<QLabel*>("labelSmallMY");
    m_labelSmallMZ = findChild<QLabel*>("labelSmallMZ");

    // 创建标签名称到控件指针的映射
    if (m_labelSmallFX) {
        m_smallForceLabels["FX"] = m_labelSmallFX;
        m_labelSmallFX->setText("原始值FX: 0.00");
        m_labelSmallFX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallFX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallFY) {
        m_smallForceLabels["FY"] = m_labelSmallFY;
        m_labelSmallFY->setText("原始值FY: 0.00");
        m_labelSmallFY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallFY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallFZ) {
        m_smallForceLabels["FZ"] = m_labelSmallFZ;
        m_labelSmallFZ->setText("原始值FZ: 0.00");
        m_labelSmallFZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallFZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallMX) {
        m_smallForceLabels["MX"] = m_labelSmallMX;
        m_labelSmallMX->setText("原始值MX: 0.00");
        m_labelSmallMX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallMX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallMY) {
        m_smallForceLabels["MY"] = m_labelSmallMY;
        m_labelSmallMY->setText("原始值MY: 0.00");
        m_labelSmallMY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallMY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallMZ) {
        m_smallForceLabels["MZ"] = m_labelSmallMZ;
        m_labelSmallMZ->setText("原始值MZ: 0.00");
        m_labelSmallMZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallMZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    qCDebug(lcMainWindow) << "小六维力标签初始化完成，找到" << m_smallForceLabels.size() << "个标签";
}


void MainWindow::setupForceReading()
{
    // 已根据要求删除六维力传感器轮询功能，以减少主 Modbus 队列负载
    qCDebug(lcMainWindow) << "六维力传感器轮询功能已禁用";
    return;
}

void MainWindow::setupBigForceReading()
{
    // 立即读取一次
    readBigForceRegisters();

    // 每500毫秒读取一次（与原有浮点数读取保持相同频率）
    QTimer::singleShot(500, this, [this]() {
        if (m_modbusManager && m_modbusManager->isConnected()) {
            readBigForceRegisters();
            // 继续定时读取
            QTimer::singleShot(500, this, [this]() { setupBigForceReading(); });
        } else {
            // Modbus未连接，等待2秒后重试
            QTimer::singleShot(2000, this, [this]() { setupBigForceReading(); });
        }
    });
}
void MainWindow::readBigForceRegisters()
{
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        qCDebug(lcMainWindow) << "Modbus未连接，无法读取大六维力数据";
        return;
    }

    // 读取612-623地址（12个寄存器，6个浮点数）
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 612, 12);
}
void MainWindow::readSmallForceRegisters()
{
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        qCDebug(lcMainWindow) << "Modbus未连接，无法读取小六维力数据";
        return;
    }

    // 读取624-635地址（12个寄存器，6个浮点数）
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 624, 12);
}
void MainWindow::updateBigForceLabel(const QString& labelName, float value)
{
    if (!m_bigForceLabels.contains(labelName)) {
        return;
    }

    // 存储当前原始值
    m_bigForceCurrentValues[labelName] = value;

    QLabel* label = m_bigForceLabels[labelName];
    if (!label) {
        return;
    }

    // 根据去皮标志计算显示值
    float displayValue = value;
    QString prefix = "原始值";

    if (m_isForcePeeled && m_bigForceOffsets.contains(labelName)) {
        float offset = m_bigForceOffsets[labelName];
        displayValue = value - offset;  // 显示值 = 当前值 - 基准值
        prefix = "去皮值";

        // 调试输出
        static QMap<QString, int> debugCounters;
        int count = debugCounters.value(labelName, 0);
        if (count++ % 10 == 0) {  // 每10次输出一次，避免日志过多
            qCDebug(lcMainWindow) << "大" << labelName << ": 原始=" << value
                     << ", 基准=" << offset
                     << ", 显示=" << displayValue;
            debugCounters[labelName] = count;
        }
    }

    // 格式化显示（保留2位小数），保持左对齐
    QString displayText = QString("%1%2: %3")
        .arg(prefix)
        .arg(labelName)
        .arg(displayValue, 0, 'f', 2);

    label->setText(displayText);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 根据数值大小设置颜色（可选）
    if (m_isForcePeeled && qAbs(displayValue) > 10.0) {
        label->setStyleSheet("color: #ff5555; font-weight: bold;");
    } else {
        label->setStyleSheet("color: #ffffff;");
    }
}

void MainWindow::updateSmallForceLabel(const QString& labelName, float value)
{
    if (!m_smallForceLabels.contains(labelName)) {
        return;
    }

    // 存储当前原始值
    m_smallForceCurrentValues[labelName] = value;

    QLabel* label = m_smallForceLabels[labelName];
    if (!label) {
        return;
    }

    // 根据去皮标志计算显示值
    float displayValue = value;
    QString prefix = "原始值";

    if (m_isForcePeeled && m_smallForceOffsets.contains(labelName)) {
        float offset = m_smallForceOffsets[labelName];
        displayValue = value - offset;  // 显示值 = 当前值 - 基准值
        prefix = "去皮值";

        // 调试输出
        static QMap<QString, int> debugCounters;
        int count = debugCounters.value(labelName, 0);
        if (count++ % 10 == 0) {  // 每10次输出一次，避免日志过多
            qCDebug(lcMainWindow) << "小" << labelName << ": 原始=" << value
                     << ", 基准=" << offset
                     << ", 显示=" << displayValue;
            debugCounters[labelName] = count;
        }
    }

    // 格式化显示（保留2位小数），保持左对齐
    QString displayText = QString("%1%2: %3")
        .arg(prefix)
        .arg(labelName)
        .arg(displayValue, 0, 'f', 2);

    label->setText(displayText);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 根据数值大小设置颜色（可选）
    if (m_isForcePeeled && qAbs(displayValue) > 5.0) {
        label->setStyleSheet("color: #ff5555; font-weight: bold;");
    } else {
        label->setStyleSheet("color: #ffffff;");
    }
}

void MainWindow::setupForceDisplayModeButtons()
{
    if (!isBigFeatureEnabled("force_sensor")) {
        return;
    }

    m_btnBigForceControl = findChild<TechPushButton*>("Btn_bigForceControl");
    m_btnSmallForceControl = findChild<TechPushButton*>("Btn_smallForceControl");

    if (!m_btnBigForceControl || !m_btnSmallForceControl) {
        qWarning() << "未找到Btn_bigForceControl或Btn_smallForceControl按钮";
        return;
    }

    m_btnBigForceControl->enableClickAnimation(true);
    m_btnSmallForceControl->enableClickAnimation(true);
    m_btnBigForceControl->enableHoverAnimation(true);
    m_btnSmallForceControl->enableHoverAnimation(true);
    m_btnBigForceControl->setTextGlow(true);
    m_btnSmallForceControl->setTextGlow(true);

    m_btnBigForceControl->setButtonStyle(TechPushButton::StyleHolographic);
    m_btnSmallForceControl->setButtonStyle(TechPushButton::StyleHolographic);

    connect(m_btnBigForceControl, &TechPushButton::clicked, this, [this]() {
        setForceDisplayMode(ForceDisplayBig);
    });
    connect(m_btnSmallForceControl, &TechPushButton::clicked, this, [this]() {
        setForceDisplayMode(ForceDisplaySmall);
    });

    setForceDisplayMode(m_forceDisplayMode);
    qCDebug(lcMainWindow) << "大/小六维力模式按钮初始化完成";
}

void MainWindow::setForceDisplayMode(ForceDisplayMode mode)
{
    if (!m_btnBigForceControl || !m_btnSmallForceControl) {
        return;
    }

    m_forceDisplayMode = mode;

    const QColor activeColor(0, 200, 255);
    const QColor inactiveColor(80, 80, 100);
    const QColor inactiveTextColor(200, 200, 200);

    // 发送Modbus指令 (新增)
    if (mode == ForceDisplayBig) {
        writeToMainDevice(404, 0); // 大力模式：写0
        qCDebug(lcMainWindow) << "切换至大力传感器模式，地址404写入0";
    } else {
        writeToMainDevice(404, 1); // 小力模式：写1
        qCDebug(lcMainWindow) << "切换至小力传感器模式，地址404写入1";
    }

    // 先全部设为非激活
    m_btnBigForceControl->setPrimaryColor(inactiveColor);
    m_btnBigForceControl->setGlowColor(inactiveColor);
    m_btnBigForceControl->setTextColor(inactiveTextColor);
    m_btnBigForceControl->enablePulseEffect(false);

    m_btnSmallForceControl->setPrimaryColor(inactiveColor);
    m_btnSmallForceControl->setGlowColor(inactiveColor);
    m_btnSmallForceControl->setTextColor(inactiveTextColor);
    m_btnSmallForceControl->enablePulseEffect(false);

    // 激活当前模式
    if (mode == ForceDisplayBig) {
        m_btnBigForceControl->setPrimaryColor(activeColor);
        m_btnBigForceControl->setGlowColor(activeColor.lighter(150));
        m_btnBigForceControl->setTextColor(Qt::white);
        // m_btnBigForceControl->enablePulseEffect(true);
    } else {
        m_btnSmallForceControl->setPrimaryColor(activeColor);
        m_btnSmallForceControl->setGlowColor(activeColor.lighter(150));
        m_btnSmallForceControl->setTextColor(Qt::white);
        // m_btnSmallForceControl->enablePulseEffect(true);
    }

    m_btnBigForceControl->update();
    m_btnSmallForceControl->update();
}

void MainWindow::setupForceClearButton()
{
    if (!isFeatureEnabled("force_sensor", "force.clear_zero")) {
        return;
    }

    m_btnForceClear = findChild<QPushButton*>("btn_ForceClear");
    if (m_btnForceClear) {
        // 连接按下和释放信号
        connect(m_btnForceClear, &QPushButton::pressed, this, &MainWindow::onForceClearPressed);
        connect(m_btnForceClear, &QPushButton::released, this, &MainWindow::onForceClearReleased);

        // 设置简化按钮样式
        m_btnForceClear->setStyleSheet(
            "QPushButton {"
            "    padding: 10px;"
            "    font-size: 14px;"
            "}"
            );

        qCDebug(lcMainWindow) << "去皮按钮初始化完成";
    } else {
        qWarning() << "未找到btn_ForceClear按钮";
    }
}

void MainWindow::onForceClearPressed()
{
    qCDebug(lcMainWindow) << "去皮按钮按下";

    // 获取当前页面名称（用于操作记录）
    QString pageName = getCurrentPageName();

    // 记录去皮前的值（用于操作记录）
    QMap<QString, float> beforeValues;

    // 1. 只记录大六维力的当前值作为基准值
    qCDebug(lcMainWindow) << "=== 记录大六维力基准值 ===";
    for (auto it = m_bigForceCurrentValues.begin(); it != m_bigForceCurrentValues.end(); ++it) {
        QString labelName = it.key();
        float currentValue = it.value();

        beforeValues[QString("大%1").arg(labelName)] = currentValue;
        m_bigForceOffsets[labelName] = currentValue;

        qCDebug(lcMainWindow) << "大六维力 " << labelName << " 基准值: " << currentValue;
    }

    // 2. 只记录小六维力的当前值作为基准值
    qCDebug(lcMainWindow) << "=== 记录小六维力基准值 ===";
    for (auto it = m_smallForceCurrentValues.begin(); it != m_smallForceCurrentValues.end(); ++it) {
        QString labelName = it.key();
        float currentValue = it.value();

        beforeValues[QString("小%1").arg(labelName)] = currentValue;
        m_smallForceOffsets[labelName] = currentValue;

        qCDebug(lcMainWindow) << "小六维力 " << labelName << " 基准值: " << currentValue;
    }

    // 3. 设置去皮标志（注意：不在按钮按下时立即更新显示）
    m_isForcePeeled = true;

    // 4. 给192.168.1.13设备的401地址写1
    writeToMainDevice(401, 1);

    // 5. 记录操作到历史记录
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = "去皮按钮";
    record.controlType = "ForceClear";
    record.operation = "force_clear_pressed";
    record.oldValue = "未去皮";

    // 构建详细的值字符串
    QString valuesStr = "大六维力基准值: ";
    for (auto it = beforeValues.begin(); it != beforeValues.end(); ++it) {
        if (it.key().startsWith("大")) {
            valuesStr += QString("%1=%2; ").arg(it.key()).arg(it.value(), 0, 'f', 2);
        }
    }
    valuesStr += "小六维力基准值: ";
    for (auto it = beforeValues.begin(); it != beforeValues.end(); ++it) {
        if (it.key().startsWith("小")) {
            valuesStr += QString("%1=%2; ").arg(it.key()).arg(it.value(), 0, 'f', 2);
        }
    }

    record.newValue = valuesStr;
    m_recorder->addRecord(record);

    // 6. 显示通知（但不要立即更新标签显示）
    showNotification("已记录当前值为基准值，后续显示将自动减去基准值");

    qCDebug(lcMainWindow) << "去皮操作完成：已记录基准值，去皮标志设为true";
}
void MainWindow::onForceClearReleased()
{
    qCDebug(lcMainWindow) << "去皮按钮释放";

    // 给192.168.1.13设备的401地址写0
    writeToMainDevice(401, 0);

    // 注意：我们不在这里清除去皮标志，因为去皮状态应该保持
    // 如果需要取消去皮，可以添加专门的取消去皮功能
}

void MainWindow::toggleForceControl()
{
    if (!isFeatureEnabled("motion_control", "motion.force_control")) {
        showNotification("力控功能已关闭");
        return;
    }

    // 切换状态
        m_forcecontrolMode = !m_forcecontrolMode;

        if (m_forcecontrolMode) {
            // 力控开启
            m_btnForceControl->setText(m_isForcePeeled ? "力控开启(去皮)" : "力控开启");
            m_btnForceControl->setPrimaryColor(QColor("#00C8FF"));
            m_btnForceControl->setGlowColor(QColor(0, 200, 255, 180));
            writeToMainDevice(400, 1);
            qCDebug(lcMainWindow) << "力控模式：开启，地址400写入1";
            ui->statusBar->showMessage("力控开启模式已启用", 2000);
        } else {
            // 力控关闭
            m_btnForceControl->setText(m_isForcePeeled ? "力控关闭(去皮)" : "力控关闭");
            m_btnForceControl->setPrimaryColor(QColor("#7F8C8D"));
            m_btnForceControl->setGlowColor(QColor(127, 140, 141, 100));
            writeToMainDevice(400, 0);
            qCDebug(lcMainWindow) << "力控模式：关闭，地址400写入0";
            ui->statusBar->showMessage("力控关闭模式已启用", 2000);
        }

    // 更新所有 TechSliderLabel 的力控状态
    QList<TechSliderLabel*> allSliderLabels = this->findChildren<TechSliderLabel*>();
    for (TechSliderLabel *label : allSliderLabels) {
        label->setForceControlMode(m_forcecontrolMode);
    }

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "btn_ForceControl";
    record.controlType = "TechPushButton";
    record.operation = "force_control_toggled";
    record.oldValue = !m_forcecontrolMode;
    record.newValue = m_forcecontrolMode;
    m_recorder->addRecord(record);

    // 立即检查报警条件（因为力控状态变化可能影响报警显示）
    checkAlarmConditions();

    qCDebug(lcMainWindow) << "力控按钮点击，新状态:" << (m_forcecontrolMode ? "开启" : "关闭");
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

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "TBtn_Stepmove";
    record.controlType = "QToolButton";
    record.operation = "step_mode_changed";
    record.oldValue = m_stepModeEnabled ? "点动模式" : "步进模式";
    record.newValue = m_stepModeEnabled ? "步进模式" : "点动模式";
    m_recorder->addRecord(record);
}

// 步进模式下使能按钮按下
void MainWindow::onEnableButtonPressedStepMode()
{
    qCDebug(lcMainWindow) << "步进模式下使能按钮按下";

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

    // 需求：仅在首页(索引0)且步进模式时，释放使能键后清空统一步进输入框。
    if (ui && ui->StackedWidget && m_stepModeEnabled && ui->StackedWidget->currentIndex() == 0 && m_stepValueEdit) {
        m_stepValueEdit->clear();
    }

    // 步进模式下使能按钮释放：不写514；统一输入框是否清空由上面的首页条件控制。

    if (m_stepValueEdit) {
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
    record.controlName = "StepMove_" + jointName;
    record.controlType = "StepMove";
    record.operation = "step_move_start";
    record.oldValue = "";
    QString msg;
    if (jointName.contains("J1")) {
        msg = QString("悬臂组件当前角度为%1°，开始步进%2°").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J2")) {
        msg = QString("升降组件当前高度为%1mm，开始步进%2mm").arg(currentValue, 0, 'f', 1).arg(stepValue);
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
    record.newValue = msg;

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

// 记录步进动作结束
void MainWindow::recordStepMoveEnd(const QString &jointName, double currentValue)
{
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "StepMove_" + jointName;
    record.controlType = "StepMove";
    record.operation = "step_move_end";
    record.oldValue = "";
    QString msg;
    if (jointName.contains("J1")) {
        msg = QString("悬臂组件当前角度为%1°，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J2")) {
        msg = QString("升降组件当前高度为%1mm，步进结束").arg(currentValue, 0, 'f', 1);
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





// 修改：原有的清除报警按钮函数
void MainWindow::on_TBtn_RemoveWarning_clicked()
{
    qCDebug(lcMainWindow) << "用户点击清除报警按钮";

    // 写入29地址清除报警
    writeToMainDevice(290, 1);
    
    // 清除力控超限错误，给403写0（写到192.168.1.13）
    ModbusThreadManager::instance()->writeSingleRegister(403, 0);

    // 清除所有报警状态
    if (m_emergencyStopAlarm) {
        m_emergencyStopAlarm = false;
        qCDebug(lcMainWindow) << "用户清除了紧急停止报警";
    }

    if (m_forceLimitAlarm) {
        m_forceLimitAlarm = false;
        qCDebug(lcMainWindow) << "用户清除了力控超限报警";
    }

    // 更新报警显示
    updateAlarmDisplay();

    // 显示通知
    showNotification("报警已清除");

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "TBtn_RemoveWarning";
    record.controlType = "QToolButton";
    record.operation = "clear_alarm";
    record.oldValue = "";
    record.newValue = "用户清除报警";
    m_recorder->addRecord(record);
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
    m_forceLimitAlarm = false;
    m_emergencyStopColumnFlag = false;
    m_emergencyStopChassisFlag = false;
    m_robotArmEmergency150Flag = false;
    m_agvChassisEmergency51Bit5Flag = false;
    m_agvStationOffline51Bit1Flag = false;
    m_agvDriveFault51Bit2Flag = false;
    m_agvBatteryLow51Bit0Flag = false;
    m_robotPositiveLimit102Bit2Flag = false;
    m_robotNegativeLimit102Bit3Flag = false;
    m_agvBatteryLowAcked = false;
    m_mainRegister150Valid = false;
    m_mainRegister150Shadow = 0;
    m_forceLimitFlag = false;

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

    // 2. 统一更新显示
    updateAlarmDisplay();

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
        m_alarmWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                                      Qt::WindowStaysOnTopHint | Qt::Tool);
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

        // 设置固定大小
        m_alarmWidget->setFixedSize(350, 120);

        qCDebug(lcMainWindow) << "报警窗口创建完成";
    }
    
    // 控制清除按钮的显示/隐藏
    QPushButton *clearBtn = m_alarmWidget->findChild<QPushButton*>("clearAlarmBtn");
    if (clearBtn) {
        clearBtn->setVisible(closable);
    }
    
    // 更新报警信息
    m_alarmLabel->setText(message);

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

    // 计算显示位置（屏幕右下角）
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

// 新增：更新报警显示
void MainWindow::updateAlarmDisplay()
{
    // 如果有任何报警处于激活状态，显示相应的报警
    if (m_emergencyStopAlarm) {
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
        showAlarm(alarmMessage, "#ff5555", false);
    } else {
        // 没有报警时隐藏窗口
        // 只有在没有转向切换报警时才隐藏
        if (!m_isSteeringAlarmActive) {
            hideAlarm();
        }
    }
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
            if (!m_agvBatteryLowAcked) {
                showAgvBatteryLowDialog();
            }
        } else {
            m_agvBatteryLowAcked = false;
            hideAgvBatteryLowDialog();
        }
    }
}

void MainWindow::showAgvStationOfflineAlarm()
{
    if (!m_agvStationOfflineAlarmWidget) {
        m_agvStationOfflineAlarmWidget = new QWidget(nullptr);
        m_agvStationOfflineAlarmWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                                                       Qt::WindowStaysOnTopHint | Qt::Tool);
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

    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect screenGeometry = screen->availableGeometry();
    const int x = screenGeometry.width() - m_agvStationOfflineAlarmWidget->width() - 40;
    const int y = 60;
    m_agvStationOfflineAlarmWidget->move(x, y);
    m_agvStationOfflineAlarmWidget->show();
    m_agvStationOfflineAlarmWidget->raise();
}

void MainWindow::hideAgvStationOfflineAlarm()
{
    if (m_agvStationOfflineAlarmWidget && m_agvStationOfflineAlarmWidget->isVisible()) {
        m_agvStationOfflineAlarmWidget->hide();
    }
}

void MainWindow::showAgvDriveFaultAlarm()
{
    if (!m_agvDriveFaultAlarmWidget) {
        m_agvDriveFaultAlarmWidget = new QWidget(nullptr);
        m_agvDriveFaultAlarmWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                                                   Qt::WindowStaysOnTopHint | Qt::Tool);
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

    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect screenGeometry = screen->availableGeometry();
    const int x = screenGeometry.width() - m_agvDriveFaultAlarmWidget->width() - 40;
    const int y = 200;
    m_agvDriveFaultAlarmWidget->move(x, y);
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
    if (!m_agvBatteryLowDialog) {
        m_agvBatteryLowDialog = new QDialog(this);
        m_agvBatteryLowDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_agvBatteryLowDialog->setModal(false);
        m_agvBatteryLowDialog->setObjectName("agvBatteryLowDialog");

        QVBoxLayout *layout = new QVBoxLayout(m_agvBatteryLowDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        QLabel *msgLabel = new QLabel("电池电量低，请充电", m_agvBatteryLowDialog);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);

        QPushButton *confirmBtn = new QPushButton("确认", m_agvBatteryLowDialog);
        layout->addWidget(confirmBtn, 0, Qt::AlignCenter);
        connect(confirmBtn, &QPushButton::clicked, this, [this]() {
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "提示系统";
                record.controlName = "低电量提示";
                record.controlType = "提示窗口";
                record.operation = "用户确认";
                record.oldValue = "电池电量低，请充电";
                record.newValue = "用户点击确认，窗口隐藏";
                m_recorder->addRecord(record);
            }
            m_agvBatteryLowAcked = true;
            hideAgvBatteryLowDialog();
        });

        m_agvBatteryLowDialog->setFixedSize(360, 150);
        m_agvBatteryLowDialog->setStyleSheet(
            "#agvBatteryLowDialog {"
            "  background-color: rgba(50, 35, 0, 235);"
            "  border: 3px solid #ffb000;"
            "  border-radius: 10px;"
            "}"
            "QLabel {"
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
            "}");
    }

    m_agvBatteryLowDialog->show();
    m_agvBatteryLowDialog->raise();
    m_agvBatteryLowDialog->activateWindow();
}

void MainWindow::hideAgvBatteryLowDialog()
{
    if (m_agvBatteryLowDialog && m_agvBatteryLowDialog->isVisible()) {
        m_agvBatteryLowDialog->hide();
    }
}

void MainWindow::showRobotOperationHintDialog(const QString &message)
{
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

    if (!m_robotOperationHintDialog) {
        m_robotOperationHintDialog = new QDialog(this);
        m_robotOperationHintDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_robotOperationHintDialog->setModal(false);
        m_robotOperationHintDialog->setObjectName("robotOperationHintDialog");

        QVBoxLayout *layout = new QVBoxLayout(m_robotOperationHintDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        QLabel *msgLabel = new QLabel(m_robotOperationHintDialog);
        msgLabel->setObjectName("robotOperationHintLabel");
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);

        QPushButton *confirmBtn = new QPushButton("确定", m_robotOperationHintDialog);
        layout->addWidget(confirmBtn, 0, Qt::AlignCenter);
        connect(confirmBtn, &QPushButton::clicked, this, [this]() {
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "提示系统";
                record.controlName = "外部按键操作提示";
                record.controlType = "提示窗口";
                record.operation = "用户确认";
                record.oldValue = "";
                record.newValue = "用户点击确定，提示窗口隐藏";
                m_recorder->addRecord(record);
            }
            hideRobotOperationHintDialog();
        });

        m_robotOperationHintDialog->setFixedSize(380, 150);
        m_robotOperationHintDialog->setStyleSheet(
            "#robotOperationHintDialog {"
            "  background-color: rgba(20, 30, 50, 235);"
            "  border: 3px solid #4da3ff;"
            "  border-radius: 10px;"
            "}"
            "#robotOperationHintLabel {"
            "  color: #8ec5ff;"
            "  font-size: 20px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}"
            "QPushButton {"
            "  background-color: #4da3ff;"
            "  color: #102030;"
            "  border: 2px solid #8ec5ff;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 90px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #8ec5ff;"
            "}");
    }

    if (QLabel *msgLabel = m_robotOperationHintDialog->findChild<QLabel*>("robotOperationHintLabel")) {
        msgLabel->setText(message);
    }

    m_robotOperationHintDialog->show();
    m_robotOperationHintDialog->raise();
    m_robotOperationHintDialog->activateWindow();
}

void MainWindow::hideRobotOperationHintDialog()
{
    if (m_robotOperationHintDialog && m_robotOperationHintDialog->isVisible()) {
        m_robotOperationHintDialog->hide();
    }
}

void MainWindow::showTeachingWriteGateDeniedDialog()
{
    if (!m_teachingWriteGateDeniedDialog) {
        m_teachingWriteGateDeniedDialog = new QDialog(this);
        m_teachingWriteGateDeniedDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_teachingWriteGateDeniedDialog->setModal(false);
        m_teachingWriteGateDeniedDialog->setObjectName("teachingWriteGateDeniedDialog");

        auto *layout = new QVBoxLayout(m_teachingWriteGateDeniedDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        auto *msgLabel = new QLabel(m_teachingWriteGateDeniedDialog);
        msgLabel->setObjectName("teachingWriteGateDeniedLabel");
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setWordWrap(true);
        msgLabel->setText(ModbusWriteGate::teachingGateUserDialogMessage());
        layout->addWidget(msgLabel);

        auto *confirmBtn = new QPushButton(QStringLiteral("确认"), m_teachingWriteGateDeniedDialog);
        layout->addWidget(confirmBtn, 0, Qt::AlignCenter);
        connect(confirmBtn, &QPushButton::clicked, this, [this]() {
            hideTeachingWriteGateDeniedDialog();
        });

        m_teachingWriteGateDeniedDialog->setFixedSize(380, 150);
        m_teachingWriteGateDeniedDialog->setStyleSheet(
            "#teachingWriteGateDeniedDialog {"
            "  background-color: rgba(20, 30, 50, 235);"
            "  border: 3px solid #4da3ff;"
            "  border-radius: 10px;"
            "}"
            "#teachingWriteGateDeniedLabel {"
            "  color: #8ec5ff;"
            "  font-size: 20px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}"
            "QPushButton {"
            "  background-color: #4da3ff;"
            "  color: #102030;"
            "  border: 2px solid #8ec5ff;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 90px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #8ec5ff;"
            "}");
    }

    if (auto *lbl = m_teachingWriteGateDeniedDialog->findChild<QLabel*>("teachingWriteGateDeniedLabel")) {
        lbl->setText(ModbusWriteGate::teachingGateUserDialogMessage());
    }
    m_teachingWriteGateDeniedDialog->show();
    m_teachingWriteGateDeniedDialog->raise();
    m_teachingWriteGateDeniedDialog->activateWindow();
}

void MainWindow::hideTeachingWriteGateDeniedDialog()
{
    if (m_teachingWriteGateDeniedDialog && m_teachingWriteGateDeniedDialog->isVisible()) {
        m_teachingWriteGateDeniedDialog->hide();
    }
}

void MainWindow::showRobotLimitReachedDialog(const QString &message)
{
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

    if (!m_robotLimitReachedDialog) {
        m_robotLimitReachedDialog = new QDialog(this);
        m_robotLimitReachedDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_robotLimitReachedDialog->setModal(false);
        m_robotLimitReachedDialog->setObjectName("robotLimitReachedDialog");

        QVBoxLayout *layout = new QVBoxLayout(m_robotLimitReachedDialog);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        m_robotLimitReachedLabel = new QLabel(m_robotLimitReachedDialog);
        m_robotLimitReachedLabel->setObjectName("robotLimitReachedLabel");
        m_robotLimitReachedLabel->setAlignment(Qt::AlignCenter);
        m_robotLimitReachedLabel->setWordWrap(true);
        layout->addWidget(m_robotLimitReachedLabel);

        QPushButton *confirmBtn = new QPushButton("确认", m_robotLimitReachedDialog);
        layout->addWidget(confirmBtn, 0, Qt::AlignCenter);
        connect(confirmBtn, &QPushButton::clicked, this, [this]() {
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "提示系统";
                record.controlName = "主控限位提示";
                record.controlType = "提示窗口";
                record.operation = "用户确认";
                record.oldValue = "";
                record.newValue = "用户点击确认，限位提示窗口隐藏";
                m_recorder->addRecord(record);
            }
            hideRobotLimitReachedDialog();
        });

        m_robotLimitReachedDialog->setFixedSize(360, 150);
        m_robotLimitReachedDialog->setStyleSheet(
            "#robotLimitReachedDialog {"
            "  background-color: rgba(45, 24, 0, 235);"
            "  border: 3px solid #ffaa00;"
            "  border-radius: 10px;"
            "}"
            "#robotLimitReachedLabel {"
            "  color: #ffd166;"
            "  font-size: 22px;"
            "  font-weight: bold;"
            "  background-color: transparent;"
            "}"
            "QPushButton {"
            "  background-color: #ffaa00;"
            "  color: #2b1800;"
            "  border: 2px solid #ffd166;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  min-width: 90px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #ffd166;"
            "}");
    }

    if (m_robotLimitReachedLabel) {
        m_robotLimitReachedLabel->setText(message);
    }

    m_robotLimitReachedDialog->show();
    m_robotLimitReachedDialog->raise();
    m_robotLimitReachedDialog->activateWindow();
}

void MainWindow::hideRobotLimitReachedDialog()
{
    if (m_robotLimitReachedDialog && m_robotLimitReachedDialog->isVisible()) {
        m_robotLimitReachedDialog->hide();
    }
}

void MainWindow::showRobotWeightOverloadDialog()
{
    if (!m_robotWeightOverloadWidget) {
        m_robotWeightOverloadWidget = new QWidget(nullptr);
        m_robotWeightOverloadWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                                                    Qt::WindowStaysOnTopHint | Qt::Tool);
        m_robotWeightOverloadWidget->setObjectName("robotWeightOverloadWidget");

        QVBoxLayout *layout = new QVBoxLayout(m_robotWeightOverloadWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(8);

        m_robotWeightOverloadLabel = new QLabel(m_robotWeightOverloadWidget);
        m_robotWeightOverloadLabel->setAlignment(Qt::AlignCenter);
        m_robotWeightOverloadLabel->setWordWrap(true);
        m_robotWeightOverloadLabel->setText(QStringLiteral("重量超载"));
        layout->addWidget(m_robotWeightOverloadLabel);

        m_robotWeightOverloadWidget->setFixedSize(360, 120);
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
            "}");
    }

    m_robotWeightOverloadWidget->show();
    m_robotWeightOverloadWidget->raise();
    m_robotWeightOverloadWidget->activateWindow();
}

void MainWindow::hideRobotWeightOverloadDialog()
{
    if (m_robotWeightOverloadWidget && m_robotWeightOverloadWidget->isVisible()) {
        m_robotWeightOverloadWidget->hide();
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
        qCDebug(lcMainWindow) << "测试2：测试力控超限报警...";
        showAlarm("手动测试报警 - 力控超限", "#ff8800");
    });

    // 等待6秒，测试报警条件检查
    QTimer::singleShot(6000, this, [this]() {
        qCDebug(lcMainWindow) << "测试3：通过设置标志位触发报警检查...";

        // 设置报警标志
        m_emergencyStopColumnFlag = true;
        m_emergencyStopChassisFlag = true;
        m_forceLimitFlag = true;
        m_forcecontrolMode = true;  // 模拟力控开启

        qCDebug(lcMainWindow) << "立柱急停标志:" << m_emergencyStopColumnFlag;
        qCDebug(lcMainWindow) << "底盘急停标志:" << m_emergencyStopChassisFlag;
        qCDebug(lcMainWindow) << "力控超限标志:" << m_forceLimitFlag;
        qCDebug(lcMainWindow) << "力控模式:" << m_forcecontrolMode;

        // 手动触发报警检查
        checkAlarmConditions();
    });

    // 等待9秒，清除报警
    QTimer::singleShot(9000, this, [this]() {
        qCDebug(lcMainWindow) << "测试4：清除报警...";

        // 清除报警标志
        m_emergencyStopColumnFlag = false;
        m_emergencyStopChassisFlag = false;
        m_forceLimitFlag = false;

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
