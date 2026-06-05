#include "featureswitchwidget.h"
#include "featureswitchmanager.h"
#include "mainwindow.h"
#include "mappingconfig.h"
#include "techvirtualkeyboard.h"
#include "techslideredit.h"
#include <algorithm>
#include <QShowEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <QEvent>
#include <QSettings>
#include <QSignalBlocker>

namespace {

MainWindow::ModbusRegisterSpec parseLegacyRegisterString(const QString &raw)
{
    MainWindow::ModbusRegisterSpec spec;
    const QString t = raw.trimmed();
    if (t.isEmpty() || t == QStringLiteral("无")) {
        return spec;
    }
    if (!t.startsWith(QStringLiteral("主控:")) && !t.startsWith(QStringLiteral("AGV:"))) {
        return spec;
    }

    const int colon = t.indexOf(QLatin1Char(':'));
    spec.device = t.left(colon);
    const QString after = t.mid(colon + 1);
    int i = 0;
    while (i < after.size() && after.at(i).isDigit()) {
        ++i;
    }
    spec.address = after.left(i);
    QString rest = after.mid(i).trimmed();

    if (rest.startsWith(QLatin1Char('('))) {
        const int close = rest.indexOf(QLatin1Char(')'));
        QString inner = close > 0 ? rest.mid(1, close - 1) : rest.mid(1);
        if (inner.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
            inner = inner.mid(3).trimmed();
        }
        spec.bit = inner;
        rest = close >= 0 ? rest.mid(close + 1).trimmed() : QString();
    }

    if (rest.startsWith(QLatin1Char('='))) {
        spec.value1 = rest.mid(1).trimmed();
    } else if (!rest.isEmpty()) {
        spec.value1 = rest;
    }
    return spec;
}

QString composeLegacyRegisterString(const MainWindow::ModbusRegisterSpec &spec)
{
    if (!spec.isConfigured()) {
        return QStringLiteral("无");
    }

    QString composed = spec.device + QLatin1Char(':') + spec.address.trimmed();
    if (!spec.bit.trimmed().isEmpty()) {
        QString bit = spec.bit.trimmed();
        if (!bit.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
            bit = QStringLiteral("bit") + bit;
        }
        composed += QLatin1Char('(') + bit + QLatin1Char(')');
    }
    if (!spec.value1.trimmed().isEmpty()) {
        const QString val = spec.value1.trimmed();
        if (val.startsWith(QLatin1Char('+')) || val.contains(QLatin1Char('='))) {
            composed += val.startsWith(QLatin1Char('+')) ? val : (QLatin1Char('=') + val);
        } else if (spec.bit.trimmed().isEmpty()) {
            composed += QLatin1Char('=') + val;
        } else {
            composed += QLatin1Char('=') + val;
        }
    }
    return composed;
}

bool hasModbusOperation(const MainWindow::ControllableButtonInfo &info)
{
    if (info.objectName == QStringLiteral("techBtn_spare_1")
        || info.objectName == QStringLiteral("techBtn_spare_2")) {
        return true;
    }
    if (info.objectName == QStringLiteral("steeringModeSelector")) {
        return false;
    }
    return !info.defaultReads.isEmpty() || !info.defaultWrites.isEmpty();
}

bool shouldSkipControllable(const MainWindow::ControllableButtonInfo &info)
{
    return info.widgetKind == QStringLiteral("环形仪表")
        || info.objectName.startsWith(QStringLiteral("robot_ArcGauge_"))
        || info.widgetKind == QStringLiteral("滑块输入")
        || info.objectName.startsWith(QStringLiteral("TechSliderEdit_"))
        || info.objectName.startsWith(QStringLiteral("SEdit_"));
}

MainWindow::ModbusRegisterSpec loadRegisterSpec(QSettings &settings,
                                                const QString &keyPrefix,
                                                const MainWindow::ModbusRegisterSpec &fallback)
{
    auto normalizeBitText = [](QString bit) {
        bit = bit.trimmed();
        if (bit == QStringLiteral("空位")
            || bit == QStringLiteral("(空位)")
            || bit == QStringLiteral("无")
            || bit == QStringLiteral("(无)")) {
            return QString();
        }
        if (bit.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
            bit = bit.mid(3).trimmed();
        }
        return bit;
    };

    MainWindow::ModbusRegisterSpec loaded;

    if (settings.contains(keyPrefix + QStringLiteral("_device"))) {
        loaded.device = settings.value(keyPrefix + QStringLiteral("_device"), QStringLiteral("无")).toString();
        loaded.address = settings.value(keyPrefix + QStringLiteral("_addr")).toString();
        loaded.bit = settings.value(keyPrefix + QStringLiteral("_bit")).toString();
        if (loaded.bit.isEmpty()) {
            loaded.bit = settings.value(keyPrefix + QStringLiteral("_extra")).toString();
        }
        loaded.bit = normalizeBitText(loaded.bit);
        loaded.value1 = settings.value(keyPrefix + QStringLiteral("_value1")).toString();
        if (loaded.value1.isEmpty()) {
            loaded.value1 = settings.value(keyPrefix + QStringLiteral("_value")).toString();
        }
        loaded.value2 = settings.value(keyPrefix + QStringLiteral("_value2")).toString();
        loaded.value3 = settings.value(keyPrefix + QStringLiteral("_value3")).toString();
    } else if (settings.contains(keyPrefix)) {
        loaded = parseLegacyRegisterString(settings.value(keyPrefix).toString());
    }

    if (loaded.isConfigured()) {
        if (loaded.bit.trimmed().isEmpty()) {
            loaded.bit = normalizeBitText(fallback.bit);
        }
        if (loaded.value1.trimmed().isEmpty()) {
            loaded.value1 = fallback.value1;
        }
        if (loaded.value2.trimmed().isEmpty()) {
            loaded.value2 = fallback.value2;
        }
        if (loaded.value3.trimmed().isEmpty()) {
            loaded.value3 = fallback.value3;
        }
        return loaded;
    }
    return fallback;
}

constexpr int kMaxModbusTargetsPerDirection = 3;

QList<MainWindow::ModbusRegisterSpec> loadRegisterSpecs(QSettings &settings,
                                                        const QString &basePrefix,
                                                        const QList<MainWindow::ModbusRegisterSpec> &fallbacks)
{
    auto splitCompositeBitSpec = [](const MainWindow::ModbusRegisterSpec &spec) {
        QList<MainWindow::ModbusRegisterSpec> out;
        const QString bitText = spec.bit.trimmed();
        if (!bitText.contains(QLatin1Char('/'))) {
            out.append(spec);
            return out;
        }

        const QStringList bitParts = bitText.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (bitParts.isEmpty()) {
            out.append(spec);
            return out;
        }

        const QString v1 = spec.value1.trimmed();
        const QString v2 = spec.value2.trimmed();
        const bool twoBitMode = (bitParts.size() == 2
                                 && v1.size() == 2 && v2.size() == 2
                                 && (v1.at(0) == QLatin1Char('0') || v1.at(0) == QLatin1Char('1'))
                                 && (v1.at(1) == QLatin1Char('0') || v1.at(1) == QLatin1Char('1'))
                                 && (v2.at(0) == QLatin1Char('0') || v2.at(0) == QLatin1Char('1'))
                                 && (v2.at(1) == QLatin1Char('0') || v2.at(1) == QLatin1Char('1')));

        for (int i = 0; i < bitParts.size(); ++i) {
            MainWindow::ModbusRegisterSpec piece = spec;
            QString pieceBit = bitParts.at(i).trimmed();
            if (pieceBit.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
                pieceBit = pieceBit.mid(3).trimmed();
            }
            piece.bit = pieceBit;
            if (twoBitMode) {
                piece.value1 = QString(v1.at(i));
                piece.value2 = QString(v2.at(i));
            }
            out.append(piece);
        }
        return out;
    };

    auto appendUnique = [](QList<MainWindow::ModbusRegisterSpec> &target,
                           const MainWindow::ModbusRegisterSpec &spec) {
        if (!spec.isConfigured()) {
            return;
        }
        for (const auto &existing : target) {
            if (existing.device == spec.device
                && existing.address == spec.address
                && existing.bit == spec.bit) {
                return;
            }
        }
        target.append(spec);
    };

    QList<MainWindow::ModbusRegisterSpec> loaded;
    loaded.reserve(kMaxModbusTargetsPerDirection);

    const bool hasLegacyBase = settings.contains(basePrefix + QStringLiteral("_device"))
        || settings.contains(basePrefix);

    bool hasIndexed = false;
    for (int i = 0; i < kMaxModbusTargetsPerDirection; ++i) {
        const QString indexedPrefix = QStringLiteral("%1_%2").arg(basePrefix).arg(i + 1);
        if (settings.contains(indexedPrefix + QStringLiteral("_device")) || settings.contains(indexedPrefix)) {
            hasIndexed = true;
            break;
        }
    }

    if (hasIndexed) {
        for (int i = 0; i < kMaxModbusTargetsPerDirection; ++i) {
            const QString indexedPrefix = QStringLiteral("%1_%2").arg(basePrefix).arg(i + 1);
            const MainWindow::ModbusRegisterSpec fallback = (i < fallbacks.size())
                ? fallbacks.at(i)
                : MainWindow::ModbusRegisterSpec{};
            if (settings.contains(indexedPrefix + QStringLiteral("_device"))
                || settings.contains(indexedPrefix)) {
                const MainWindow::ModbusRegisterSpec indexed = loadRegisterSpec(settings, indexedPrefix, fallback);
                for (const auto &piece : splitCompositeBitSpec(indexed)) {
                    appendUnique(loaded, piece);
                }
            } else if (i < fallbacks.size()) {
                appendUnique(loaded, fallbacks.at(i));
            }
        }
    } else if (hasLegacyBase) {
        const MainWindow::ModbusRegisterSpec legacy = loadRegisterSpec(
            settings, basePrefix, fallbacks.isEmpty() ? MainWindow::ModbusRegisterSpec{} : fallbacks.first());
        for (const auto &piece : splitCompositeBitSpec(legacy)) {
            appendUnique(loaded, piece);
        }
        for (int i = 1; i < fallbacks.size(); ++i) {
            appendUnique(loaded, fallbacks.at(i));
        }
    } else {
        for (const auto &fallback : fallbacks) {
            appendUnique(loaded, fallback);
        }
    }

    if (loaded.size() > kMaxModbusTargetsPerDirection) {
        loaded = loaded.mid(0, kMaxModbusTargetsPerDirection);
    }
    return loaded;
}

} // namespace

