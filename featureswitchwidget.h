#ifndef FEATURESWITCHWIDGET_H
#define FEATURESWITCHWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QMap>
#include <QVector>

#include "mainwindow.h"

class QLineEdit;
class QComboBox;
class QTextEdit;
class TechVirtualKeyboard;
class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;
class QGridLayout;

/**
 * @brief 功能开关管理页面 (厂家专用)
 * 允许实时调整大/小功能的启用状态，并支持保存到配置文件。
 */
class FeatureSwitchWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FeatureSwitchWidget(QWidget *parent = nullptr);

signals:
    // 通知外部宿主（通常是 MainWindow）重新加载并应用运行时配置
    void runtimeSettingsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onApply();
    void onSave();
    void onReload();
    void onToggleAll(bool checked);

private:
    void setupUI();
    void loadCurrentState();
    void setupPollingUI(QVBoxLayout *scrollLayout);
    void setupSliderLimitUI(QVBoxLayout *scrollLayout);
    void setupTechSliderEditUI(QVBoxLayout *scrollLayout);
    void loadTechSliderEditState();
    void saveTechSliderEditState();
    void setupInclinometerThresholdUI(QVBoxLayout *scrollLayout);
    void setupButtonVisibilityUI(QVBoxLayout *scrollLayout);
    void refreshButtonVisibilityList();
    void loadPollingState();
    void savePollingState();
    void loadSliderLimitState();
    void saveSliderLimitState();
    void loadInclinometerThresholdState();
    void saveInclinometerThresholdState();
    void loadButtonVisibilityState();
    void saveButtonVisibilityState();
    void setupControlNameOverrideUI(QVBoxLayout *scrollLayout);
    void refreshControlNameOverrideTargets(const QList<MainWindow::ControllableButtonInfo> &buttons);
    void loadControlNameOverrideState();
    void saveControlNameOverrideState();

    struct ModbusRegisterEdits {
        QComboBox *device = nullptr;
        QLineEdit *address = nullptr;
        QLineEdit *bit = nullptr;
        QLineEdit *value1 = nullptr;
        QLineEdit *value2 = nullptr;
        QLineEdit *value3 = nullptr;
    };

    struct ModbusButtonEdits {
        QCheckBox *visible = nullptr;
        QCheckBox *secondStateDim = nullptr;
        QVector<ModbusRegisterEdits> reads;
        QVector<ModbusRegisterEdits> writes;
        bool readForUiSync = false;
    };

    ModbusRegisterEdits makeRegisterRowEdits(QWidget *parent, const QString &lineEditStyle);
    void addModbusRegisterRow(QHBoxLayout *row,
                              const QString &label,
                              const ModbusRegisterEdits &edits,
                              const QString &syncHint = QString());
    void applyRegisterSpecToEdits(const MainWindow::ModbusRegisterSpec &spec,
                                  ModbusRegisterEdits &edits);
    MainWindow::ModbusRegisterSpec readRegisterSpecFromEdits(const ModbusRegisterEdits &edits) const;

    QMap<QString, QCheckBox*> m_bigCheckboxes;
    QMap<QString, QCheckBox*> m_smallCheckboxes;

    // 轮询参数输入框
    QLineEdit *m_editMainModbusPoll;
    QLineEdit *m_editMainUiPoll;
    QLineEdit *m_editMainDeviceStatusPoll;
    QLineEdit *m_editMainDeviceStatusStart;
    QLineEdit *m_editMainDeviceStatusCount;
    QLineEdit *m_editMainControlSyncStart;
    QLineEdit *m_editMainControlSyncCount;
    QLineEdit *m_editMainReconnect;
    QLineEdit *m_editAgvPoll;
    QLineEdit *m_editAgvReconnect;
    QLineEdit *m_editTeachingWriteDeviceId;
    QCheckBox *m_cbUiStateSync;

    /** TechArcGauge：显示开关 + 最小/最大范围 */
    struct ArcGaugeEdits {
        QCheckBox *visible = nullptr;
        QLineEdit *minEdit = nullptr;
        QLineEdit *maxEdit = nullptr;
    };
    QMap<QString, ArcGaugeEdits> m_arcGaugeEdits;

    struct SliderEditEdits {
        QCheckBox *visible = nullptr;
        QLineEdit *displayMinEdit = nullptr;
        QLineEdit *displayMaxEdit = nullptr;
    };
    QMap<QString, SliderEditEdits> m_sliderEditEdits;

    /** 非环形仪表的参数范围（如驻车伸出触发长度） */
    struct LimitEdits {
        QLineEdit *minEdit;
        QLineEdit *maxEdit;
    };
    QMap<QString, LimitEdits> m_limitEdits;

    QLineEdit *m_editInclinometerThresholdX = nullptr;
    QLineEdit *m_editInclinometerThresholdY = nullptr;

    QMap<QString, ModbusButtonEdits> m_modbusButtonEdits;
    QMap<QString, QCheckBox*> m_otherVisibilityCheckboxes;

    QGroupBox *m_modbusButtonGroup = nullptr;
    QWidget *m_modbusButtonListHost = nullptr;
    QVBoxLayout *m_modbusButtonListLayout = nullptr;

    QGroupBox *m_otherVisibilityGroup = nullptr;
    QWidget *m_otherVisibilityListHost = nullptr;
    QGridLayout *m_otherVisibilityGrid = nullptr;

    QComboBox *m_controlNameTargetCombo = nullptr;
    QTextEdit *m_controlNameEdit = nullptr;
    QMap<QString, QString> m_controlNameOverrides;

    TechVirtualKeyboard *m_virtualKeyboard = nullptr;
};

#endif // FEATURESWITCHWIDGET_H
