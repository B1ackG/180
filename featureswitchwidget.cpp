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
#include <QGuiApplication>
#include <QScreen>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QIntValidator>
#include <QFrame>
#include <QDialog>
#include <QScrollBar>
#include <QSet>

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

/** 弹窗 / 虚拟键盘 / 功能控制台自身，不进入「其他可见性」以免干扰配置 */
bool isConsoleNoiseControl(MainWindow *mainWindow, const QString &objectName)
{
    if (objectName.startsWith(QStringLiteral("qt_"))) {
        return true;
    }
    if (!mainWindow) {
        return false;
    }
    QWidget *widget = mainWindow->findChild<QWidget*>(objectName);
    if (!widget) {
        return false;
    }
    for (QWidget *p = widget->parentWidget(); p && p != mainWindow; p = p->parentWidget()) {
        if (qobject_cast<QDialog*>(p) || qobject_cast<QMessageBox*>(p)) {
            return true;
        }
        if (qobject_cast<FeatureSwitchWidget*>(p) || p->inherits("FeatureSwitchWidget")) {
            return true;
        }
        if (p->inherits("TechVirtualKeyboard")) {
            return true;
        }
    }
    return false;
}

QLabel *makeHintLabel(const QString &text, QWidget *parent = nullptr)
{
    auto *hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("consoleHint"));
    return hint;
}

QString smallFeatureGroupTitle(const QString &key)
{
    const QString prefix = key.section(QLatin1Char('.'), 0, 0);
    static const QMap<QString, QString> titles = {
        {QStringLiteral("startup"), QStringLiteral("启动")},
        {QStringLiteral("ui"), QStringLiteral("界面")},
        {QStringLiteral("permission"), QStringLiteral("权限")},
        {QStringLiteral("records"), QStringLiteral("操作记录")},
        {QStringLiteral("tcp"), QStringLiteral("TCP 上报")},
        {QStringLiteral("modbus_main"), QStringLiteral("主控 Modbus")},
        {QStringLiteral("modbus_agv"), QStringLiteral("AGV Modbus")},
        {QStringLiteral("agv"), QStringLiteral("AGV 显示")},
        {QStringLiteral("motion"), QStringLiteral("运动控制")},
        {QStringLiteral("input"), QStringLiteral("输入设备")},
        {QStringLiteral("alarm"), QStringLiteral("报警")},
        {QStringLiteral("debug"), QStringLiteral("调试")},
    };
    return titles.value(prefix, prefix.isEmpty() ? QStringLiteral("其它") : prefix);
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

QComboBox *makeSpareNameDeviceCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->addItems({QStringLiteral("无"), QStringLiteral("主控"), QStringLiteral("AGV")});
    combo->setFixedWidth(58);
    return combo;
}

QLineEdit *makeSpareNameAddrEdit(QWidget *parent, const QString &lineEditStyle)
{
    auto *edit = new QLineEdit(parent);
    edit->setPlaceholderText(QStringLiteral("起始寄存器"));
    edit->setFixedWidth(88);
    edit->setStyleSheet(lineEditStyle);
    return edit;
}

void applySpareNameRegisterToEdits(QComboBox *device,
                                   QLineEdit *addr,
                                   const QString &deviceValue,
                                   const QString &addrValue)
{
    if (!device || !addr) {
        return;
    }
    QSignalBlocker blockerDevice(device);
    QSignalBlocker blockerAddr(addr);
    const int deviceIndex = device->findText(deviceValue.trimmed().isEmpty() ? QStringLiteral("无") : deviceValue.trimmed());
    device->setCurrentIndex(deviceIndex >= 0 ? deviceIndex : 0);
    addr->setText(addrValue.trimmed());
    const bool enabled = device->currentText() != QStringLiteral("无");
    addr->setEnabled(enabled);
    if (!enabled) {
        addr->clear();
    }
}