FeatureSwitchWidget::ModbusRegisterEdits FeatureSwitchWidget::makeRegisterRowEdits(
    QWidget *parent,
    const QString &lineEditStyle)
{
    ModbusRegisterEdits edits;
    edits.device = new QComboBox(parent);
    edits.device->addItems({QStringLiteral("无"), QStringLiteral("主控"), QStringLiteral("AGV")});
    edits.device->setFixedWidth(58);

    edits.address = new QLineEdit(parent);
    edits.address->setPlaceholderText(QStringLiteral("寄存器"));
    edits.address->setFixedWidth(68);
    edits.address->setStyleSheet(lineEditStyle);
    edits.address->installEventFilter(this);

    edits.bit = new QLineEdit(parent);
    edits.bit->setPlaceholderText(QStringLiteral("位，可空"));
    edits.bit->setFixedWidth(72);
    edits.bit->setStyleSheet(lineEditStyle);
    edits.bit->installEventFilter(this);

    edits.value1 = new QLineEdit(parent);
    edits.value1->setPlaceholderText(QStringLiteral("值1"));
    edits.value1->setFixedWidth(68);
    edits.value1->setStyleSheet(lineEditStyle);
    edits.value1->installEventFilter(this);

    edits.value2 = new QLineEdit(parent);
    edits.value2->setPlaceholderText(QStringLiteral("值2"));
    edits.value2->setFixedWidth(68);
    edits.value2->setStyleSheet(lineEditStyle);
    edits.value2->installEventFilter(this);

    edits.value3 = new QLineEdit(parent);
    edits.value3->setPlaceholderText(QStringLiteral("值3"));
    edits.value3->setFixedWidth(68);
    edits.value3->setStyleSheet(lineEditStyle);
    edits.value3->installEventFilter(this);

    const auto updateEnabled = [edits]() {
        const bool enabled = edits.device && edits.device->currentText() != QStringLiteral("无");
        if (edits.address) {
            edits.address->setEnabled(enabled);
        }
        if (edits.bit) {
            edits.bit->setEnabled(enabled);
        }
        if (edits.value1) {
            edits.value1->setEnabled(enabled);
        }
        if (edits.value2) {
            edits.value2->setEnabled(enabled);
        }
        if (edits.value3) {
            edits.value3->setEnabled(enabled);
        }
        if (!enabled) {
            if (edits.address) {
                edits.address->clear();
            }
            if (edits.bit) {
                edits.bit->clear();
            }
            if (edits.value1) {
                edits.value1->clear();
            }
            if (edits.value2) {
                edits.value2->clear();
            }
            if (edits.value3) {
                edits.value3->clear();
            }
        }
    };

    connect(edits.device, QOverload<int>::of(&QComboBox::currentIndexChanged), parent,
            [updateEnabled](int) { updateEnabled(); });

    updateEnabled();
    return edits;
}

