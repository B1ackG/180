#ifdef ENABLE_VIRTUAL_MATRIX_KEYS

#include "virtualmatrixkeypanel.h"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QList>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

VirtualMatrixKeyPanel::VirtualMatrixKeyPanel(QWidget *hostWindow)
    : QWidget(nullptr)
    , m_host(hostWindow)
{
    setObjectName(QStringLiteral("virtualMatrixKeyPanel"));
    setWindowTitle(QStringLiteral("虚拟外部按键"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_GroupLeader, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_QuitOnClose, false);
    setFocusPolicy(Qt::NoFocus);

    setStyleSheet(QStringLiteral(
        "#virtualMatrixKeyPanel {"
        "  background: rgba(8, 18, 32, 235);"
        "  border: 1px solid rgba(0, 200, 255, 0.55);"
        "}"
        "#virtualMatrixKeyPanel QPushButton {"
        "  color: #dff6ff;"
        "  background: rgba(12, 36, 58, 0.92);"
        "  border: 1px solid rgba(0, 198, 255, 0.50);"
        "  border-radius: 4px;"
        "  font-weight: 700;"
        "  font-size: 11px;"
        "  padding: 0px;"
        "}"
        "#virtualMatrixKeyPanel QPushButton:pressed,"
        "#virtualMatrixKeyPanel QPushButton:checked {"
        "  background: rgba(0, 130, 200, 0.95);"
        "  border: 1px solid #9fe7ff;"
        "}"
        "#virtualMatrixKeyPanel QPushButton#virtualEnableButton {"
        "  color: #fff3d1;"
        "  border: 1px solid rgba(255, 184, 77, 0.70);"
        "  min-height: 28px;"
        "}"
        "#virtualMatrixKeyPanel QPushButton#virtualEnableButton:pressed,"
        "#virtualMatrixKeyPanel QPushButton#virtualEnableButton:checked {"
        "  background: rgba(180, 110, 20, 0.95);"
        "  border: 1px solid #ffd27a;"
        "}"
        "#virtualMatrixKeyPanel QPushButton#virtualKeyToggle {"
        "  border: none;"
        "  border-radius: 0px;"
        "  background: rgba(10, 28, 46, 0.96);"
        "  font-size: 12px;"
        "}"));

    buildUi();
    applyCollapsedState();

    if (m_host) {
        m_host->installEventFilter(this);
        connect(m_host, &QObject::destroyed, this, &QObject::deleteLater);
    }
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

VirtualMatrixKeyPanel::~VirtualMatrixKeyPanel()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    if (m_host) {
        m_host->removeEventFilter(this);
    }
}

void VirtualMatrixKeyPanel::buildUi()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_toggleButton = new QPushButton(this);
    m_toggleButton->setObjectName(QStringLiteral("virtualKeyToggle"));
    m_toggleButton->setFixedWidth(kCollapsedWidth);
    m_toggleButton->setFocusPolicy(Qt::NoFocus);
    m_toggleButton->setAutoRepeat(false);
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    connect(m_toggleButton, &QPushButton::clicked, this, &VirtualMatrixKeyPanel::onToggleClicked);
    root->addWidget(m_toggleButton);

    m_expandedBody = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(m_expandedBody);
    bodyLayout->setContentsMargins(4, 6, 4, 6);
    bodyLayout->setSpacing(4);

    m_enableButton = new QPushButton(QStringLiteral("使能"), m_expandedBody);
    m_enableButton->setObjectName(QStringLiteral("virtualEnableButton"));
    m_enableButton->setFocusPolicy(Qt::NoFocus);
    m_enableButton->setCheckable(true);
    m_enableButton->setAutoRepeat(false);
    m_enableButton->setContextMenuPolicy(Qt::NoContextMenu);
    m_enableButton->setCursor(Qt::PointingHandCursor);
    m_enableButton->setToolTip(QStringLiteral("左键按住为短按，松开即松开；右键锁定按下，再点一次松开"));
    bodyLayout->addWidget(m_enableButton);

    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);

    for (int keyNumber = 1; keyNumber <= 14; ++keyNumber) {
        auto *button = new QPushButton(QStringLiteral("○%1").arg(keyNumber), m_expandedBody);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCheckable(true);
        button->setAutoRepeat(false);
        button->setContextMenuPolicy(Qt::NoContextMenu);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setToolTip(QStringLiteral("左键按住为短按，松开即松开；右键锁定按下，再点一次松开"));
        m_keyButtons.insert(keyNumber, button);
        m_keyHolds.insert(keyNumber, InputHold());
        const int row = (keyNumber - 1) / 2;
        const int col = (keyNumber - 1) % 2;
        grid->addWidget(button, row, col);
    }

    bodyLayout->addLayout(grid, 1);
    root->addWidget(m_expandedBody, 1);
}

void VirtualMatrixKeyPanel::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) {
        return;
    }
    if (collapsed) {
        releaseAllHeldInputs();
    }
    m_collapsed = collapsed;
    applyCollapsedState();
    relayoutToHost();
}

void VirtualMatrixKeyPanel::applyCollapsedState()
{
    if (m_expandedBody) {
        m_expandedBody->setVisible(!m_collapsed);
    }
    if (m_toggleButton) {
        m_toggleButton->setText(m_collapsed ? QStringLiteral("键") : QStringLiteral("«"));
        m_toggleButton->setToolTip(m_collapsed
                                       ? QStringLiteral("展开虚拟外部按键")
                                       : QStringLiteral("折叠虚拟外部按键"));
    }
}

