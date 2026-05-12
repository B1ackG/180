#include "featureswitchwidget.h"
#include "featureswitchmanager.h"
#include "techvirtualkeyboard.h"
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <QEvent>
#include <QSettings>

FeatureSwitchWidget::FeatureSwitchWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
    loadCurrentState();
    m_virtualKeyboard = new TechVirtualKeyboard(this);

    const QList<QLineEdit*> edits = findChildren<QLineEdit*>();
    for (QLineEdit *edit : edits) {
        edit->installEventFilter(this);
    }

    setWindowTitle("功能开关管理 (厂家权限)");
    
    // 设置深色调工业风格样式
    setStyleSheet(
        "QWidget { background-color: #1a1a2a; color: #00ffff; font-family: 'Microsoft YaHei UI'; }"
        "QGroupBox { border: 2px solid #00c8ff; border-radius: 10px; margin-top: 15px; font-weight: bold; padding: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 0 5px; }"
        "QCheckBox { spacing: 10px; padding: 5px; }"
        "QCheckBox::indicator { width: 24px; height: 24px; border: 2px solid #00c8ff; border-radius: 4px; }"
        "QCheckBox::indicator:checked { background-color: #00c8ff; }"
        "QPushButton { background-color: #004466; border: 1px solid #00c8ff; border-radius: 5px; padding: 8px 20px; color: white; }"
        "QPushButton:hover { background-color: #006699; }"
        "QScrollArea { border: none; background-color: transparent; }"
    );
    
    resize(980, 820);
}