void FeatureSwitchWidget::addModbusRegisterRow(QHBoxLayout *row,
                                               const QString &label,
                                               const ModbusRegisterEdits &edits,
                                               const QString &syncHint)
{
    QLabel *lbl = new QLabel(label);
    lbl->setFixedWidth(28);
    lbl->setStyleSheet(QStringLiteral("color: #aaccff;"));
    row->addWidget(lbl);
    row->addWidget(edits.device);
    row->addWidget(new QLabel(QStringLiteral("寄存器")));
    row->addWidget(edits.address);
    row->addWidget(new QLabel(QStringLiteral("位")));
    row->addWidget(edits.bit);
    row->addWidget(new QLabel(QStringLiteral("值1")));
    row->addWidget(edits.value1);
    row->addWidget(new QLabel(QStringLiteral("值2")));
    row->addWidget(edits.value2);
    row->addWidget(new QLabel(QStringLiteral("值3")));
    row->addWidget(edits.value3);
    if (!syncHint.isEmpty()) {
        QLabel *hint = new QLabel(syncHint);
        hint->setStyleSheet(QStringLiteral("color: #66aa88; font-size: 10px;"));
        row->addWidget(hint);
    }
}

void FeatureSwitchWidget::applyRegisterSpecToEdits(const MainWindow::ModbusRegisterSpec &spec,
                                                   ModbusRegisterEdits &edits)
{
    if (!edits.device || !edits.address || !edits.bit || !edits.value1 || !edits.value2 || !edits.value3) {
        return;
    }

    QSignalBlocker blockerDevice(edits.device);
    const int deviceIndex = edits.device->findText(spec.device);
    edits.device->setCurrentIndex(deviceIndex >= 0 ? deviceIndex : 0);

    const bool enabled = spec.device != QStringLiteral("无");
    edits.address->setEnabled(enabled);
    edits.bit->setEnabled(enabled);
    edits.value1->setEnabled(enabled);
    edits.value2->setEnabled(enabled);
    edits.value3->setEnabled(enabled);

    edits.address->setText(spec.address);
    edits.bit->setText(spec.bit);
    edits.value1->setText(spec.value1);
    edits.value2->setText(spec.value2);
    edits.value3->setText(spec.value3);

    if (!enabled) {
        edits.address->clear();
        edits.bit->clear();
        edits.value1->clear();
        edits.value2->clear();
        edits.value3->clear();
    }
}

