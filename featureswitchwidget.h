#ifndef FEATURESWITCHWIDGET_H
#define FEATURESWITCHWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QMap>

class QLineEdit;
class TechVirtualKeyboard;
class QVBoxLayout;

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
    void setupInclinometerThresholdUI(QVBoxLayout *scrollLayout);
    void loadPollingState();
    void savePollingState();
    void loadSliderLimitState();
    void saveSliderLimitState();
    void loadInclinometerThresholdState();
    void saveInclinometerThresholdState();

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

    // SliderLabel 限制输入框: key -> [minEdit, maxEdit]
    struct LimitEdits {
        QLineEdit *minEdit;
        QLineEdit *maxEdit;
    };
    QMap<QString, LimitEdits> m_limitEdits;

    QLineEdit *m_editInclinometerThresholdX = nullptr;
    QLineEdit *m_editInclinometerThresholdY = nullptr;

    TechVirtualKeyboard *m_virtualKeyboard = nullptr;
};

#endif // FEATURESWITCHWIDGET_H