bool FeatureSwitchWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(watched);
        if (lineEdit && lineEdit->isEnabled() && m_virtualKeyboard) {
            m_virtualKeyboard->setTargetLineEdit(lineEdit);
            m_virtualKeyboard->showAtWidget(lineEdit);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FeatureSwitchWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QLabel *title = new QLabel("<h1 style='color: #00ffff;'>系统功能控制台</h1>");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);

    // 大功能组
    QGroupBox *bigGroup = new QGroupBox("核心功能层 (Big Features)");
    QGridLayout *bigLayout = new QGridLayout(bigGroup);
    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();
    
    // 排序后展示，并为 key 提供中文描述（若有）
    QMap<QString, QString> desc;
    desc["startup_checks"] = "启动自检";
    desc["ui_navigation"] = "界面导航";
    desc["permission_system"] = "权限体系";
    desc["operation_records"] = "操作记录";
    desc["tcp_transmission"] = "TCP 上报";
    desc["modbus_main"] = "主控 Modbus";
    desc["modbus_agv"] = "AGV Modbus";
    desc["motion_control"] = "运动控制";
    desc["input_devices"] = "输入设备";
    desc["force_sensor"] = "力传感";
    desc["alarm_system"] = "报警系统";

    QStringList bigKeys = mgr->allBigFeatures().values();
    bigKeys.sort();

    for (int i = 0; i < bigKeys.size(); ++i) {
        const QString key = bigKeys.at(i);
        QString label = desc.contains(key) ? QString("%1 [%2]").arg(desc.value(key)).arg(key) : key;
        QCheckBox *cb = new QCheckBox(label);
        bigLayout->addWidget(cb, i / 2, i % 2);
        m_bigCheckboxes[key] = cb;
    }
    scrollLayout->addWidget(bigGroup);

    // 小功能项
    QGroupBox *smallGroup = new QGroupBox("子功能细项 (Small Features)");
    QGridLayout *smallLayout = new QGridLayout(smallGroup);
    
    QStringList smallKeys = mgr->allSmallFeatures().values();
    smallKeys.sort();
    
    // 小功能的中文说明映射
    QMap<QString, QString> sdesc;
    sdesc["startup.clear_servo_alarm"] = "启动清除伺服报警";
    sdesc["startup.write_registers"] = "启动写寄存器";
    sdesc["startup.log_report"] = "启动日志报告";
    sdesc["ui.styles"] = "界面样式";
    sdesc["ui.animations"] = "界面动画";
    sdesc["ui.virtual_keyboard"] = "虚拟键盘";
    sdesc["permission.admin_login"] = "管理员登录";
    sdesc["records.filter_export"] = "记录筛选与导出";
    sdesc["tcp.send_all"] = "TCP 全量发送";
    sdesc["tcp.local_simulator"] = "本机 TCP 模拟器 (127.0.0.1)";
    sdesc["tcp.remote_simulator"] = "远程 TCP 模拟器 (192.168.1.70)";
    sdesc["modbus_main.polling"] = "主控轮询";
    sdesc["modbus_main.float_reading"] = "浮点解析";
    sdesc["modbus_main.read_logs"] = "主设备 Modbus 读日志";
    sdesc["modbus_main.write_logs"] = "主设备 Modbus 写日志";
    sdesc["modbus_agv.read_logs"] = "AGV Modbus 读日志";
    sdesc["modbus_agv.write_logs"] = "AGV Modbus 写日志";
    sdesc["agv.fault_codes"] = "AGV 故障码";
    sdesc["agv.speed_gauge"] = "AGV 速度表";
    sdesc["motion.steering_mode"] = "转向模式";
    sdesc["motion.speed_mode"] = "速度模式";
    sdesc["motion.step_mode"] = "步进/点动";
    sdesc["motion.force_control"] = "力控参与运动";
    sdesc["input.matrix_key"] = "矩阵按键";
    sdesc["input.enable_button"] = "使能按钮";
    sdesc["force.big_sensor"] = "大力传感器";
    sdesc["force.small_sensor"] = "小力传感器";
    sdesc["force.clear_zero"] = "力传感器清零";
    sdesc["alarm.emergency_stop"] = "急停报警";
    sdesc["alarm.force_limit"] = "力控超限报警";
    sdesc["alarm.steering_switch"] = "转向模式切换报警";
    sdesc["alarm.popup"] = "报警弹窗显示";
    sdesc["alarm.status_logs"] = "报警状态周期日志";
    sdesc["debug.qdebug"] = "全局调试输出(qDebug)";

    const QSet<QString> logSwitchKeys = {
        "modbus_main.read_logs",
        "modbus_main.write_logs",
        "modbus_agv.read_logs",
        "modbus_agv.write_logs",
        "alarm.status_logs",
        "debug.qdebug"
    };

    int smallIndex = 0;
    for (int i = 0; i < smallKeys.size(); ++i) {
        const QString key = smallKeys.at(i);
        if (logSwitchKeys.contains(key)) {
            continue;
        }
        QString label = sdesc.contains(key) ? QString("%1 [%2]").arg(sdesc.value(key)).arg(key) : key;
        QCheckBox *cb = new QCheckBox(label);
        smallLayout->addWidget(cb, smallIndex / 2, smallIndex % 2);
        ++smallIndex;
        m_smallCheckboxes[key] = cb;

        // 互斥处理：本机模拟器和远程模拟器
        if (key == "tcp.local_simulator") {
            connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
                if (checked && m_smallCheckboxes.contains("tcp.remote_simulator")) {
                    m_smallCheckboxes["tcp.remote_simulator"]->setChecked(false);
                }
            });
        } else if (key == "tcp.remote_simulator") {
            connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
                if (checked && m_smallCheckboxes.contains("tcp.local_simulator")) {
                    m_smallCheckboxes["tcp.local_simulator"]->setChecked(false);
                }
            });
        }
    }
    scrollLayout->addWidget(smallGroup);

    QGroupBox *logGroup = new QGroupBox("日志类型开关 (Log Switches)");
    QGridLayout *logLayout = new QGridLayout(logGroup);
    QStringList logKeys = logSwitchKeys.values();
    logKeys.sort();
    for (int i = 0; i < logKeys.size(); ++i) {
        const QString key = logKeys.at(i);
        QString label = sdesc.contains(key) ? QString("%1 [%2]").arg(sdesc.value(key)).arg(key) : key;
        QCheckBox *cb = new QCheckBox(label);
        logLayout->addWidget(cb, i / 2, i % 2);
        m_smallCheckboxes[key] = cb;
    }
    scrollLayout->addWidget(logGroup);
    
    // 轮询参数配置组
    setupPollingUI(scrollLayout);
    
    // 滑块自定义范围配置组
    setupSliderLimitUI(scrollLayout);

    setupInclinometerThresholdUI(scrollLayout);

    scrollLayout->addStretch();

    scroll->setWidget(scrollContent);
    mainLayout->addWidget(scroll);

    // 底部控制按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnAll = new QPushButton("开启全部");
    connect(btnAll, &QPushButton::clicked, this, [this](){ onToggleAll(true); });

    QPushButton *btnNone = new QPushButton("关闭全部");
    connect(btnNone, &QPushButton::clicked, this, [this](){ onToggleAll(false); });

    QPushButton *btnReload = new QPushButton("撤销修改 (重载)");
    connect(btnReload, &QPushButton::clicked, this, &FeatureSwitchWidget::onReload);

    QPushButton *btnApply = new QPushButton("立即生效");
    btnApply->setStyleSheet("background-color: #2196F3; font-weight: bold; border-color: #ffffff;");
    connect(btnApply, &QPushButton::clicked, this, &FeatureSwitchWidget::onApply);

    QPushButton *btnSave = new QPushButton("保存并写入INI");
    btnSave->setStyleSheet("background-color: #4CAF50; font-weight: bold; border-color: #ffffff;");
    connect(btnSave, &QPushButton::clicked, this, &FeatureSwitchWidget::onSave);

    QPushButton *btnClose = new QPushButton("退出");
    btnClose->setStyleSheet("background-color: #f44336; font-weight: bold; border-color: #ffffff;");
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    btnLayout->addWidget(btnAll);
    btnLayout->addWidget(btnNone);
    btnLayout->addWidget(btnReload);
    btnLayout->addStretch();
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);
}