void wireSpareNameRegisterRow(QComboBox *device, QLineEdit *addr)
{
    if (!device || !addr) {
        return;
    }
    const auto updateEnabled = [device, addr]() {
        const bool enabled = device->currentText() != QStringLiteral("无");
        addr->setEnabled(enabled);
        if (!enabled) {
            addr->clear();
        }
    };
    QObject::connect(device, QOverload<int>::of(&QComboBox::currentIndexChanged), device,
            [updateEnabled](int) { updateEnabled(); });
    updateEnabled();
}

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
    // 必须是独立窗口：若只作为 MainWindow 子控件 show()，底栏容易被裁切到屏幕外。
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setWindowModality(Qt::NonModal);

    setupUI();
    loadCurrentState();
    m_virtualKeyboard = new TechVirtualKeyboard(this);

    const QList<QLineEdit*> edits = findChildren<QLineEdit*>();
    for (QLineEdit *edit : edits) {
        edit->installEventFilter(this);
    }

    setWindowTitle(QStringLiteral("功能开关管理 (厂家权限)"));

    // 深色工业风：统一输入框 / 下拉 / 页脚层级，减少视觉噪音
    setStyleSheet(QStringLiteral(
        "FeatureSwitchWidget { background-color: #12121c; }"
        "QWidget { background-color: transparent; color: #d7f7ff; font-family: 'Microsoft YaHei UI'; font-size: 13px; }"
        "QLabel { color: #c8eef8; background: transparent; }"
        "QGroupBox { border: 1px solid #1e6a88; border-radius: 8px; margin-top: 14px; font-weight: bold; color: #7fd4f0; padding: 12px 10px 10px 10px; background-color: rgba(18, 36, 52, 160); }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #4ec8ef; }"
        "QCheckBox { spacing: 8px; padding: 4px 2px; color: #d7f7ff; font-weight: normal; background: transparent; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #2aa0c8; border-radius: 3px; background: #0b1c28; }"
        "QCheckBox::indicator:checked { background-color: #1aa0d0; border-color: #5ad8ff; }"
        "QCheckBox::indicator:hover { border-color: #5ad8ff; }"
        "QLineEdit { background-color: #0b1c28; color: #ffffff; border: 1px solid #1e6a88; border-radius: 4px; padding: 4px 6px; selection-background-color: #1a6a90; }"
        "QLineEdit:focus { border-color: #4ec8ef; }"
        "QLineEdit:disabled { color: #667788; background-color: #0a141c; }"
        "QComboBox { background-color: #0b1c28; color: #ffffff; border: 1px solid #1e6a88; border-radius: 4px; padding: 3px 6px; min-height: 24px; }"
        "QComboBox:hover { border-color: #4ec8ef; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background-color: #0f2430; color: #ffffff; selection-background-color: #1a6a90; border: 1px solid #1e6a88; }"
        "QPushButton { background-color: #143a52; border: 1px solid #2a88aa; border-radius: 5px; padding: 8px 16px; color: #e8fbff; font-weight: normal; }"
        "QPushButton:hover { background-color: #1b5270; border-color: #4ec8ef; }"
        "QPushButton:pressed { background-color: #0f2e42; }"
        "QPushButton#btnPrimary { background-color: #1a7a4a; border-color: #3dca7a; font-weight: bold; }"
        "QPushButton#btnPrimary:hover { background-color: #21965c; }"
        "QPushButton#btnAccent { background-color: #1a5f9a; border-color: #4aa8e0; font-weight: bold; }"
        "QPushButton#btnAccent:hover { background-color: #2474b8; }"
        "QPushButton#btnDanger { background-color: #8a2e2e; border-color: #d06060; font-weight: bold; }"
        "QPushButton#btnDanger:hover { background-color: #a83838; }"
        "QScrollArea { border: none; background-color: transparent; }"
        "QScrollBar:vertical { background: #0d1620; width: 10px; margin: 2px; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: #2a6a88; border-radius: 5px; min-height: 28px; }"
        "QScrollBar::handle:vertical:hover { background: #3a8aaa; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QTabWidget::pane { border: 1px solid #1e6a88; border-radius: 8px; background-color: #101820; top: -1px; }"
        "QTabBar::tab { background-color: #0f2430; color: #8ec8dc; border: 1px solid #1a5570; border-bottom: none; padding: 9px 20px; margin-right: 2px; border-top-left-radius: 6px; border-top-right-radius: 6px; min-width: 72px; }"
        "QTabBar::tab:selected { background-color: #164860; color: #ffffff; font-weight: bold; border-color: #3a9cc0; }"
        "QTabBar::tab:hover:!selected { background-color: #143848; color: #d7f7ff; }"
        "QFrame#consoleFooter { background-color: #0c141c; border-top: 1px solid #1e6a88; }"
        "QFrame#consoleHeader { background-color: #0c2030; border: 1px solid #1e6a88; border-radius: 8px; }"
        "QLabel#consoleTitle { color: #5ad8ff; font-size: 22px; font-weight: bold; }"
        "QLabel#consoleSubtitle { color: #7eb8d4; font-size: 12px; font-weight: normal; }"
        "QLabel#consoleBadge { color: #0a1620; background-color: #4ec8ef; border-radius: 3px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
        "QWidget#modbusCard { background-color: #0a1c28; border: 1px solid #1e5570; border-radius: 6px; }"
        "QLabel#cardTitle { color: #e8fbff; font-weight: bold; font-size: 13px; }"
        "QLabel#cardMeta { color: #6a9eb0; font-size: 11px; font-weight: normal; }"
        "QLabel#sectionLabel { color: #4ec8ef; font-size: 11px; font-weight: bold; padding-top: 4px; }"
        "QLabel#consoleHint { color: #7eb8d4; font-size: 12px; font-weight: normal; padding: 2px 0 6px 0; }"
    ));

    // 按可用屏幕区域限制尺寸，避免示教器分辨率下底栏（保存/退出）被裁掉
    QSize target(1100, 800);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        target.setWidth(qMin(1100, qMax(720, avail.width() - 48)));
        target.setHeight(qMin(800, qMax(520, avail.height() - 48)));
    }
    resize(target);
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
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(10);

    // 顶栏：标题 + 厂家权限标识
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("consoleHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(12);

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("系统功能控制台"), header);
    title->setObjectName(QStringLiteral("consoleTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("厂家运行时配置 · 修改后可「立即生效」或「保存并写入 INI」"), header);
    subtitle->setObjectName(QStringLiteral("consoleSubtitle"));
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);

    auto *badge = new QLabel(QStringLiteral("厂家权限"), header);
    badge->setObjectName(QStringLiteral("consoleBadge"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedHeight(24);

    headerLayout->addLayout(titleBlock, 1);
    headerLayout->addWidget(badge, 0, Qt::AlignTop);
    mainLayout->addWidget(header);

    auto *tabs = new QTabWidget(this);
    tabs->setDocumentMode(false);
    tabs->setUsesScrollButtons(true);

    auto makeTabLayout = [tabs](const QString &tabTitle) -> QVBoxLayout* {
        auto *page = new QWidget(tabs);
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);

        auto *scroll = new QScrollArea(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *scrollContent = new QWidget(scroll);
        auto *contentLayout = new QVBoxLayout(scrollContent);
        contentLayout->setContentsMargins(12, 12, 12, 16);
        contentLayout->setSpacing(10);

        scroll->setWidget(scrollContent);
        pageLayout->addWidget(scroll);
        tabs->addTab(page, tabTitle);
        return contentLayout;
    };

    QVBoxLayout *baseLayout = makeTabLayout(QStringLiteral("基础开关"));
    QVBoxLayout *commLayout = makeTabLayout(QStringLiteral("通信参数"));
    QVBoxLayout *displayLayout = makeTabLayout(QStringLiteral("显示范围"));
    QVBoxLayout *controlLayout = makeTabLayout(QStringLiteral("控件配置"));
    QVBoxLayout *logsLayout = makeTabLayout(QStringLiteral("日志调试"));

    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();

    // 大功能组
    QGroupBox *bigGroup = new QGroupBox(QStringLiteral("核心功能模块"));
    QVBoxLayout *bigOuter = new QVBoxLayout(bigGroup);
    bigOuter->addWidget(makeHintLabel(
        QStringLiteral("关闭大模块将连带禁用其下全部子功能。悬停复选框可查看内部键名。"), bigGroup));
    QGridLayout *bigLayout = new QGridLayout();
    bigLayout->setHorizontalSpacing(16);
    bigLayout->setVerticalSpacing(4);
    bigOuter->addLayout(bigLayout);

    const QMap<QString, QString> desc = {
        {QStringLiteral("startup_checks"), QStringLiteral("启动自检")},
        {QStringLiteral("ui_navigation"), QStringLiteral("界面导航")},
        {QStringLiteral("permission_system"), QStringLiteral("权限体系")},
        {QStringLiteral("operation_records"), QStringLiteral("操作记录")},
        {QStringLiteral("tcp_transmission"), QStringLiteral("TCP 上报")},
        {QStringLiteral("modbus_main"), QStringLiteral("主控 Modbus")},
        {QStringLiteral("modbus_agv"), QStringLiteral("AGV Modbus")},
        {QStringLiteral("motion_control"), QStringLiteral("运动控制")},
        {QStringLiteral("input_devices"), QStringLiteral("输入设备")},
        {QStringLiteral("alarm_system"), QStringLiteral("报警系统")},
    };

    QStringList bigKeys = mgr->allBigFeatures().values();
    bigKeys.sort();
    for (int i = 0; i < bigKeys.size(); ++i) {
        const QString key = bigKeys.at(i);
        QCheckBox *cb = new QCheckBox(desc.value(key, key));
        cb->setToolTip(key);
        bigLayout->addWidget(cb, i / 2, i % 2);
        m_bigCheckboxes[key] = cb;
    }
    baseLayout->addWidget(bigGroup);

    // 小功能：按类别分组
    QGroupBox *smallGroup = new QGroupBox(QStringLiteral("子功能细项"));
    QVBoxLayout *smallOuter = new QVBoxLayout(smallGroup);
    smallOuter->addWidget(makeHintLabel(
        QStringLiteral("按业务类别分组；本机 / 远程 TCP 模拟器互斥，只能启用其一。"), smallGroup));

    const QMap<QString, QString> sdesc = {
        {QStringLiteral("startup.clear_servo_alarm"), QStringLiteral("启动清除伺服报警")},
        {QStringLiteral("startup.write_registers"), QStringLiteral("启动写寄存器")},
        {QStringLiteral("startup.log_report"), QStringLiteral("启动日志报告")},
        {QStringLiteral("ui.styles"), QStringLiteral("界面样式")},
        {QStringLiteral("ui.animations"), QStringLiteral("界面动画")},
        {QStringLiteral("ui.virtual_keyboard"), QStringLiteral("虚拟键盘")},
        {QStringLiteral("permission.admin_login"), QStringLiteral("管理员登录")},
        {QStringLiteral("records.filter_export"), QStringLiteral("记录筛选与导出")},
        {QStringLiteral("tcp.send_all"), QStringLiteral("TCP 全量发送")},
        {QStringLiteral("tcp.local_simulator"), QStringLiteral("本机 TCP 模拟器 (127.0.0.1)")},
        {QStringLiteral("tcp.remote_simulator"), QStringLiteral("远程 TCP 模拟器 (192.168.x.xx)")},
        {QStringLiteral("modbus_main.polling"), QStringLiteral("主控轮询")},
        {QStringLiteral("modbus_main.float_reading"), QStringLiteral("浮点解析")},
        {QStringLiteral("modbus_main.read_enabled"), QStringLiteral("主控 Modbus 读使能")},
        {QStringLiteral("modbus_main.write_enabled"), QStringLiteral("主控 Modbus 写使能")},
        {QStringLiteral("modbus_main.read_logs"), QStringLiteral("主设备 Modbus 读日志")},
        {QStringLiteral("modbus_main.write_logs"), QStringLiteral("主设备 Modbus 写日志")},
        {QStringLiteral("modbus_agv.read_enabled"), QStringLiteral("AGV Modbus 读使能")},
        {QStringLiteral("modbus_agv.write_enabled"), QStringLiteral("AGV Modbus 写使能")},
        {QStringLiteral("modbus_agv.read_logs"), QStringLiteral("AGV Modbus 读日志")},
        {QStringLiteral("modbus_agv.write_logs"), QStringLiteral("AGV Modbus 写日志")},
        {QStringLiteral("agv.fault_codes"), QStringLiteral("AGV 故障码")},
        {QStringLiteral("agv.speed_gauge"), QStringLiteral("AGV 速度表")},
        {QStringLiteral("motion.steering_mode"), QStringLiteral("转向模式")},
        {QStringLiteral("motion.speed_mode"), QStringLiteral("速度模式")},
        {QStringLiteral("motion.step_mode"), QStringLiteral("步进/点动")},
        {QStringLiteral("motion.control_mode_switch"), QStringLiteral("控制模式切换(示教/遥控)")},
        {QStringLiteral("motion.agv_oa_switch"), QStringLiteral("AGV 避障开关")},
        {QStringLiteral("motion.agv_park_switch"), QStringLiteral("AGV 驻车开关")},
        {QStringLiteral("motion.agv_speed_control"), QStringLiteral("AGV 速度调节")},
        {QStringLiteral("motion.agv_angle_control"), QStringLiteral("AGV 角度调节")},
        {QStringLiteral("input.matrix_key"), QStringLiteral("矩阵按键")},
        {QStringLiteral("input.enable_button"), QStringLiteral("使能按钮")},
        {QStringLiteral("alarm.emergency_stop"), QStringLiteral("急停报警")},
        {QStringLiteral("alarm.steering_switch"), QStringLiteral("转向模式切换报警")},
        {QStringLiteral("alarm.popup"), QStringLiteral("报警弹窗显示")},
        {QStringLiteral("alarm.status_logs"), QStringLiteral("报警状态周期日志")},
        {QStringLiteral("debug.qdebug"), QStringLiteral("全局调试输出 (qDebug)")},
    };

    const QSet<QString> logSwitchKeys = {
        QStringLiteral("modbus_main.read_logs"),
        QStringLiteral("modbus_main.write_logs"),
        QStringLiteral("modbus_agv.read_logs"),
        QStringLiteral("modbus_agv.write_logs"),
        QStringLiteral("alarm.status_logs"),
        QStringLiteral("debug.qdebug")
    };

    QStringList smallKeys = mgr->allSmallFeatures().values();
    smallKeys.sort();

    QMap<QString, QStringList> groupedKeys;
    for (const QString &key : smallKeys) {
        if (logSwitchKeys.contains(key)) {
            continue;
        }
        groupedKeys[smallFeatureGroupTitle(key)].append(key);
    }

    const QStringList groupOrder = {
        QStringLiteral("启动"), QStringLiteral("界面"), QStringLiteral("权限"),
        QStringLiteral("操作记录"), QStringLiteral("TCP 上报"),
        QStringLiteral("主控 Modbus"), QStringLiteral("AGV Modbus"), QStringLiteral("AGV 显示"),
        QStringLiteral("运动控制"), QStringLiteral("输入设备"), QStringLiteral("报警"),
    };

    QStringList orderedGroups = groupOrder;
    for (auto it = groupedKeys.constBegin(); it != groupedKeys.constEnd(); ++it) {
        if (!orderedGroups.contains(it.key())) {
            orderedGroups.append(it.key());
        }
    }

    for (const QString &groupTitle : orderedGroups) {
        if (!groupedKeys.contains(groupTitle) || groupedKeys.value(groupTitle).isEmpty()) {
            continue;
        }
        auto *section = new QGroupBox(groupTitle, smallGroup);
        section->setFlat(true);
        auto *grid = new QGridLayout(section);
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(2);
        grid->setContentsMargins(6, 4, 6, 4);

        const QStringList keys = groupedKeys.value(groupTitle);
        for (int i = 0; i < keys.size(); ++i) {
            const QString &key = keys.at(i);
            QCheckBox *cb = new QCheckBox(sdesc.value(key, key), section);
            cb->setToolTip(key);
            grid->addWidget(cb, i / 2, i % 2);
            m_smallCheckboxes[key] = cb;

            if (key == QStringLiteral("tcp.local_simulator")) {
                connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
                    if (checked && m_smallCheckboxes.contains(QStringLiteral("tcp.remote_simulator"))) {
                        m_smallCheckboxes[QStringLiteral("tcp.remote_simulator")]->setChecked(false);
                    }
                });
            } else if (key == QStringLiteral("tcp.remote_simulator")) {
                connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
                    if (checked && m_smallCheckboxes.contains(QStringLiteral("tcp.local_simulator"))) {
                        m_smallCheckboxes[QStringLiteral("tcp.local_simulator")]->setChecked(false);
                    }
                });
            }
        }
        smallOuter->addWidget(section);
    }
    baseLayout->addWidget(smallGroup);

    QGroupBox *logGroup = new QGroupBox(QStringLiteral("日志与调试开关"));
    QVBoxLayout *logOuter = new QVBoxLayout(logGroup);
    logOuter->addWidget(makeHintLabel(
        QStringLiteral("仅影响日志输出量，不影响业务功能本身。生产环境建议保持关闭。"), logGroup));
    QGridLayout *logLayout = new QGridLayout();
    logLayout->setHorizontalSpacing(14);
    logOuter->addLayout(logLayout);

    QStringList logKeys = logSwitchKeys.values();
    logKeys.sort();
    for (int i = 0; i < logKeys.size(); ++i) {
        const QString key = logKeys.at(i);
        QCheckBox *cb = new QCheckBox(sdesc.value(key, key));
        cb->setToolTip(key);
        logLayout->addWidget(cb, i / 2, i % 2);
        m_smallCheckboxes[key] = cb;
    }
    logsLayout->addWidget(logGroup);

    setupNetworkUI(commLayout);
    setupPollingUI(commLayout);
    setupSliderLimitUI(displayLayout);
    setupInclinometerThresholdUI(displayLayout);
    setupButtonVisibilityUI(controlLayout);

    baseLayout->addStretch();
    commLayout->addStretch();
    displayLayout->addStretch();
    controlLayout->addStretch();
    logsLayout->addStretch();

    mainLayout->addWidget(tabs, 1);

    // 底栏：批量操作 | 生效保存 | 退出（固定高度，不被 Tab 内容挤没）
    auto *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("consoleFooter"));
    footer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    footer->setMinimumHeight(56);
    auto *btnLayout = new QHBoxLayout(footer);
    btnLayout->setContentsMargins(12, 10, 12, 10);
    btnLayout->setSpacing(8);

    QPushButton *btnAll = new QPushButton(QStringLiteral("开启全部"), footer);
    connect(btnAll, &QPushButton::clicked, this, [this]() { onToggleAll(true); });

    QPushButton *btnNone = new QPushButton(QStringLiteral("关闭全部"), footer);
    connect(btnNone, &QPushButton::clicked, this, [this]() { onToggleAll(false); });

    QPushButton *btnReload = new QPushButton(QStringLiteral("撤销修改"), footer);
    btnReload->setToolTip(QStringLiteral("从当前 INI 重新加载，丢弃未保存的界面修改"));
    connect(btnReload, &QPushButton::clicked, this, &FeatureSwitchWidget::onReload);

    QPushButton *btnApply = new QPushButton(QStringLiteral("立即生效"), footer);
    btnApply->setObjectName(QStringLiteral("consoleBtnApply"));
    btnApply->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1a5f9a; border: 1px solid #4aa8e0; border-radius: 5px;"
        " padding: 8px 16px; color: #e8fbff; font-weight: bold; }"
        "QPushButton:hover { background-color: #2474b8; }"));
    btnApply->setToolTip(QStringLiteral("写入配置并通知主窗口立刻应用，不关闭本页"));
    connect(btnApply, &QPushButton::clicked, this, &FeatureSwitchWidget::onApply);

    QPushButton *btnSave = new QPushButton(QStringLiteral("保存并写入 INI"), footer);
    btnSave->setObjectName(QStringLiteral("consoleBtnSave"));
    btnSave->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1a7a4a; border: 1px solid #3dca7a; border-radius: 5px;"
        " padding: 8px 16px; color: #e8fbff; font-weight: bold; }"
        "QPushButton:hover { background-color: #21965c; }"));
    connect(btnSave, &QPushButton::clicked, this, &FeatureSwitchWidget::onSave);

    QPushButton *btnClose = new QPushButton(QStringLiteral("退出"), footer);
    btnClose->setObjectName(QStringLiteral("consoleBtnClose"));
    btnClose->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #8a2e2e; border: 1px solid #d06060; border-radius: 5px;"
        " padding: 8px 16px; color: #e8fbff; font-weight: bold; }"
        "QPushButton:hover { background-color: #a83838; }"));
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    btnLayout->addWidget(btnAll);
    btnLayout->addWidget(btnNone);
    btnLayout->addWidget(btnReload);
    btnLayout->addStretch();
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnClose);
    mainLayout->addWidget(footer, 0);
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
    loadNetworkState();
    loadPollingState();
    loadSliderLimitState();
    loadTechSliderEditState();
    loadInclinometerThresholdState();
    loadButtonVisibilityState();
}

