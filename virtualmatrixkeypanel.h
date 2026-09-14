#ifndef VIRTUALMATRIXKEYPANEL_H
#define VIRTUALMATRIXKEYPANEL_H

#ifdef ENABLE_VIRTUAL_MATRIX_KEYS

#include <QMap>
#include <QPointer>
#include <QSet>
#include <QWidget>

class QEvent;
class QPushButton;

/**
 * @brief 本机 Debug 用的虚拟外部按键条：独立置顶窗口，贴在主窗口右侧。
 *
 * 不作为主窗口子控件，避免被 ApplicationModal 弹窗挡住或吃掉鼠标。
 * ○1～○14 与使能键均为点按切换：点一下进入按下，再点一下进入松开。
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

    /** ApplicationModal 会在 QApplication::notify 里丢掉其它窗口的鼠标；Debug 下对虚拟按键放行。 */
    static bool shouldReceiveDuringModal(QObject *receiver, QEvent *event);

signals:
    void matrixKeyChanged(int keyNumber, bool pressed);
    void enableButtonChanged(bool enabled);

protected:
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void applyCollapsedState();
    void releaseAllHeldInputs();
    void onKeyToggled(int keyNumber, bool pressed);
    void onEnableToggled(bool enabled);
    void onToggleClicked();
    bool isPanelWidget(QObject *obj) const;
    static bool isPointerEvent(const QEvent *event);

    static constexpr int kCollapsedWidth = 28;
    static constexpr int kExpandedWidth = 120;
    static constexpr int kTopOffset = 96;
    static constexpr int kBottomMargin = 48;
    static constexpr int kOutsideGap = 2;

    QPointer<QWidget> m_host;
    bool m_collapsed = false;
    bool m_enableHeld = false;
    QSet<int> m_heldKeys;
    QWidget *m_expandedBody = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QPushButton *m_enableButton = nullptr;
    QMap<int, QPushButton *> m_keyButtons;
};

#endif // ENABLE_VIRTUAL_MATRIX_KEYS

#endif // VIRTUALMATRIXKEYPANEL_H