void FeatureSwitchWidget::loadCurrentState()
{
    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();
    for (auto it = m_bigCheckboxes.begin(); it != m_bigCheckboxes.end(); ++it) {
        it.value()->setChecked(mgr->isBigFeatureEnabled(it.key()));
    }
    for (auto it = m_smallCheckboxes.begin(); it != m_smallCheckboxes.end(); ++it) {
        it.value()->setChecked(mgr->isSmallFeatureEnabled(it.key()));
    }
    loadPollingState();
    loadSliderLimitState();
    loadInclinometerThresholdState();
}

void FeatureSwitchWidget::setupPollingUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *pollGroup = new QGroupBox("通信轮询参数 (Polling Settings)");
    QVBoxLayout *pollLayout = new QVBoxLayout(pollGroup);

    m_cbUiStateSync = new QCheckBox("启用控件状态同步");
    pollLayout->addWidget(m_cbUiStateSync);

    auto addPollItem = [&](const QString &label, QLineEdit *&edit) {
        QHBoxLayout *h = new QHBoxLayout();
        h->addWidget(new QLabel(label));
        edit = new QLineEdit();
        edit->setFixedWidth(150);
        edit->setStyleSheet("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px; padding: 3px;");
        edit->installEventFilter(this);
        h->addWidget(edit);
        h->addStretch();
        pollLayout->addLayout(h);
    };

    addPollItem("主控 Modbus 轮询 (ms):", m_editMainModbusPoll);
    addPollItem("控件状态同步轮询 (ms):", m_editMainUiPoll);
    addPollItem("设备状态轮询间隔 (ms, 0~84):", m_editMainDeviceStatusPoll);
    addPollItem("设备状态轮询起始地址 (192.168.1.13):", m_editMainDeviceStatusStart);
    addPollItem("设备状态轮询数量 (0~84默认85):", m_editMainDeviceStatusCount);
    addPollItem("模式同步轮询起始地址 (如125):", m_editMainControlSyncStart);
    addPollItem("模式同步轮询数量 (如6):", m_editMainControlSyncCount);
    addPollItem("主控 重连间隔 (ms):", m_editMainReconnect);
    addPollItem("AGV Modbus 轮询 (ms):", m_editAgvPoll);
    addPollItem("AGV 重连间隔 (ms):", m_editAgvReconnect);
    addPollItem("示教写权限设备号 (与主控8192相等才允许写.13/.88，默认0):", m_editTeachingWriteDeviceId);

    scrollLayout->addWidget(pollGroup);
}