void FeatureSwitchWidget::setupNetworkUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *netGroup = new QGroupBox(QStringLiteral("网络地址"));
    QVBoxLayout *netLayout = new QVBoxLayout(netGroup);
    netLayout->addWidget(makeHintLabel(
        QStringLiteral("格式均为 192.168.x.xx；与「本机 / 远程 TCP 模拟器」开关配合。立即生效后写入 config.ini，WIN7 IP 会加入日志传输白名单。"),
        netGroup));

    auto addIpRow = [&](const QString &prefixLabel,
                        QLineEdit *&subnetEdit,
                        QLineEdit *&hostEdit) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *prefix = new QLabel(prefixLabel);
        prefix->setMinimumWidth(150);

        subnetEdit = new QLineEdit();
        subnetEdit->setPlaceholderText(QStringLiteral("1"));
        subnetEdit->setFixedWidth(52);
        subnetEdit->setAlignment(Qt::AlignCenter);
        subnetEdit->setValidator(new QIntValidator(0, 255, netGroup));
        subnetEdit->installEventFilter(this);

        QLabel *dot = new QLabel(QStringLiteral("."));
        dot->setAlignment(Qt::AlignCenter);
        dot->setFixedWidth(10);

        hostEdit = new QLineEdit();
        hostEdit->setPlaceholderText(QStringLiteral("70"));
        hostEdit->setFixedWidth(52);
        hostEdit->setAlignment(Qt::AlignCenter);
        hostEdit->setValidator(new QIntValidator(0, 255, netGroup));
        hostEdit->installEventFilter(this);

        row->addWidget(prefix);
        row->addWidget(subnetEdit);
        row->addWidget(dot);
        row->addWidget(hostEdit);
        row->addStretch();
        netLayout->addLayout(row);
    };

    addIpRow(QStringLiteral("WIN7_IP: 192.168."), m_editWin7Subnet, m_editWin7Host);
    addIpRow(QStringLiteral("远程模拟器: 192.168."), m_editSimSubnet, m_editSimHost);

    scrollLayout->addWidget(netGroup);
}