MainWindow::ModbusRegisterSpec FeatureSwitchWidget::readRegisterSpecFromEdits(
    const ModbusRegisterEdits &edits) const
{
    MainWindow::ModbusRegisterSpec spec;
    if (!edits.device || !edits.address || !edits.bit || !edits.value1 || !edits.value2 || !edits.value3) {
        return spec;
    }
    spec.device = edits.device->currentText().trimmed();
    spec.address = edits.address->text().trimmed();
    spec.bit = edits.bit->text().trimmed();
    spec.value1 = edits.value1->text().trimmed();
    spec.value2 = edits.value2->text().trimmed();
    spec.value3 = edits.value3->text().trimmed();
    return spec;
}

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

    setupButtonVisibilityUI(scrollLayout);

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
    loadTechSliderEditState();
    loadInclinometerThresholdState();
    loadButtonVisibilityState();
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
    const QString lineEditStyle =
        QStringLiteral("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px;");

    QGroupBox *arcGroup = new QGroupBox(QStringLiteral("TechArcGauge 显示与参数范围"));
    QVBoxLayout *arcLayout = new QVBoxLayout(arcGroup);

    QLabel *arcHint = new QLabel(
        QStringLiteral("每行可单独控制环形仪表是否显示，并自定义数值 Min/Max（与主界面仪表 objectName 一致）"));
    arcHint->setWordWrap(true);
    arcHint->setStyleSheet(QStringLiteral("color: #88ccff; font-size: 11px;"));
    arcLayout->addWidget(arcHint);

    QHBoxLayout *arcToolbar = new QHBoxLayout();
    QPushButton *arcShowAll = new QPushButton(QStringLiteral("仪表全部显示"));
    QPushButton *arcHideAll = new QPushButton(QStringLiteral("仪表全部隐藏"));
    arcToolbar->addWidget(arcShowAll);
    arcToolbar->addWidget(arcHideAll);
    arcToolbar->addStretch();
    arcLayout->addLayout(arcToolbar);

    const QStringList arcGaugeNames = {
        QStringLiteral("robot_ArcGauge_J1Angle"),
        QStringLiteral("robot_ArcGauge_J2Height"),
        QStringLiteral("robot_ArcGauge_J3Length"),
        QStringLiteral("robot_ArcGauge_J4Angle"),
        QStringLiteral("robot_ArcGauge_SixAxis1"),
        QStringLiteral("robot_ArcGauge_SixAxis2"),
        QStringLiteral("robot_ArcGauge_SixAxis3"),
        QStringLiteral("robot_ArcGauge_SixAxis4"),
        QStringLiteral("robot_ArcGauge_SixAxis5"),
        QStringLiteral("robot_ArcGauge_SixAxis6")
    };
    const QMap<QString, QString> arcLabels = {
        {QStringLiteral("robot_ArcGauge_J1Angle"), QStringLiteral("悬臂角度 (J1)")},
        {QStringLiteral("robot_ArcGauge_J2Height"), QStringLiteral("升降高度 (J2)")},
        {QStringLiteral("robot_ArcGauge_J3Length"), QStringLiteral("总伸展长度 (J3)")},
        {QStringLiteral("robot_ArcGauge_J4Angle"), QStringLiteral("末端角度 (J4)")},
        {QStringLiteral("robot_ArcGauge_SixAxis1"), QStringLiteral("六轴 RX")},
        {QStringLiteral("robot_ArcGauge_SixAxis2"), QStringLiteral("六轴 RY")},
        {QStringLiteral("robot_ArcGauge_SixAxis3"), QStringLiteral("六轴 RZ")},
        {QStringLiteral("robot_ArcGauge_SixAxis4"), QStringLiteral("六轴 X")},
        {QStringLiteral("robot_ArcGauge_SixAxis5"), QStringLiteral("六轴 Y")},
        {QStringLiteral("robot_ArcGauge_SixAxis6"), QStringLiteral("六轴 Z")}
    };

    for (const QString &name : arcGaugeNames) {
        QHBoxLayout *row = new QHBoxLayout();
        QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"));
        visibleCb->setFixedWidth(56);

        QLabel *lbl = new QLabel(arcLabels.value(name, name) + QStringLiteral(":"));
        lbl->setFixedWidth(130);

        QLineEdit *minEdit = new QLineEdit();
        minEdit->setPlaceholderText(QStringLiteral("最小值"));
        minEdit->setFixedWidth(72);
        minEdit->setStyleSheet(lineEditStyle);
        minEdit->installEventFilter(this);

        QLineEdit *maxEdit = new QLineEdit();
        maxEdit->setPlaceholderText(QStringLiteral("最大值"));
        maxEdit->setFixedWidth(72);
        maxEdit->setStyleSheet(lineEditStyle);
        maxEdit->installEventFilter(this);

        row->addWidget(visibleCb);
        row->addWidget(lbl);
        row->addWidget(new QLabel(QStringLiteral("Min:")));
        row->addWidget(minEdit);
        row->addWidget(new QLabel(QStringLiteral("Max:")));
        row->addWidget(maxEdit);
        row->addStretch();
        arcLayout->addLayout(row);

        m_arcGaugeEdits[name] = {visibleCb, minEdit, maxEdit};
    }

    connect(arcShowAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(true);
            }
        }
    });
    connect(arcHideAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(false);
            }
        }
    });

    scrollLayout->addWidget(arcGroup);

    setupTechSliderEditUI(scrollLayout);

    QGroupBox *otherGroup = new QGroupBox(QStringLiteral("其他参数范围"));
    QVBoxLayout *otherLayout = new QVBoxLayout(otherGroup);

    const QString parkKey = QStringLiteral("agv_park_out_trigger_length");
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lbl = new QLabel(
            QStringLiteral("驻车伸出触发长度 (支腿长度设置框，整数):"));
        lbl->setFixedWidth(280);
        row->addWidget(lbl);

        QLineEdit *minEdit = new QLineEdit();
        minEdit->setPlaceholderText(QStringLiteral("最小值"));
        minEdit->setFixedWidth(80);
        minEdit->setStyleSheet(lineEditStyle);
        minEdit->installEventFilter(this);

        QLineEdit *maxEdit = new QLineEdit();
        maxEdit->setPlaceholderText(QStringLiteral("最大值"));
        maxEdit->setFixedWidth(80);
        maxEdit->setStyleSheet(lineEditStyle);
        maxEdit->installEventFilter(this);

        row->addWidget(new QLabel(QStringLiteral("Min:")));
        row->addWidget(minEdit);
        row->addWidget(new QLabel(QStringLiteral(" Max:")));
        row->addWidget(maxEdit);
        row->addStretch();
        otherLayout->addLayout(row);
        m_limitEdits[parkKey] = {minEdit, maxEdit};
    }

    scrollLayout->addWidget(otherGroup);
}

namespace {
struct SliderEditDefaults {
    double displayMin = 0.0;
    double displayMax = 100.0;
};

QMap<QString, SliderEditDefaults> builtinSliderEditDefaults()
{
    QMap<QString, SliderEditDefaults> defaults;
    const auto put = [&](const char *name, double vmin, double vmax) {
        defaults.insert(QString::fromLatin1(name), {vmin, vmax});
    };
    put("TechSliderEdit_HoriSupSec_RotationSpeed", 0, 5);
    put("TechSliderEdit_HoriSupSec_MoveSpeed", 0, 20);
    put("TechSliderEdit_VeSupSec_MoveSpeed", 0, 35);
    put("TechSliderEdit_EOAT_RotationSpeed", 0, 100);
    put("SEdit_AGV_MoveSpeed", 0, 100);
    put("SEdit_AGV_Angle", -25, 25);
    put("TechSliderEdit_Robot_RobotSpeed", 0, 100);
    return defaults;
}

QLineEdit *makeLimitEdit(QWidget *parent, const QString &style, FeatureSwitchWidget *host)
{
    QLineEdit *edit = new QLineEdit(parent);
    edit->setFixedWidth(64);
    edit->setStyleSheet(style);
    edit->installEventFilter(host);
    return edit;
}
} // namespace