void VirtualMatrixKeyPanel::relayoutToHost()
{
    if (!m_host) {
        return;
    }

    const int width = m_collapsed ? kCollapsedWidth : kExpandedWidth;
    const int height = qMax(240, m_host->height() - kTopOffset - kBottomMargin);
    const QRect hostGeo = m_host->frameGeometry();

    QRect screen = hostGeo;
    if (QScreen *s = QGuiApplication::screenAt(hostGeo.center())) {
        screen = s->availableGeometry();
    } else if (QScreen *s = QGuiApplication::primaryScreen()) {
        screen = s->availableGeometry();
    }

    // 优先弹到主窗口右侧外侧；全屏/空间不够时贴回窗口内右缘。
    int x = hostGeo.right() + kOutsideGap;
    if (x + width > screen.right()) {
        x = hostGeo.right() - width;
    }
    if (x < screen.left()) {
        x = screen.left();
    }

    int y = hostGeo.top() + kTopOffset;
    if (y + height > screen.bottom()) {
        y = qMax(screen.top(), screen.bottom() - height);
    }

    setGeometry(x, y, width, height);
    raise();
}

void VirtualMatrixKeyPanel::hideEvent(QHideEvent *event)
{
    releaseAllHeldInputs();
    QWidget::hideEvent(event);
}

bool VirtualMatrixKeyPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (!event) {
        return QWidget::eventFilter(watched, event);
    }

    if (m_host && watched == m_host) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WindowStateChange:
            relayoutToHost();
            break;
        case QEvent::Hide:
            hide();
            break;
        case QEvent::Show:
            show();
            relayoutToHost();
            raise();
            break;
        case QEvent::Close:
            close();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (auto *button = qobject_cast<QPushButton *>(watched)) {
        if (button == m_enableButton) {
            const bool before = m_enableHold.isPressed();
            if (handleInputMouse(button, m_enableHold, event)) {
                if (m_enableHold.isPressed() != before) {
                    emit enableButtonChanged(m_enableHold.isPressed());
                }
                return true;
            }
        } else {
            const int keyNumber = keyNumberForButton(button);
            if (keyNumber > 0) {
                InputHold &hold = m_keyHolds[keyNumber];
                const bool before = hold.isPressed();
                if (handleInputMouse(button, hold, event)) {
                    if (hold.isPressed() != before) {
                        emit matrixKeyChanged(keyNumber, hold.isPressed());
                    }
                    return true;
                }
            }
        }
    }

    if (event->type() == QEvent::Show) {
        auto *shown = qobject_cast<QWidget *>(watched);
        if (shown && shown->isWindow() && shown != this
            && shown->windowModality() != Qt::NonModal && isVisible()) {
            QTimer::singleShot(0, this, [this]() {
                if (isVisible()) {
                    raise();
                }
            });
        }
    }

    return QWidget::eventFilter(watched, event);
}

void VirtualMatrixKeyPanel::releaseAllHeldInputs()
{
    const bool enableWasPressed = m_enableHold.isPressed();
    clearInput(m_enableButton, m_enableHold);

    QList<int> releasedKeys;
    for (auto it = m_keyHolds.begin(); it != m_keyHolds.end(); ++it) {
        if (it.value().isPressed()) {
            releasedKeys.append(it.key());
        }
        clearInput(m_keyButtons.value(it.key()), it.value());
    }

    if (enableWasPressed) {
        emit enableButtonChanged(false);
    }
    for (int keyNumber : releasedKeys) {
        emit matrixKeyChanged(keyNumber, false);
    }
}

int VirtualMatrixKeyPanel::keyNumberForButton(const QPushButton *button) const
{
    for (auto it = m_keyButtons.cbegin(); it != m_keyButtons.cend(); ++it) {
        if (it.value() == button) {
            return it.key();
        }
    }
    return 0;
}

bool VirtualMatrixKeyPanel::handleInputMouse(QPushButton *button, InputHold &hold, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type == QEvent::ContextMenu) {
        return true;
    }
    if (type != QEvent::MouseButtonPress
        && type != QEvent::MouseButtonRelease
        && type != QEvent::MouseButtonDblClick) {
        return false;
    }

    const auto *mouse = static_cast<const QMouseEvent *>(event);
    const Qt::MouseButton which = mouse->button();

    if (type == QEvent::MouseButtonRelease) {
        if (which == Qt::LeftButton) {
            hold.momentary = false;
            if (button && QWidget::mouseGrabber() == button) {
                button->releaseMouse();
            }
        }
        syncChecked(button, hold.isPressed());
        return true;
    }

    if (which == Qt::LeftButton) {
        hold.momentary = true;
        if (button) {
            button->grabMouse();
        }
    } else if (which == Qt::RightButton) {
        hold.latched = !hold.latched;
    }

    syncChecked(button, hold.isPressed());
    return true;
}

void VirtualMatrixKeyPanel::syncChecked(QPushButton *button, bool pressed)
{
    if (!button || button->isChecked() == pressed) {
        return;
    }
    const QSignalBlocker blocker(button);
    button->setChecked(pressed);
}

void VirtualMatrixKeyPanel::clearInput(QPushButton *button, InputHold &hold)
{
    hold.latched = false;
    hold.momentary = false;
    syncChecked(button, false);
    if (button && QWidget::mouseGrabber() == button) {
        button->releaseMouse();
    }
}

void VirtualMatrixKeyPanel::onToggleClicked()
{
    setCollapsed(!m_collapsed);
}

#endif // ENABLE_VIRTUAL_MATRIX_KEYS