void FeatureSwitchWidget::setupPollingUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *pollGroup = new QGroupBox(QStringLiteral("通信轮询参数"));
    QVBoxLayout *pollLayout = new QVBoxLayout(pollGroup);
    pollLayout->addWidget(makeHintLabel(
        QStringLiteral("单位均为毫秒（ms），除非另行说明。设备状态数量建议保持默认 85。"), pollGroup));

    m_cbUiStateSync = new QCheckBox(QStringLiteral("启用控件状态同步"));
    pollLayout->addWidget(m_cbUiStateSync);

    auto addPollGrid = [&](const QString &sectionTitle,
                           const QList<QPair<QString, QLineEdit**>> &items) {
        auto *section = new QGroupBox(sectionTitle, pollGroup);
        section->setFlat(true);
        auto *grid = new QGridLayout(section);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(8);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);

        for (int i = 0; i < items.size(); ++i) {
            const int row = i / 2;
            const int col = (i % 2) * 2;
            auto *lbl = new QLabel(items.at(i).first, section);
            lbl->setWordWrap(true);
            QLineEdit *&editRef = *items.at(i).second;
            editRef = new QLineEdit(section);
            editRef->setMinimumWidth(88);
            editRef->setMaximumWidth(140);
            editRef->installEventFilter(this);
            grid->addWidget(lbl, row, col);
            grid->addWidget(editRef, row, col + 1);
        }
        pollLayout->addWidget(section);
    };

    addPollGrid(QStringLiteral("主控"), {
        {QStringLiteral("Modbus 轮询"), &m_editMainModbusPoll},
        {QStringLiteral("控件同步轮询"), &m_editMainUiPoll},
        {QStringLiteral("设备状态轮询 (0~84)"), &m_editMainDeviceStatusPoll},
        {QStringLiteral("设备状态起始地址"), &m_editMainDeviceStatusStart},
        {QStringLiteral("设备状态数量"), &m_editMainDeviceStatusCount},
        {QStringLiteral("模式同步起始地址"), &m_editMainControlSyncStart},
        {QStringLiteral("模式同步数量"), &m_editMainControlSyncCount},
        {QStringLiteral("重连间隔"), &m_editMainReconnect},
        {QStringLiteral("示教写权限设备号"), &m_editTeachingWriteDeviceId},
    });

    addPollGrid(QStringLiteral("AGV"), {
        {QStringLiteral("Modbus 轮询"), &m_editAgvPoll},
        {QStringLiteral("重连间隔"), &m_editAgvReconnect},
    });

    scrollLayout->addWidget(pollGroup);
}

