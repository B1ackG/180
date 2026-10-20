#ifndef VIRTUALMATRIXKEYPANEL_H
#define VIRTUALMATRIXKEYPANEL_H

#ifdef ENABLE_VIRTUAL_MATRIX_KEYS

#include <QMap>
#include <QPointer>
#include <QWidget>

class QPushButton;

/**
 * @brief 本机 Debug 用的虚拟外部按键条：独立置顶窗口，贴在主窗口右侧。
 *
 * 不挂到主窗口下面，避免跟主界面一起被模态窗盖住。
 * 窗口带 Qt::WA_GroupLeader：ApplicationModal 只挡住普通顶层窗口，
 * 这个属性让 QWidgetWindow 的 qt_try_modal 继续把鼠标交给按键。
 * ○1～○14 与使能键：左键按住为短按，松开即松开；右键锁定按下，再点一次松开。
 * 左键按住时再右键会转为锁定，松开左键后仍保持按下。已锁定时松开左键不会松开。
 * 示教器 Release 不编译此控件。
 */
class VirtualMatrixKeyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit VirtualMatrixKeyPanel(QWidget *hostWindow);
    ~VirtualMatrixKeyPanel() override;

    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

    /** 贴到宿主窗口右缘；宿主移动/缩放时跟随。 */
    void relayoutToHost();

signals:
    void matrixKeyChanged(int keyNumber, bool pressed);
    void enableButtonChanged(bool enabled);

protected:
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void applyCollapsedState();
    struct InputHold {
        bool latched = false;
        bool momentary = false;
        bool isPressed() const { return latched || momentary; }
    };

    void releaseAllHeldInputs();
    void onToggleClicked();
    int keyNumberForButton(const QPushButton *button) const;
    /** 处理左键短按与右键锁定。返回 true 表示事件已吃掉。 */
    bool handleInputMouse(QPushButton *button, InputHold &hold, QEvent *event);
    void syncChecked(QPushButton *button, bool pressed);
    void clearInput(QPushButton *button, InputHold &hold);

    static constexpr int kCollapsedWidth = 28;
    static constexpr int kExpandedWidth = 120;
    static constexpr int kTopOffset = 96;
    static constexpr int kBottomMargin = 48;
    static constexpr int kOutsideGap = 2;

    QPointer<QWidget> m_host;
    bool m_collapsed = false;
    InputHold m_enableHold;
    QMap<int, InputHold> m_keyHolds;
    QWidget *m_expandedBody = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QPushButton *m_enableButton = nullptr;
    QMap<int, QPushButton *> m_keyButtons;
};

#endif // ENABLE_VIRTUAL_MATRIX_KEYS

#endif // VIRTUALMATRIXKEYPANEL_H
