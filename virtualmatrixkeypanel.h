#ifndef VIRTUALMATRIXKEYPANEL_H
#define VIRTUALMATRIXKEYPANEL_H

#ifdef ENABLE_VIRTUAL_MATRIX_KEYS

#include <QMap>
#include <QSet>
#include <QWidget>

class QPushButton;

/**
 * @brief 本机 Debug 用的虚拟外部按键条：叠在主窗口右侧，默认折叠。
 *
 * ○1～○14 与使能键均为点按切换：点一下进入按下，再点一下进入松开。
 * 示教器 Release 不编译此控件。
 */
class VirtualMatrixKeyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit VirtualMatrixKeyPanel(QWidget *parent = nullptr);

    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

    /** 贴到父控件右缘；窗口尺寸变化时由 MainWindow 调用。 */
    void relayoutToHost();

signals:
    void matrixKeyChanged(int keyNumber, bool pressed);
    void enableButtonChanged(bool enabled);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    void buildUi();
    void applyCollapsedState();
    void releaseAllHeldInputs();
    void onKeyToggled(int keyNumber, bool pressed);
    void onEnableToggled(bool enabled);
    void onToggleClicked();

    static constexpr int kCollapsedWidth = 22;
    static constexpr int kExpandedWidth = 112;
    static constexpr int kTopOffset = 112;
    static constexpr int kBottomMargin = 56;

    bool m_collapsed = true;
    bool m_enableHeld = false;
    QSet<int> m_heldKeys;
    QWidget *m_expandedBody = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QPushButton *m_enableButton = nullptr;
    QMap<int, QPushButton *> m_keyButtons;
};

#endif // ENABLE_VIRTUAL_MATRIX_KEYS

#endif // VIRTUALMATRIXKEYPANEL_H