void FeatureSwitchWidget::setupSliderLimitUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *arcGroup = new QGroupBox(QStringLiteral("环形仪表显示与范围"));
    QVBoxLayout *arcLayout = new QVBoxLayout(arcGroup);
    arcLayout->addWidget(makeHintLabel(
        QStringLiteral("控制首页 / 六轴页环形仪表是否显示，以及数值 Min/Max（与主界面 objectName 对应）。"),
        arcGroup));

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

    auto *arcGrid = new QGridLayout();
    arcGrid->setHorizontalSpacing(8);
    arcGrid->setVerticalSpacing(6);
    arcLayout->addLayout(arcGrid);

    for (int i = 0; i < arcGaugeNames.size(); ++i) {
        const QString &name = arcGaugeNames.at(i);
        QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"));
        visibleCb->setFixedWidth(52);

        QLabel *lbl = new QLabel(arcLabels.value(name, name));
        lbl->setMinimumWidth(110);
        lbl->setToolTip(name);

        QLineEdit *minEdit = new QLineEdit();
        minEdit->setPlaceholderText(QStringLiteral("Min"));
        minEdit->setFixedWidth(72);
        minEdit->installEventFilter(this);

        QLineEdit *maxEdit = new QLineEdit();
        maxEdit->setPlaceholderText(QStringLiteral("Max"));
        maxEdit->setFixedWidth(72);
        maxEdit->installEventFilter(this);

        const int colBase = (i % 2) * 5;
        const int row = i / 2;
        arcGrid->addWidget(visibleCb, row, colBase);
        arcGrid->addWidget(lbl, row, colBase + 1);
        arcGrid->addWidget(minEdit, row, colBase + 2);
        arcGrid->addWidget(new QLabel(QStringLiteral("~")), row, colBase + 3);
        arcGrid->addWidget(maxEdit, row, colBase + 4);

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

    QGroupBox *otherGroup = new QGroupBox(QStringLiteral("其它参数阈值"));
    QVBoxLayout *otherLayout = new QVBoxLayout(otherGroup);

    const auto addOtherLimitRow = [&](const QString &key, const QString &labelText) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lbl = new QLabel(labelText);
        lbl->setMinimumWidth(220);
        row->addWidget(lbl);

        QLineEdit *minEdit = new QLineEdit();
        minEdit->setPlaceholderText(QStringLiteral("Min"));
        minEdit->setFixedWidth(80);
        minEdit->installEventFilter(this);

        QLineEdit *maxEdit = new QLineEdit();
        maxEdit->setPlaceholderText(QStringLiteral("Max"));
        maxEdit->setFixedWidth(80);
        maxEdit->installEventFilter(this);

        row->addWidget(minEdit);
        row->addWidget(new QLabel(QStringLiteral("~")));
        row->addWidget(maxEdit);
        row->addStretch();
        otherLayout->addLayout(row);
        m_limitEdits[key] = {minEdit, maxEdit};
    };

    addOtherLimitRow(QStringLiteral("agv_park_out_trigger_length"),
                     QStringLiteral("驻车伸出触发长度"));
    addOtherLimitRow(QStringLiteral("weight_overload_limit"),
                     QStringLiteral("负载超限阈值"));
    addOtherLimitRow(QStringLiteral("weight_lock_limit"),
                     QStringLiteral("负载超重阈值"));

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
    if (!style.isEmpty()) {
        edit->setStyleSheet(style);
    }
    edit->installEventFilter(host);
    return edit;
}
} // namespace