void FeatureSwitchWidget::setupTechSliderEditUI(QVBoxLayout *scrollLayout)
{
    const QString lineEditStyle =
        QStringLiteral("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px;");

    QGroupBox *group = new QGroupBox(QStringLiteral("TechSliderEdit 显示与范围"));
    QVBoxLayout *layout = new QVBoxLayout(group);

    QLabel *hint = new QLabel(
        QStringLiteral("每行可设置是否显示及滑块两端显示范围（Min/Max）；LineEdit 输入范围与数值范围将自动与显示范围一致"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #88ccff; font-size: 11px;"));
    layout->addWidget(hint);

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *showAll = new QPushButton(QStringLiteral("滑块全部显示"));
    QPushButton *hideAll = new QPushButton(QStringLiteral("滑块全部隐藏"));
    toolbar->addWidget(showAll);
    toolbar->addWidget(hideAll);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    QScrollArea *scroll = new QScrollArea(group);
    scroll->setWidgetResizable(true);
    scroll->setMaximumHeight(220);
    QWidget *listHost = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(listHost);
    listLayout->setContentsMargins(0, 0, 0, 0);

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QList<TechSliderEdit*> sliders;
    if (mainWindow) {
        sliders = mainWindow->findChildren<TechSliderEdit*>();
    }
    std::sort(sliders.begin(), sliders.end(), [](TechSliderEdit *a, TechSliderEdit *b) {
        return a->objectName() < b->objectName();
    });

    if (sliders.isEmpty()) {
        listLayout->addWidget(new QLabel(
            QStringLiteral("未找到 TechSliderEdit（请确认主窗口已初始化）"), listHost));
    } else {
        for (TechSliderEdit *slider : sliders) {
            const QString name = slider->objectName();
            if (name.isEmpty()) {
                continue;
            }

            QHBoxLayout *row = new QHBoxLayout();
            QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"), listHost);
            visibleCb->setFixedWidth(56);

            QString title = slider->labelText().trimmed();
            if (title.isEmpty()) {
                title = name;
            }
            QLabel *lbl = new QLabel(title, listHost);
            lbl->setFixedWidth(120);
            lbl->setToolTip(name);

            QLineEdit *displayMin = makeLimitEdit(listHost, lineEditStyle, this);
            QLineEdit *displayMax = makeLimitEdit(listHost, lineEditStyle, this);

            row->addWidget(visibleCb);
            row->addWidget(lbl);
            row->addWidget(new QLabel(QStringLiteral("Min:"), listHost));
            row->addWidget(displayMin);
            row->addWidget(new QLabel(QStringLiteral("Max:"), listHost));
            row->addWidget(displayMax);
            row->addStretch();
            listLayout->addLayout(row);

            m_sliderEditEdits[name] = {visibleCb, displayMin, displayMax};
        }
    }

    scroll->setWidget(listHost);
    layout->addWidget(scroll);

    connect(showAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(true);
            }
        }
    });
    connect(hideAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(false);
            }
        }
    });

    scrollLayout->addWidget(group);
}

void FeatureSwitchWidget::loadTechSliderEditState()
{
    const QMap<QString, SliderEditDefaults> builtinDefaults = builtinSliderEditDefaults();
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);

    settings.beginGroup(QStringLiteral("TechSliderEditLimits"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        const QString &name = it.key();
        SliderEditDefaults fallback = builtinDefaults.value(name, SliderEditDefaults());
        if (TechSliderEdit *slider = mainWindow ? mainWindow->findChild<TechSliderEdit*>(name) : nullptr) {
            fallback.displayMin = slider->displayRangeMinimum();
            fallback.displayMax = slider->displayRangeMaximum();
        }

        const auto read = [&](const QString &suffix, double defaultVal) -> double {
            const QString key = name + suffix;
            return settings.contains(key) ? settings.value(key).toDouble() : defaultVal;
        };

        if (it->displayMinEdit) {
            it->displayMinEdit->setText(QString::number(
                read(QStringLiteral("_display_min"), read(QStringLiteral("_value_min"), fallback.displayMin))));
        }
        if (it->displayMaxEdit) {
            it->displayMaxEdit->setText(QString::number(
                read(QStringLiteral("_display_max"), read(QStringLiteral("_value_max"), fallback.displayMax))));
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        if (it->visible) {
            it->visible->setChecked(settings.value(it.key(), true).toBool());
        }
    }
    settings.endGroup();
}

void FeatureSwitchWidget::saveTechSliderEditState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("TechSliderEditLimits"));

    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        const QString &name = it.key();
        const SliderEditEdits &edits = it.value();
        if (edits.displayMinEdit) {
            settings.setValue(name + QStringLiteral("_display_min"), edits.displayMinEdit->text().toDouble());
        }
        if (edits.displayMaxEdit) {
            settings.setValue(name + QStringLiteral("_display_max"), edits.displayMaxEdit->text().toDouble());
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        if (it->visible) {
            settings.setValue(it.key(), it->visible->isChecked());
        }
    }
    settings.endGroup();
    settings.sync();
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