void FeatureSwitchWidget::setupSliderLimitUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *limitGroup = new QGroupBox("参数范围自定义 (Parameter Limits)");
    QVBoxLayout *limitLayout = new QVBoxLayout(limitGroup);

    // 与 MainWindow::setupSliderLabelConfigs / m_arcGauges 的 key 保持一致。
    QStringList targetNames = {
        "robot_ArcGauge_J1Angle",
        "robot_ArcGauge_J2Height",
        "robot_ArcGauge_J3Length",
        "robot_ArcGauge_J4Angle",
        "robot_ArcGauge_SixAxis1",
        "robot_ArcGauge_SixAxis2",
        "robot_ArcGauge_SixAxis3",
        "robot_ArcGauge_SixAxis4",
        "robot_ArcGauge_SixAxis5",
        "robot_ArcGauge_SixAxis6"
    };
    QMap<QString, QString> itemLabels;
    itemLabels["robot_ArcGauge_J1Angle"] = "悬臂角度 (J1)";
    itemLabels["robot_ArcGauge_J2Height"] = "升降高度 (J2)";
    itemLabels["robot_ArcGauge_J3Length"] = "总伸展长度 (J3)";
    itemLabels["robot_ArcGauge_J4Angle"] = "末端角度 (J4)";
    itemLabels["robot_ArcGauge_SixAxis1"] = "六轴 1";
    itemLabels["robot_ArcGauge_SixAxis2"] = "六轴 2";
    itemLabels["robot_ArcGauge_SixAxis3"] = "六轴 3";
    itemLabels["robot_ArcGauge_SixAxis4"] = "六轴 4";
    itemLabels["robot_ArcGauge_SixAxis5"] = "六轴 5";
    itemLabels["robot_ArcGauge_SixAxis6"] = "六轴 6";

    for (const QString &name : targetNames) {
        QHBoxLayout *row = new QHBoxLayout();
        QString desc = itemLabels.value(name, name);
        QLabel *lbl = new QLabel(desc + ":");
        lbl->setFixedWidth(150);
        row->addWidget(lbl);

        QLineEdit *minEdit = new QLineEdit();
        minEdit->setPlaceholderText("最小值");
        minEdit->setFixedWidth(80);
        minEdit->setStyleSheet("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px;");
        minEdit->installEventFilter(this);

        QLineEdit *maxEdit = new QLineEdit();
        maxEdit->setPlaceholderText("最大值");
        maxEdit->setFixedWidth(80);
        maxEdit->setStyleSheet("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px;");
        maxEdit->installEventFilter(this);

        row->addWidget(new QLabel("Min:"));
        row->addWidget(minEdit);
        row->addWidget(new QLabel(" Max:"));
        row->addWidget(maxEdit);
        row->addStretch();

        limitLayout->addLayout(row);
        
        m_limitEdits[name] = {minEdit, maxEdit};
    }

    scrollLayout->addWidget(limitGroup);
}

