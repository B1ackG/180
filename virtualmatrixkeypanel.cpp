#ifdef ENABLE_VIRTUAL_MATRIX_KEYS

#include "virtualmatrixkeypanel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QPushButton>
#include <QVBoxLayout>

VirtualMatrixKeyPanel::VirtualMatrixKeyPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("virtualMatrixKeyPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet(QStringLiteral(
        "#virtualMatrixKeyPanel {"
        "  background: rgba(8, 18, 32, 210);"
        "  border-left: 1px solid rgba(0, 200, 255, 0.45);"
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
    m_enableButton->setCursor(Qt::PointingHandCursor);
    connect(m_enableButton, &QPushButton::toggled, this, &VirtualMatrixKeyPanel::onEnableToggled);
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
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(button, &QPushButton::toggled, this, [this, keyNumber](bool pressed) {
            onKeyToggled(keyNumber, pressed);
        });
        m_keyButtons.insert(keyNumber, button);
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
    QWidget *host = parentWidget();
    if (!host) {
        return;
    }

    const int width = m_collapsed ? kCollapsedWidth : kExpandedWidth;
    const int height = qMax(200, host->height() - kTopOffset - kBottomMargin);
    setGeometry(host->width() - width, kTopOffset, width, height);
    raise();
}

void VirtualMatrixKeyPanel::hideEvent(QHideEvent *event)
{
    releaseAllHeldInputs();
    QWidget::hideEvent(event);
}

void VirtualMatrixKeyPanel::releaseAllHeldInputs()
{
    if (m_enableButton && m_enableButton->isChecked()) {
        m_enableButton->setChecked(false);
    }

    for (auto it = m_keyButtons.cbegin(); it != m_keyButtons.cend(); ++it) {
        QPushButton *button = it.value();
        if (button && button->isChecked()) {
            button->setChecked(false);
        }
    }
}

void VirtualMatrixKeyPanel::onKeyToggled(int keyNumber, bool pressed)
{
    if (pressed) {
        if (m_heldKeys.contains(keyNumber)) {
            return;
        }
        m_heldKeys.insert(keyNumber);
    } else if (!m_heldKeys.remove(keyNumber)) {
        return;
    }
    emit matrixKeyChanged(keyNumber, pressed);
}

void VirtualMatrixKeyPanel::onEnableToggled(bool enabled)
{
    if (m_enableHeld == enabled) {
        return;
    }
    m_enableHeld = enabled;
    emit enableButtonChanged(enabled);
}

void VirtualMatrixKeyPanel::onToggleClicked()
{
    setCollapsed(!m_collapsed);
}

#endif // ENABLE_VIRTUAL_MATRIX_KEYS
