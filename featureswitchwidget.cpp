#include "featureswitchwidget.h"
#include "featureswitchmanager.h"
#include "techvirtualkeyboard.h"
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <QEvent>
#include <QSettings>
#include "mainwindow.h"

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
    
    resize(700, 900);
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
    QVBoxLayout *bigLayout = new QVBoxLayout(bigGroup);
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

    for (const QString &key : bigKeys) {
        QString label = desc.contains(key) ? QString("%1 [%2]").arg(desc.value(key)).arg(key) : key;
        QCheckBox *cb = new QCheckBox(label);
        bigLayout->addWidget(cb);
        m_bigCheckboxes[key] = cb;
    }
    scrollLayout->addWidget(bigGroup);

    // 小功能项
    QGroupBox *smallGroup = new QGroupBox("子功能细项 (Small Features)");
    QVBoxLayout *smallLayout = new QVBoxLayout(smallGroup);
    
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

    for (const QString &key : smallKeys) {
        QString label = sdesc.contains(key) ? QString("%1 [%2]").arg(sdesc.value(key)).arg(key) : key;
        QCheckBox *cb = new QCheckBox(label);
        smallLayout->addWidget(cb);
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
    
    // 轮询参数配置组
    setupPollingUI(scrollLayout);
    
    // 滑块自定义范围配置组
    setupSliderLimitUI(scrollLayout);

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
}

void FeatureSwitchWidget::setupPollingUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *pollGroup = new QGroupBox("通信轮询参数 (Polling Settings)");
    QVBoxLayout *pollLayout = new QVBoxLayout(pollGroup);

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
    addPollItem("主控 UI 刷新 (ms):", m_editMainUiPoll);
    addPollItem("主控 重连间隔 (ms):", m_editMainReconnect);
    addPollItem("AGV Modbus 轮询 (ms):", m_editAgvPoll);
    addPollItem("AGV 重连间隔 (ms):", m_editAgvReconnect);

    scrollLayout->addWidget(pollGroup);
}

void FeatureSwitchWidget::setupSliderLimitUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *limitGroup = new QGroupBox("参数范围自定义 (Parameter Limits)");
    QVBoxLayout *limitLayout = new QVBoxLayout(limitGroup);

    MainWindow *mw = nullptr;
    for (QWidget *widget : qApp->topLevelWidgets()) {
        mw = qobject_cast<MainWindow*>(widget);
        if (mw) break;
    }

    if (!mw) {
        limitLayout->addWidget(new QLabel("无法加载参数配置：MainWindow未找到"));
        scrollLayout->addWidget(limitGroup);
        return;
    }

    // 通过 QProperty 或从 MainWindow 获取配置（这里直接访问成员，需确保可见性或已有接口）
    // 我们可以依赖 mw->m_sliderLabelConfigs，但在 H 中它是原有的结构。
    
    // 直接遍历固定的四个控件名以保持简单，或者从 mW 获取
    QStringList targetNames = {"label_Value1", "label_Value2", "label_Value3", "label_Value4"};
    QMap<QString, QString> itemLabels;
    itemLabels["label_Value1"] = "悬臂角度 (Target1)";
    itemLabels["label_Value2"] = "升降高度 (Target2)";
    itemLabels["label_Value3"] = "悬臂长度 (Target3)";
    itemLabels["label_Value4"] = "末端角度 (Target4)";

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

void FeatureSwitchWidget::loadPollingState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    m_editMainModbusPoll->setText(settings.value("main_modbus_poll_ms", 500).toString());
    m_editMainUiPoll->setText(settings.value("main_ui_poll_ms", 200).toString());
    m_editMainReconnect->setText(settings.value("main_reconnect_ms", 5000).toString());
    m_editAgvPoll->setText(settings.value("agv_poll_ms", 200).toString());
    m_editAgvReconnect->setText(settings.value("agv_reconnect_ms", 5000).toString());
    settings.endGroup();
}

void FeatureSwitchWidget::savePollingState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    settings.setValue("main_modbus_poll_ms", m_editMainModbusPoll->text().toInt());
    settings.setValue("main_ui_poll_ms", m_editMainUiPoll->text().toInt());
    settings.setValue("main_reconnect_ms", m_editMainReconnect->text().toInt());
    settings.setValue("agv_poll_ms", m_editAgvPoll->text().toInt());
    settings.setValue("agv_reconnect_ms", m_editAgvReconnect->text().toInt());
    settings.endGroup();
    settings.sync();

    // 通知 MainWindow 更新
    MainWindow *mw = qobject_cast<MainWindow*>(parent());
    // 如果 parent 不是 MainWindow，尝试寻找全程序的 MainWindow 实例
    if (!mw) {
        for (QWidget *widget : qApp->topLevelWidgets()) {
            mw = qobject_cast<MainWindow*>(widget);
            if (mw) break;
        }
    }

    if (mw) {
        mw->loadPollingRuntimeSettings();
        mw->applyPollingRuntimeSettings();
    }
}

void FeatureSwitchWidget::loadSliderLimitState()
{
    // 先获取 MainWindow 中的当前值（包含默认或已加载的值）
    MainWindow *mw = nullptr;
    for (QWidget *widget : qApp->topLevelWidgets()) {
        mw = qobject_cast<MainWindow*>(widget);
        if (mw) break;
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");
    for (auto it = m_limitEdits.begin(); it != m_limitEdits.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());
        
        bool okMin, okMax;
        double minVal = settings.value(keyMin).toDouble(&okMin);
        double maxVal = settings.value(keyMax).toDouble(&okMax);
        
        // 如果 INI 中没有，则尝试从 MW 的 Config Map 中获取
        if (!okMin || !okMax) {
            if (mw && mw->m_sliderLabelConfigs.contains(it.key())) {
                const auto& cfg = mw->m_sliderLabelConfigs[it.key()];
                if (!okMin) minVal = cfg.minValue;
                if (!okMax) maxVal = cfg.maxValue;
            }
        }
        
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
        
        settings.setValue(keyMin, it.value().minEdit->text().toDouble());
        settings.setValue(keyMax, it.value().maxEdit->text().toDouble());
    }
    settings.endGroup();
    settings.sync();

    // 通知 MainWindow 重新加载并应用
    MainWindow *mw = nullptr;
    for (QWidget *widget : qApp->topLevelWidgets()) {
        mw = qobject_cast<MainWindow*>(widget);
        if (mw) break;
    }

    if (mw) {
        mw->loadSliderLabelRuntimeSettings();
        mw->applySliderLabelRuntimeSettings();
    }
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