void FeatureSwitchWidget::setupTechSliderEditUI(QVBoxLayout *scrollLayout)
{
    QGroupBox *group = new QGroupBox(QStringLiteral("滑块输入显示与范围"));
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->addWidget(makeHintLabel(
        QStringLiteral("设置是否显示及滑块两端范围（Min/Max）；输入框数值范围会与显示范围保持一致。"),
        group));

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *showAll = new QPushButton(QStringLiteral("滑块全部显示"));
    QPushButton *hideAll = new QPushButton(QStringLiteral("滑块全部隐藏"));
    toolbar->addWidget(showAll);
    toolbar->addWidget(hideAll);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    auto *listHost = new QWidget(group);
    auto *listLayout = new QVBoxLayout(listHost);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);

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
            QStringLiteral("未找到滑块输入控件（请确认主窗口已初始化）"), listHost));
    } else {
        for (TechSliderEdit *slider : sliders) {
            const QString name = slider->objectName();
            if (name.isEmpty()) {
                continue;
            }

            QHBoxLayout *row = new QHBoxLayout();
            QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"), listHost);
            visibleCb->setFixedWidth(52);

            QString title = slider->labelText().trimmed();
            if (title.isEmpty()) {
                title = name;
            }
            QLabel *lbl = new QLabel(title, listHost);
            lbl->setMinimumWidth(120);
            lbl->setToolTip(name);

            QLineEdit *displayMin = makeLimitEdit(listHost, QString(), this);
            QLineEdit *displayMax = makeLimitEdit(listHost, QString(), this);
            displayMin->setPlaceholderText(QStringLiteral("Min"));
            displayMax->setPlaceholderText(QStringLiteral("Max"));

            row->addWidget(visibleCb);
            row->addWidget(lbl);
            row->addWidget(displayMin);
            row->addWidget(new QLabel(QStringLiteral("~"), listHost));
            row->addWidget(displayMax);
            row->addStretch();
            listLayout->addLayout(row);

            m_sliderEditEdits[name] = {visibleCb, displayMin, displayMax};
        }
    }

    layout->addWidget(listHost);

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
    QGroupBox *incGroup = new QGroupBox(QStringLiteral("倾角仪显示阈值"));
    QVBoxLayout *incLayout = new QVBoxLayout(incGroup);
    incLayout->addWidget(makeHintLabel(
        QStringLiteral("首页 X/Y 倾角卡片超过该阈值时高亮提示，单位：度（°）。"), incGroup));

    auto addRow = [&](const QString &desc, QLineEdit *&edit) {
        QHBoxLayout *h = new QHBoxLayout();
        h->addWidget(new QLabel(desc));
        edit = new QLineEdit();
        edit->setFixedWidth(120);
        edit->installEventFilter(this);
        h->addWidget(edit);
        h->addStretch();
        incLayout->addLayout(h);
    };

    addRow(QStringLiteral("X 轴阈值"), m_editInclinometerThresholdX);
    addRow(QStringLiteral("Y 轴阈值"), m_editInclinometerThresholdY);

    scrollLayout->addWidget(incGroup);
}