void FeatureSwitchWidget::setupButtonVisibilityUI(QVBoxLayout *scrollLayout)
{
    m_modbusButtonGroup = new QGroupBox(QStringLiteral("Modbus 按键：显示与寄存器"));
    QVBoxLayout *modbusLayout = new QVBoxLayout(m_modbusButtonGroup);

    QLabel *modbusHint = new QLabel(
        QStringLiteral("仅列出有 Modbus 读/写的按键（自动扫描）。每项分四段：设备、寄存器、位（不需要可留空）、值（判断或写入）。"
                       "读行一般用于界面状态同步；默认值为程序内已有逻辑，打开本页即可看到。"));
    modbusHint->setWordWrap(true);
    modbusHint->setStyleSheet(QStringLiteral("color: #88ccff; font-size: 11px;"));
    modbusLayout->addWidget(modbusHint);

    QHBoxLayout *modbusToolbar = new QHBoxLayout();
    QPushButton *modbusShowAll = new QPushButton(QStringLiteral("全部显示"));
    QPushButton *modbusHideAll = new QPushButton(QStringLiteral("全部隐藏"));
    modbusToolbar->addWidget(modbusShowAll);
    modbusToolbar->addWidget(modbusHideAll);
    modbusToolbar->addStretch();
    modbusLayout->addLayout(modbusToolbar);

    QScrollArea *modbusScroll = new QScrollArea();
    modbusScroll->setWidgetResizable(true);
    modbusScroll->setMaximumHeight(420);
    m_modbusButtonListHost = new QWidget();
    m_modbusButtonListLayout = new QVBoxLayout(m_modbusButtonListHost);
    m_modbusButtonListLayout->setSpacing(8);
    modbusScroll->setWidget(m_modbusButtonListHost);
    modbusLayout->addWidget(modbusScroll);

    connect(modbusShowAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(true);
            }
        }
    });
    connect(modbusHideAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(false);
            }
        }
    });

    scrollLayout->addWidget(m_modbusButtonGroup);

    m_otherVisibilityGroup = new QGroupBox(QStringLiteral("其他控件可见性（无 Modbus）"));
    QVBoxLayout *otherLayout = new QVBoxLayout(m_otherVisibilityGroup);

    QLabel *otherHint = new QLabel(
        QStringLiteral("无 Modbus 读写的导航按钮等，仅控制是否显示。"
                       "TechArcGauge / TechSliderEdit 请在上方专用分组中配置。"));
    otherHint->setWordWrap(true);
    otherHint->setStyleSheet(QStringLiteral("color: #88ccff; font-size: 11px;"));
    otherLayout->addWidget(otherHint);

    QHBoxLayout *otherToolbar = new QHBoxLayout();
    QPushButton *otherShowAll = new QPushButton(QStringLiteral("全部显示"));
    QPushButton *otherHideAll = new QPushButton(QStringLiteral("全部隐藏"));
    otherToolbar->addWidget(otherShowAll);
    otherToolbar->addWidget(otherHideAll);
    otherToolbar->addStretch();
    otherLayout->addLayout(otherToolbar);

    QScrollArea *otherScroll = new QScrollArea();
    otherScroll->setWidgetResizable(true);
    otherScroll->setMaximumHeight(220);
    m_otherVisibilityListHost = new QWidget();
    m_otherVisibilityGrid = new QGridLayout(m_otherVisibilityListHost);
    otherScroll->setWidget(m_otherVisibilityListHost);
    otherLayout->addWidget(otherScroll);

    connect(otherShowAll, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *cb : m_otherVisibilityCheckboxes) {
            cb->setChecked(true);
        }
    });
    connect(otherHideAll, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *cb : m_otherVisibilityCheckboxes) {
            cb->setChecked(false);
        }
    });

    scrollLayout->addWidget(m_otherVisibilityGroup);
    refreshButtonVisibilityList();
}