void FeatureSwitchWidget::setupInclinometerThresholdUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *incGroup = new QGroupBox("倾角仪显示阈值 (首页 X/Y 卡片)");
    QVBoxLayout *incLayout = new QVBoxLayout(incGroup);

    auto addRow = [&](const QString &desc, QLineEdit *&edit) {
        QHBoxLayout *h = new QHBoxLayout();
        h->addWidget(new QLabel(desc));
        edit = new QLineEdit();
        edit->setFixedWidth(150);
        edit->setStyleSheet("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px; padding: 3px;");
        edit->installEventFilter(this);
        h->addWidget(edit);
        h->addStretch();
        incLayout->addLayout(h);
    };

    addRow("X 轴倾角显示阈值 (°):", m_editInclinometerThresholdX);
    addRow("Y 轴倾角显示阈值 (°):", m_editInclinometerThresholdY);

    scrollLayout->addWidget(incGroup);
}

void FeatureSwitchWidget::loadInclinometerThresholdState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Inclinometer");
    m_editInclinometerThresholdX->setText(
        QString::number(settings.value("display_threshold_x_deg", 1.0).toDouble(), 'f', 2));
    m_editInclinometerThresholdY->setText(
        QString::number(settings.value("display_threshold_y_deg", 1.0).toDouble(), 'f', 2));
    settings.endGroup();
}

void FeatureSwitchWidget::saveInclinometerThresholdState()
{
    auto parseBounded = [](const QString &text, double fallback) -> double {
        bool ok = false;
        const double v = text.trimmed().toDouble(&ok);
        if (!ok) {
            return fallback;
        }
        return qBound(0.01, v, 90.0);
    };

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Inclinometer");
    settings.setValue("display_threshold_x_deg", parseBounded(m_editInclinometerThresholdX->text(), 1.0));
    settings.setValue("display_threshold_y_deg", parseBounded(m_editInclinometerThresholdY->text(), 1.0));
    settings.endGroup();
    settings.sync();
}

void FeatureSwitchWidget::loadPollingState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    m_cbUiStateSync->setChecked(settings.value("ui_state_sync_enabled", true).toBool());
    m_editMainModbusPoll->setText(settings.value("main_modbus_poll_ms", 500).toString());
    m_editMainUiPoll->setText(settings.value("main_ui_poll_ms", 200).toString());
    m_editMainDeviceStatusPoll->setText(settings.value("main_device_status_poll_ms", 2000).toString());
    m_editMainDeviceStatusStart->setText(settings.value("main_device_status_start", 0).toString());
    m_editMainDeviceStatusCount->setText(settings.value("main_device_status_count", 85).toString());
    m_editMainControlSyncStart->setText(settings.value("main_control_sync_start", 125).toString());
    m_editMainControlSyncCount->setText(settings.value("main_control_sync_count", 6).toString());
    m_editMainReconnect->setText(settings.value("main_reconnect_ms", 5000).toString());
    m_editAgvPoll->setText(settings.value("agv_poll_ms", 200).toString());
    m_editAgvReconnect->setText(settings.value("agv_reconnect_ms", 5000).toString());
    m_editTeachingWriteDeviceId->setText(QString::number(settings.value("teaching_write_device_id", 0).toInt()));
    settings.endGroup();
}

void FeatureSwitchWidget::savePollingState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    settings.setValue("ui_state_sync_enabled", m_cbUiStateSync->isChecked());
    settings.setValue("main_modbus_poll_ms", m_editMainModbusPoll->text().toInt());
    settings.setValue("main_ui_poll_ms", m_editMainUiPoll->text().toInt());
    settings.setValue("main_device_status_poll_ms", m_editMainDeviceStatusPoll->text().toInt());
    settings.setValue("main_device_status_start", m_editMainDeviceStatusStart->text().toInt());
    settings.setValue("main_device_status_count", m_editMainDeviceStatusCount->text().toInt());
    settings.setValue("main_control_sync_start", m_editMainControlSyncStart->text().toInt());
    settings.setValue("main_control_sync_count", m_editMainControlSyncCount->text().toInt());
    settings.setValue("main_reconnect_ms", m_editMainReconnect->text().toInt());
    settings.setValue("agv_poll_ms", m_editAgvPoll->text().toInt());
    settings.setValue("agv_reconnect_ms", m_editAgvReconnect->text().toInt());
    settings.setValue("teaching_write_device_id", m_editTeachingWriteDeviceId->text().toInt());
    settings.endGroup();
    settings.sync();
}