void FeatureSwitchWidget::setupButtonVisibilityUI(QVBoxLayout *scrollLayout)
{
    m_modbusButtonGroup = new QGroupBox(QStringLiteral("Modbus 按键：显示与寄存器"));
    QVBoxLayout *modbusLayout = new QVBoxLayout(m_modbusButtonGroup);
    modbusLayout->addWidget(makeHintLabel(
        QStringLiteral("自动扫描主窗口中带 Modbus 读写的按键。每行：设备 / 寄存器 / 位（可空）/ 值1~3。"
                       "读行用于界面状态同步；默认值来自程序内置逻辑。"),
        m_modbusButtonGroup));

    QHBoxLayout *modbusToolbar = new QHBoxLayout();
    QPushButton *modbusShowAll = new QPushButton(QStringLiteral("全部显示"));
    QPushButton *modbusHideAll = new QPushButton(QStringLiteral("全部隐藏"));
    modbusToolbar->addWidget(modbusShowAll);
    modbusToolbar->addWidget(modbusHideAll);
    modbusToolbar->addStretch();
    modbusLayout->addLayout(modbusToolbar);

    QScrollArea *modbusScroll = new QScrollArea();
    modbusScroll->setWidgetResizable(true);
    // 只用上限：过大的 minimumHeight 会抬高整页最小高度，把底栏挤出窗口
    modbusScroll->setMaximumHeight(420);
    modbusScroll->setFrameShape(QFrame::NoFrame);
    m_modbusButtonListHost = new QWidget();
    m_modbusButtonListLayout = new QVBoxLayout(m_modbusButtonListHost);
    m_modbusButtonListLayout->setSpacing(10);
    m_modbusButtonListLayout->setContentsMargins(2, 2, 8, 2);
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

    m_otherVisibilityGroup = new QGroupBox(QStringLiteral("其它控件可见性"));
    QVBoxLayout *otherLayout = new QVBoxLayout(m_otherVisibilityGroup);
    otherLayout->addWidget(makeHintLabel(
        QStringLiteral("无 Modbus 映射的导航 / 模式等控件，仅控制是否显示。"
                       "环形仪表与滑块输入请到「显示范围」页配置。弹窗临时按钮已自动隐藏。"),
        m_otherVisibilityGroup));

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
    otherScroll->setFrameShape(QFrame::NoFrame);
    m_otherVisibilityListHost = new QWidget();
    m_otherVisibilityGrid = new QGridLayout(m_otherVisibilityListHost);
    m_otherVisibilityGrid->setHorizontalSpacing(12);
    m_otherVisibilityGrid->setVerticalSpacing(4);
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

    const QString lineEditStyle; // 使用全局样式，避免卡片内输入框风格割裂

    MappingConfig *mapping = MappingConfig::instance();
    int modbusCount = 0;
    int otherRow = 0;

    for (int i = 0; i < buttons.size(); ++i) {
        const MainWindow::ControllableButtonInfo &info = buttons.at(i);
        if (shouldSkipControllable(info)) {
            continue;
        }
        if (!hasModbusOperation(info) && isConsoleNoiseControl(mainWindow, info.objectName)) {
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
            card->setObjectName(QStringLiteral("modbusCard"));
            QVBoxLayout *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 8, 10, 8);
            cardLayout->setSpacing(5);

            QHBoxLayout *titleRow = new QHBoxLayout();
            QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"), card);
            visibleCb->setChecked(visibleDefault);

            auto *titleBlock = new QVBoxLayout();
            titleBlock->setSpacing(1);
            QLabel *titleLbl = new QLabel(
                visibleText.isEmpty() ? kind : visibleText, card);
            titleLbl->setObjectName(QStringLiteral("cardTitle"));
            QLabel *metaLbl = new QLabel(
                QStringLiteral("%1 · %2").arg(kind, objectName), card);
            metaLbl->setObjectName(QStringLiteral("cardMeta"));
            metaLbl->setToolTip(objectName);
            titleBlock->addWidget(titleLbl);
            titleBlock->addWidget(metaLbl);

            titleRow->addWidget(visibleCb, 0, Qt::AlignTop);
            titleRow->addLayout(titleBlock, 1);
            cardLayout->addLayout(titleRow);

            QCheckBox *secondStateDimCb = nullptr;
            QComboBox *nameState1Device = nullptr;
            QLineEdit *nameState1Addr = nullptr;
            QComboBox *nameState2Device = nullptr;
            QLineEdit *nameState2Addr = nullptr;
            if (objectName == QStringLiteral("techBtn_spare_1")
                || objectName == QStringLiteral("techBtn_spare_2")) {
                settings.beginGroup(QStringLiteral("ButtonSecondStateDarkening"));
                const bool dimEnabled = settings.value(objectName, true).toBool();
                settings.endGroup();

                secondStateDimCb = new QCheckBox(QStringLiteral("第二态 UI 变暗"), card);
                secondStateDimCb->setChecked(dimEnabled);
                secondStateDimCb->setStyleSheet(QStringLiteral("color: #ffdd88;"));
                cardLayout->addWidget(secondStateDimCb);

                QLabel *nameHint = new QLabel(
                    QStringLiteral("多态名称：Modbus UTF-8 字符串，从起始寄存器起连续读 15 个寄存器（高字节在前，与 ModbusTCPAssistant 一致）"),
                    card);
                nameHint->setWordWrap(true);
                nameHint->setObjectName(QStringLiteral("consoleHint"));
                cardLayout->addWidget(nameHint);

                nameState1Device = makeSpareNameDeviceCombo(card);
                nameState1Addr = makeSpareNameAddrEdit(card, lineEditStyle);
                nameState1Addr->installEventFilter(this);
                wireSpareNameRegisterRow(nameState1Device, nameState1Addr);
                QHBoxLayout *name1Row = new QHBoxLayout();
                name1Row->addWidget(new QLabel(QStringLiteral("第1态名称"), card));
                name1Row->addWidget(nameState1Device);
                name1Row->addWidget(new QLabel(QStringLiteral("起始寄存器"), card));
                name1Row->addWidget(nameState1Addr);
                name1Row->addStretch();
                cardLayout->addLayout(name1Row);
                applySpareNameRegisterToEdits(nameState1Device,
                                              nameState1Addr,
                                              settings.value(objectName + QStringLiteral("_name1_device"), QStringLiteral("无")).toString(),
                                              settings.value(objectName + QStringLiteral("_name1_addr")).toString());

                nameState2Device = makeSpareNameDeviceCombo(card);
                nameState2Addr = makeSpareNameAddrEdit(card, lineEditStyle);
                nameState2Addr->installEventFilter(this);
                wireSpareNameRegisterRow(nameState2Device, nameState2Addr);
                QHBoxLayout *name2Row = new QHBoxLayout();
                name2Row->addWidget(new QLabel(QStringLiteral("第2态名称"), card));
                name2Row->addWidget(nameState2Device);
                name2Row->addWidget(new QLabel(QStringLiteral("起始寄存器"), card));
                name2Row->addWidget(nameState2Addr);
                name2Row->addStretch();
                cardLayout->addLayout(name2Row);
                applySpareNameRegisterToEdits(nameState2Device,
                                              nameState2Addr,
                                              settings.value(objectName + QStringLiteral("_name2_device"), QStringLiteral("无")).toString(),
                                              settings.value(objectName + QStringLiteral("_name2_addr")).toString());
            }

            QLabel *mappingHint = makeHintLabel(
                QStringLiteral("状态映射：第1态→值1，第2态→值2，第3态→值3；单一状态只填值1。"), card);
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

            auto *readSection = new QLabel(QStringLiteral("读寄存器"), card);
            readSection->setObjectName(QStringLiteral("sectionLabel"));
            cardLayout->addWidget(readSection);
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
            auto *writeSection = new QLabel(QStringLiteral("写寄存器"), card);
            writeSection->setObjectName(QStringLiteral("sectionLabel"));
            cardLayout->addWidget(writeSection);
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
            m_modbusButtonEdits[objectName] = {
                visibleCb,
                secondStateDimCb,
                readEditsList,
                writeEditsList,
                info.readForUiSync,
                nameState1Device,
                nameState1Addr,
                nameState2Device,
                nameState2Addr
            };
            settings.endGroup();
            ++modbusCount;
        } else {
            const QString label = visibleText.isEmpty() ? kind : visibleText;
            QCheckBox *cb = new QCheckBox(label, m_otherVisibilityListHost);
            cb->setToolTip(QStringLiteral("%1 · %2").arg(kind, objectName));
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

    if (QWidget *host = parentWidget()) {
        const QPoint topLeft = host->mapToGlobal(QPoint(0, 0));
        const QRect hostRect(topLeft, host->size());
        QRect geo = geometry();
        geo.moveCenter(hostRect.center());
        if (QScreen *screen = QGuiApplication::screenAt(hostRect.center())) {
            const QRect avail = screen->availableGeometry();
            if (geo.right() > avail.right()) {
                geo.moveRight(avail.right());
            }
            if (geo.bottom() > avail.bottom()) {
                geo.moveBottom(avail.bottom());
            }
            if (geo.left() < avail.left()) {
                geo.moveLeft(avail.left());
            }
            if (geo.top() < avail.top()) {
                geo.moveTop(avail.top());
            }
        }
        setGeometry(geo);
    }

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
        if (it->nameState1Device && it->nameState1Addr) {
            applySpareNameRegisterToEdits(it->nameState1Device,
                                          it->nameState1Addr,
                                          settings.value(it.key() + QStringLiteral("_name1_device"), QStringLiteral("无")).toString(),
                                          settings.value(it.key() + QStringLiteral("_name1_addr")).toString());
        }
        if (it->nameState2Device && it->nameState2Addr) {
            applySpareNameRegisterToEdits(it->nameState2Device,
                                          it->nameState2Addr,
                                          settings.value(it.key() + QStringLiteral("_name2_device"), QStringLiteral("无")).toString(),
                                          settings.value(it.key() + QStringLiteral("_name2_addr")).toString());
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

        if (it->nameState1Device && it->nameState1Addr) {
            settings.setValue(name + QStringLiteral("_name1_device"), it->nameState1Device->currentText());
            settings.setValue(name + QStringLiteral("_name1_addr"), it->nameState1Addr->text().trimmed());
        }
        if (it->nameState2Device && it->nameState2Addr) {
            settings.setValue(name + QStringLiteral("_name2_device"), it->nameState2Device->currentText());
            settings.setValue(name + QStringLiteral("_name2_addr"), it->nameState2Addr->text().trimmed());
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

void FeatureSwitchWidget::loadNetworkState()
{
    auto fillOctets = [](const QString &ip, QLineEdit *subnetEdit, QLineEdit *hostEdit,
                         const QString &defaultSubnet, const QString &defaultHost) {
        if (!subnetEdit || !hostEdit) {
            return;
        }
        const QStringList parts = ip.split(QLatin1Char('.'));
        const QString subnet = parts.size() >= 3 ? parts.at(2) : defaultSubnet;
        const QString host = parts.size() >= 4 ? parts.at(3) : defaultHost;
        subnetEdit->setText(subnet);
        hostEdit->setText(host);
    };

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Network");
    const QString tcpHost = settings.value("tcp_server_host", "192.168.1.70").toString();
    const QString simHost = settings.value("remote_simulator_host", "192.168.1.70").toString();
    settings.endGroup();

    fillOctets(tcpHost, m_editWin7Subnet, m_editWin7Host, QStringLiteral("1"), QStringLiteral("70"));
    fillOctets(simHost, m_editSimSubnet, m_editSimHost, QStringLiteral("1"), QStringLiteral("70"));
}

bool FeatureSwitchWidget::saveNetworkState()
{
    auto composeIp = [](const QLineEdit *subnetEdit, const QLineEdit *hostEdit) -> QString {
        if (!subnetEdit || !hostEdit) {
            return QString();
        }
        bool subnetOk = false;
        bool hostOk = false;
        const int subnet = subnetEdit->text().trimmed().toInt(&subnetOk);
        const int host = hostEdit->text().trimmed().toInt(&hostOk);
        if (!subnetOk || !hostOk || subnet < 0 || subnet > 255 || host < 0 || host > 255) {
            return QString();
        }
        return QStringLiteral("192.168.%1.%2").arg(subnet).arg(host);
    };

    const QString tcpHost = composeIp(m_editWin7Subnet, m_editWin7Host);
    const QString simHost = composeIp(m_editSimSubnet, m_editSimHost);
    if (tcpHost.isEmpty() || simHost.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("网络配置"), QStringLiteral("IP 段无效，请输入 0-255 的整数"));
        return false;
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Network");
    settings.setValue("tcp_server_host", tcpHost);
    settings.setValue("remote_simulator_host", simHost);
    settings.endGroup();
    settings.sync();
    return true;
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
        {"agv_park_out_trigger_length", qMakePair(100.0, 1100.0)},
        {"weight_overload_limit", qMakePair(0.0, 350.0)},
        {"weight_lock_limit", qMakePair(0.0, 400.0)}
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
    
    // 应用网络与轮询配置
    if (!saveNetworkState()) {
        return;
    }
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