void FeatureSwitchWidget::refreshButtonVisibilityList()
{
    if (!m_modbusButtonListLayout || !m_otherVisibilityGrid) {
        return;
    }

    while (QLayoutItem *item = m_modbusButtonListLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_modbusButtonEdits.clear();

    while (QLayoutItem *item = m_otherVisibilityGrid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_otherVisibilityCheckboxes.clear();

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QList<MainWindow::ControllableButtonInfo> buttons;
    if (mainWindow) {
        buttons = mainWindow->controllableButtons();
    }

    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);

    const QString lineEditStyle =
        QStringLiteral("background-color: #002233; color: #ffffff; border: 1px solid #00c8ff; border-radius: 3px; padding: 3px;");

    MappingConfig *mapping = MappingConfig::instance();
    int modbusCount = 0;
    int otherRow = 0;

    for (int i = 0; i < buttons.size(); ++i) {
        const MainWindow::ControllableButtonInfo &info = buttons.at(i);
        if (shouldSkipControllable(info)) {
            continue;
        }

        const QString &objectName = info.objectName;
        QString visibleText = info.displayText;
        if (visibleText.isEmpty()) {
            const QString mapped = mapping->mapControlName(objectName);
            if (mapped != objectName) {
                visibleText = mapped;
            }
        }
        const QString kind = info.widgetKind.isEmpty() ? QStringLiteral("控件") : info.widgetKind;

        settings.beginGroup(QStringLiteral("ButtonVisibility"));
        const bool visibleDefault = settings.value(objectName, true).toBool();
        settings.endGroup();

        if (hasModbusOperation(info)) {
            settings.beginGroup(QStringLiteral("ButtonModbusMapping"));
            QWidget *card = new QWidget(m_modbusButtonListHost);
            card->setStyleSheet(QStringLiteral(
                "QWidget { background-color: rgba(0, 30, 50, 120); border: 1px solid #004466; border-radius: 4px; }"));
            QVBoxLayout *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(8, 6, 8, 6);
            cardLayout->setSpacing(4);

            QHBoxLayout *titleRow = new QHBoxLayout();
            QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"), card);
            visibleCb->setChecked(visibleDefault);

            const QString title = visibleText.isEmpty()
                ? QStringLiteral("%1  [%2]").arg(kind, objectName)
                : QStringLiteral("%1  ·  %2  [%3]").arg(visibleText, kind, objectName);
            QLabel *titleLbl = new QLabel(title, card);
            titleLbl->setStyleSheet(QStringLiteral("color: #ffffff; font-weight: bold;"));
            titleRow->addWidget(visibleCb);
            titleRow->addWidget(titleLbl, 1);
            cardLayout->addLayout(titleRow);

            QCheckBox *secondStateDimCb = nullptr;
            if (objectName == QStringLiteral("techBtn_spare_1")
                || objectName == QStringLiteral("techBtn_spare_2")) {
                settings.beginGroup(QStringLiteral("ButtonSecondStateDarkening"));
                const bool dimEnabled = settings.value(objectName, true).toBool();
                settings.endGroup();

                secondStateDimCb = new QCheckBox(QStringLiteral("第二态 UI 变暗"), card);
                secondStateDimCb->setChecked(dimEnabled);
                secondStateDimCb->setStyleSheet(QStringLiteral("color: #ffdd88;"));
                cardLayout->addWidget(secondStateDimCb);
            }

            QLabel *mappingHint = new QLabel(
                QStringLiteral("状态映射：第1态→值1，第2态→值2，第3态→值3；若只有单一状态，只填写值1。"), card);
            mappingHint->setWordWrap(true);
            mappingHint->setStyleSheet(QStringLiteral("color: #77ddee; font-size: 10px;"));
            cardLayout->addWidget(mappingHint);

            QVector<ModbusRegisterEdits> readEditsList;
            QVector<ModbusRegisterEdits> writeEditsList;
            readEditsList.reserve(kMaxModbusTargetsPerDirection);
            writeEditsList.reserve(kMaxModbusTargetsPerDirection);

            const QString readPrefix = objectName + QStringLiteral("_read");
            const QString writePrefix = objectName + QStringLiteral("_write");
            const QList<MainWindow::ModbusRegisterSpec> readSpecs = loadRegisterSpecs(
                settings, readPrefix, info.defaultReads);
            const QList<MainWindow::ModbusRegisterSpec> writeSpecs = loadRegisterSpecs(
                settings, writePrefix, info.defaultWrites);

            for (int idx = 0; idx < kMaxModbusTargetsPerDirection; ++idx) {
                ModbusRegisterEdits edits = makeRegisterRowEdits(card, lineEditStyle);
                const QString readSyncHint = (info.readForUiSync && idx == 0) ? QStringLiteral("同步") : QString();
                QHBoxLayout *readRow = new QHBoxLayout();
                addModbusRegisterRow(readRow, QStringLiteral("读%1").arg(idx + 1), edits, readSyncHint);
                cardLayout->addLayout(readRow);
                if (idx < readSpecs.size()) {
                    applyRegisterSpecToEdits(readSpecs.at(idx), edits);
                }
                readEditsList.push_back(edits);
            }
            for (int idx = 0; idx < kMaxModbusTargetsPerDirection; ++idx) {
                ModbusRegisterEdits edits = makeRegisterRowEdits(card, lineEditStyle);
                QHBoxLayout *writeRow = new QHBoxLayout();
                addModbusRegisterRow(writeRow, QStringLiteral("写%1").arg(idx + 1), edits);
                cardLayout->addLayout(writeRow);
                if (idx < writeSpecs.size()) {
                    applyRegisterSpecToEdits(writeSpecs.at(idx), edits);
                }
                writeEditsList.push_back(edits);
            }

            m_modbusButtonListLayout->addWidget(card);
            m_modbusButtonEdits[objectName] = {visibleCb, secondStateDimCb, readEditsList, writeEditsList, info.readForUiSync};
            settings.endGroup();
            ++modbusCount;
        } else {
            const QString label = visibleText.isEmpty()
                ? QStringLiteral("%1  [%2]").arg(kind, objectName)
                : QStringLiteral("%1  (%2)  [%3]").arg(visibleText, kind, objectName);
            QCheckBox *cb = new QCheckBox(label, m_otherVisibilityListHost);
            cb->setChecked(visibleDefault);
            m_otherVisibilityGrid->addWidget(cb, otherRow / 2, otherRow % 2);
            m_otherVisibilityCheckboxes[objectName] = cb;
            ++otherRow;
        }
    }

    if (modbusCount == 0) {
        m_modbusButtonListLayout->addWidget(new QLabel(
            QStringLiteral("未扫描到有 Modbus 读写的按键（请确认主窗口已初始化）"),
            m_modbusButtonListHost));
    }
    if (otherRow == 0) {
        m_otherVisibilityGrid->addWidget(new QLabel(
            QStringLiteral("无其它可配置控件"),
            m_otherVisibilityListHost), 0, 0);
    }
}

void FeatureSwitchWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshButtonVisibilityList();
    loadButtonVisibilityState();
    loadTechSliderEditState();
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

void FeatureSwitchWidget::loadButtonVisibilityState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        if (it->visible) {
            it->visible->setChecked(settings.value(it.key(), true).toBool());
        }
    }
    for (auto it = m_otherVisibilityCheckboxes.begin(); it != m_otherVisibilityCheckboxes.end(); ++it) {
        it.value()->setChecked(settings.value(it.key(), true).toBool());
    }
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        if (it->visible) {
            it->visible->setChecked(settings.value(it.key(), true).toBool());
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonModbusMapping"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        const QString &name = it.key();
        const QList<MainWindow::ModbusRegisterSpec> readSpecs = loadRegisterSpecs(
            settings, name + QStringLiteral("_read"), {});
        const QList<MainWindow::ModbusRegisterSpec> writeSpecs = loadRegisterSpecs(
            settings, name + QStringLiteral("_write"), {});

        const int readCount = qMin(it->reads.size(), readSpecs.size());
        for (int idx = 0; idx < readCount; ++idx) {
            applyRegisterSpecToEdits(readSpecs.at(idx), it->reads[idx]);
        }
        const int writeCount = qMin(it->writes.size(), writeSpecs.size());
        for (int idx = 0; idx < writeCount; ++idx) {
            applyRegisterSpecToEdits(writeSpecs.at(idx), it->writes[idx]);
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonSecondStateDarkening"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        if (it->secondStateDim) {
            it->secondStateDim->setChecked(settings.value(it.key(), it->secondStateDim->isChecked()).toBool());
        }
    }
    settings.endGroup();
}