void FeatureSwitchWidget::loadSliderLimitState()
{
    const QMap<QString, QPair<double, double>> defaultRanges = {
        {"robot_ArcGauge_J1Angle", qMakePair(-170.0, 170.0)},
        {"robot_ArcGauge_J2Height", qMakePair(-850.0, 1150.0)},
        {"robot_ArcGauge_J3Length", qMakePair(0.0, 1600.0)},
        {"robot_ArcGauge_J4Angle", qMakePair(-180.0, 180.0)},
        {"robot_ArcGauge_SixAxis1", qMakePair(-15.0, 15.0)},
        {"robot_ArcGauge_SixAxis2", qMakePair(-15.0, 15.0)},
        {"robot_ArcGauge_SixAxis3", qMakePair(-12.0, 12.0)},
        {"robot_ArcGauge_SixAxis4", qMakePair(-110.0, 110.0)},
        {"robot_ArcGauge_SixAxis5", qMakePair(-110.0, 110.0)},
        {"robot_ArcGauge_SixAxis6", qMakePair(-90.0, 90.0)}
    };

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");
    for (auto it = m_limitEdits.begin(); it != m_limitEdits.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());

        const auto range = defaultRanges.value(it.key(), qMakePair(0.0, 100.0));

        QVariant minVar = settings.value(keyMin);
        QVariant maxVar = settings.value(keyMax);

        const double minVal = minVar.isValid() ? minVar.toDouble() : range.first;
        const double maxVal = maxVar.isValid() ? maxVar.toDouble() : range.second;
        
        it.value().minEdit->setText(QString::number(minVal));
        it.value().maxEdit->setText(QString::number(maxVal));
    }
    settings.endGroup();
}

void FeatureSwitchWidget::saveSliderLimitState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");

    for (auto it = m_limitEdits.begin(); it != m_limitEdits.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());

        const double minVal = it.value().minEdit->text().toDouble();
        const double maxVal = it.value().maxEdit->text().toDouble();
        settings.setValue(keyMin, minVal);
        settings.setValue(keyMax, maxVal);
    }
    settings.endGroup();
    settings.sync();
}

void FeatureSwitchWidget::onApply()
{
    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();
    for (auto it = m_bigCheckboxes.begin(); it != m_bigCheckboxes.end(); ++it) {
        mgr->setBigFeatureEnabled(it.key(), it.value()->isChecked());
    }
    for (auto it = m_smallCheckboxes.begin(); it != m_smallCheckboxes.end(); ++it) {
        mgr->setSmallFeatureEnabled(it.key(), it.value()->isChecked());
    }
    
    // 应用轮询配置
    savePollingState();
    
    // 应用滑块限制配置
    saveSliderLimitState();

    saveInclinometerThresholdState();

    emit runtimeSettingsChanged();

    this->hide(); // 立即生效后隐藏界面
}

void FeatureSwitchWidget::onSave()
{
    onApply();
    FeatureSwitchManager::instance()->save();
    QMessageBox::information(this, "结果", "配置已成功保存到 feature_switches.ini。");
}

void FeatureSwitchWidget::onReload()
{
    FeatureSwitchManager::instance()->reload();
    loadCurrentState();
}

void FeatureSwitchWidget::onToggleAll(bool checked)
{
    for (auto cb : m_bigCheckboxes) cb->setChecked(checked);
    for (auto cb : m_smallCheckboxes) cb->setChecked(checked);
}