void FeatureSwitchWidget::saveButtonVisibilityState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        if (it->visible) {
            settings.setValue(it.key(), it->visible->isChecked());
        }
    }
    for (auto it = m_otherVisibilityCheckboxes.begin(); it != m_otherVisibilityCheckboxes.end(); ++it) {
        settings.setValue(it.key(), it.value()->isChecked());
    }
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        if (it->visible) {
            settings.setValue(it.key(), it->visible->isChecked());
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonModbusMapping"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        const QString &name = it.key();
        const QString readPrefixBase = name + QStringLiteral("_read");
        for (int idx = 0; idx < it->reads.size(); ++idx) {
            const MainWindow::ModbusRegisterSpec readSpec = readRegisterSpecFromEdits(it->reads.at(idx));
            const QString readPrefix = QStringLiteral("%1_%2").arg(readPrefixBase).arg(idx + 1);
            settings.setValue(readPrefix + QStringLiteral("_device"), readSpec.device);
            settings.setValue(readPrefix + QStringLiteral("_addr"), readSpec.address);
            settings.setValue(readPrefix + QStringLiteral("_bit"), readSpec.bit);
            settings.setValue(readPrefix + QStringLiteral("_value1"), readSpec.value1);
            settings.setValue(readPrefix + QStringLiteral("_value2"), readSpec.value2);
            settings.setValue(readPrefix + QStringLiteral("_value3"), readSpec.value3);
            settings.setValue(readPrefix + QStringLiteral("_value"), readSpec.value1);
            settings.setValue(readPrefix, composeLegacyRegisterString(readSpec));
            if (idx == 0) {
                settings.setValue(readPrefixBase + QStringLiteral("_device"), readSpec.device);
                settings.setValue(readPrefixBase + QStringLiteral("_addr"), readSpec.address);
                settings.setValue(readPrefixBase + QStringLiteral("_bit"), readSpec.bit);
                settings.setValue(readPrefixBase + QStringLiteral("_value1"), readSpec.value1);
                settings.setValue(readPrefixBase + QStringLiteral("_value2"), readSpec.value2);
                settings.setValue(readPrefixBase + QStringLiteral("_value3"), readSpec.value3);
                settings.setValue(readPrefixBase + QStringLiteral("_value"), readSpec.value1);
                settings.setValue(readPrefixBase, composeLegacyRegisterString(readSpec));
            }
        }

        const QString writePrefixBase = name + QStringLiteral("_write");
        for (int idx = 0; idx < it->writes.size(); ++idx) {
            const MainWindow::ModbusRegisterSpec writeSpec = readRegisterSpecFromEdits(it->writes.at(idx));
            const QString writePrefix = QStringLiteral("%1_%2").arg(writePrefixBase).arg(idx + 1);
            settings.setValue(writePrefix + QStringLiteral("_device"), writeSpec.device);
            settings.setValue(writePrefix + QStringLiteral("_addr"), writeSpec.address);
            settings.setValue(writePrefix + QStringLiteral("_bit"), writeSpec.bit);
            settings.setValue(writePrefix + QStringLiteral("_value1"), writeSpec.value1);
            settings.setValue(writePrefix + QStringLiteral("_value2"), writeSpec.value2);
            settings.setValue(writePrefix + QStringLiteral("_value3"), writeSpec.value3);
            settings.setValue(writePrefix + QStringLiteral("_value"), writeSpec.value1);
            settings.setValue(writePrefix, composeLegacyRegisterString(writeSpec));
            if (idx == 0) {
                settings.setValue(writePrefixBase + QStringLiteral("_device"), writeSpec.device);
                settings.setValue(writePrefixBase + QStringLiteral("_addr"), writeSpec.address);
                settings.setValue(writePrefixBase + QStringLiteral("_bit"), writeSpec.bit);
                settings.setValue(writePrefixBase + QStringLiteral("_value1"), writeSpec.value1);
                settings.setValue(writePrefixBase + QStringLiteral("_value2"), writeSpec.value2);
                settings.setValue(writePrefixBase + QStringLiteral("_value3"), writeSpec.value3);
                settings.setValue(writePrefixBase + QStringLiteral("_value"), writeSpec.value1);
                settings.setValue(writePrefixBase, composeLegacyRegisterString(writeSpec));
            }
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonSecondStateDarkening"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        if (it->secondStateDim) {
            settings.setValue(it.key(), it->secondStateDim->isChecked());
        }
    }
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
        {"robot_ArcGauge_SixAxis6", qMakePair(-90.0, 90.0)},
        {"agv_park_out_trigger_length", qMakePair(100.0, 1100.0)}
    };

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");

    const auto loadRangeEdits = [&](const QString &key, QLineEdit *minEdit, QLineEdit *maxEdit) {
        if (!minEdit || !maxEdit) {
            return;
        }
        const QString keyMin = QString("%1_min").arg(key);
        const QString keyMax = QString("%1_max").arg(key);
        const auto range = defaultRanges.value(key, qMakePair(0.0, 100.0));
        const QVariant minVar = settings.value(keyMin);
        const QVariant maxVar = settings.value(keyMax);
        const double minVal = minVar.isValid() ? minVar.toDouble() : range.first;
        const double maxVal = maxVar.isValid() ? maxVar.toDouble() : range.second;
        minEdit->setText(QString::number(minVal));
        maxEdit->setText(QString::number(maxVal));
    };

    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        loadRangeEdits(it.key(), it->minEdit, it->maxEdit);
    }
    for (auto it = m_limitEdits.begin(); it != m_limitEdits.end(); ++it) {
        loadRangeEdits(it.key(), it.value().minEdit, it.value().maxEdit);
    }
    settings.endGroup();
}

void FeatureSwitchWidget::saveSliderLimitState()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");

    const auto saveRangeEdits = [&](const QString &key, QLineEdit *minEdit, QLineEdit *maxEdit) {
        if (!minEdit || !maxEdit) {
            return;
        }
        settings.setValue(QString("%1_min").arg(key), minEdit->text().toDouble());
        settings.setValue(QString("%1_max").arg(key), maxEdit->text().toDouble());
    };

    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        saveRangeEdits(it.key(), it->minEdit, it->maxEdit);
    }
    for (auto it = m_limitEdits.begin(); it != m_limitEdits.end(); ++it) {
        saveRangeEdits(it.key(), it.value().minEdit, it.value().maxEdit);
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
    saveTechSliderEditState();

    saveInclinometerThresholdState();

    saveButtonVisibilityState();

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
    refreshButtonVisibilityList();
    loadCurrentState();
}

void FeatureSwitchWidget::onToggleAll(bool checked)
{
    for (auto cb : m_bigCheckboxes) cb->setChecked(checked);
    for (auto cb : m_smallCheckboxes) cb->setChecked(checked);
}
