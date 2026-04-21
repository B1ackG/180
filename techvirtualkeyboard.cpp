#include "techvirtualkeyboard.h"
#include "animationmanager.h"
#include <QPainter>
#include <QLinearGradient>
#include <QRandomGenerator>
#include <QDebug>
#include <cmath>
#include <QKeyEvent>

TechVirtualKeyboard::TechVirtualKeyboard(QWidget *parent)
    : QWidget{parent}
    , m_targetLineEdit(nullptr)
    , m_inputPreview(nullptr)
    , m_primaryColor(QColor(0, 200, 255))      // 科技蓝
    , m_secondaryColor(QColor(138, 43, 226))   // 紫色
    , m_glowColor(QColor(0, 255, 255, 150))    // 青色辉光
    , m_glowEnabled(true)
    , m_glowIntensity(0.3)
    , m_glowDirection(true)
    , m_isEditing(false)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUI();
    setupConnections();
    applyTechStyle();

    // 注册到全局动画管理器以节省 CPU
    AnimationManager::instance()->registerWidget(this);

    // 设置固定大小，布局更美观
    setFixedSize(320, 380);
}

TechVirtualKeyboard::~TechVirtualKeyboard()
{
    AnimationManager::instance()->unregisterWidget(this);
}

void TechVirtualKeyboard::setupUI()
{
    // 创建外层垂直布局
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setSpacing(10);
    m_outerLayout->setContentsMargins(15, 15, 15, 15);

    // 创建输入预览框容器
    QWidget *previewWidget = new QWidget(this);
    previewWidget->setFixedHeight(50);
    QHBoxLayout *previewLayout = new QHBoxLayout(previewWidget);
    previewLayout->setContentsMargins(0, 0, 0, 0);

    m_inputPreview = new QLineEdit(previewWidget);
    m_inputPreview->setReadOnly(true);
    m_inputPreview->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_inputPreview->setObjectName("inputPreview");
    previewLayout->addWidget(m_inputPreview);

    // 创建主键盘布局
    QWidget *keyboardWidget = new QWidget(this);
    m_mainLayout = new QGridLayout(keyboardWidget);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // 标准化按钮大小
    QSize btnSize(65, 50);
    QSize actionBtnSize(65, 50);

    // 创建数字按钮 0-9
    for (int i = 0; i < 10; ++i) {
        m_buttons[i] = new QPushButton(QString::number(i), keyboardWidget);
        m_buttons[i]->setFixedSize(btnSize);
    }

    // 创建功能按钮
    m_buttonDecimal = new QPushButton(".", keyboardWidget);
    m_buttonDecimal->setFixedSize(btnSize);

    m_buttonNegate = new QPushButton("±", keyboardWidget);
    m_buttonNegate->setFixedSize(btnSize);

    m_buttonBackspace = new QPushButton("←", keyboardWidget);
    m_buttonBackspace->setFixedSize(actionBtnSize);

    m_buttonClear = new QPushButton("C", keyboardWidget);
    m_buttonClear->setFixedSize(actionBtnSize);

    m_buttonDelete = new QPushButton("清除", keyboardWidget);
    m_buttonDelete->setFixedSize(actionBtnSize);

    m_buttonEnter = new QPushButton("确定", keyboardWidget);
    m_buttonEnter->setFixedSize(65, 108); // 跨两行

    // 重新排列布局 (追求逻辑清晰的 4列结构)
    // 第一行: C  清除  ←  /
    m_mainLayout->addWidget(m_buttonClear, 0, 0);
    m_mainLayout->addWidget(m_buttonDelete, 0, 1);
    m_mainLayout->addWidget(m_buttonBackspace, 0, 2);
    m_mainLayout->addWidget(m_buttonNegate, 0, 3);

    // 第二行: 7 8 9 (确定)
    m_mainLayout->addWidget(m_buttons[7], 1, 0);
    m_mainLayout->addWidget(m_buttons[8], 1, 1);
    m_mainLayout->addWidget(m_buttons[9], 1, 2);
    m_mainLayout->addWidget(m_buttonEnter, 1, 3, 2, 1); // 跨两行

    // 第三行: 4 5 6
    m_mainLayout->addWidget(m_buttons[4], 2, 0);
    m_mainLayout->addWidget(m_buttons[5], 2, 1);
    m_mainLayout->addWidget(m_buttons[6], 2, 2);

    // 第四行: 1 2 3 .
    m_mainLayout->addWidget(m_buttons[1], 3, 0);
    m_mainLayout->addWidget(m_buttons[2], 3, 1);
    m_mainLayout->addWidget(m_buttons[3], 3, 2);
    m_mainLayout->addWidget(m_buttonDecimal, 3, 3);

    // 第五行: 0 (跨预览逻辑，居中对齐更好看)
    m_mainLayout->addWidget(m_buttons[0], 4, 0, 1, 3); // 0 占三列宽度

    // 将预览框和键盘添加到外层布局
    m_outerLayout->addWidget(previewWidget);
    m_outerLayout->addWidget(keyboardWidget);
}

void TechVirtualKeyboard::setupConnections()
{
    // 连接数字和小数点按钮
    for (int i = 0; i < 10; ++i) {
        connect(m_buttons[i], &QPushButton::clicked, this, &TechVirtualKeyboard::onButtonClicked);
    }
    connect(m_buttonDecimal, &QPushButton::clicked, this, &TechVirtualKeyboard::onButtonClicked);

    // 连接功能按钮
    connect(m_buttonEnter, &QPushButton::clicked, this, &TechVirtualKeyboard::onEnterClicked);
    connect(m_buttonDelete, &QPushButton::clicked, this, &TechVirtualKeyboard::onDeleteClicked);
    connect(m_buttonClear, &QPushButton::clicked, this, &TechVirtualKeyboard::onClearClicked);
    connect(m_buttonBackspace, &QPushButton::clicked, this, &TechVirtualKeyboard::onBackspaceClicked);
    connect(m_buttonNegate, &QPushButton::clicked, this, &TechVirtualKeyboard::onNegateClicked);
}

void TechVirtualKeyboard::applyTechStyle()
{
    // 修改为更柔和的深色调，符合现代上位机UI
    m_primaryColor = QColor(41, 128, 185);    // 优雅蓝
    m_secondaryColor = QColor(52, 152, 219);  // 明亮蓝
    m_glowColor = QColor(0, 255, 255, 80);    // 降低辉光强度

    // 设置窗口样式
    QString windowStyle = QString(
                              "QWidget {"
                              "    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                              "        stop:0 rgba(30, 40, 60, 250), stop:1 rgba(20, 25, 40, 255));"
                              "    border: 2px solid %1;"
                              "    border-radius: 12px;"
                              "}"
                              ).arg(m_primaryColor.name());

    setStyleSheet(windowStyle);

    // 输入预览框样式
    QString previewStyle = QString(
                               "#inputPreview {"
                               "    background-color: rgba(10, 15, 25, 200);"
                               "    border: 1px solid %1;"
                               "    border-radius: 6px;"
                               "    color: #00FFFF;"
                               "    font-size: 20px;"
                               "    font-weight: bold;"
                               "    font-family: 'Consolas', 'Monaco', monospace;"
                               "    padding-right: 15px;"
                               "}"
                               ).arg(m_primaryColor.name());

    m_inputPreview->setStyleSheet(previewStyle);

    // 设置通用的按钮样式
    QString baseButtonStyle = 
        "QPushButton {"
        "    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "        stop:0 rgba(60, 70, 90, 255), stop:1 rgba(40, 50, 65, 255));"
        "    color: white;"
        "    border: 1px solid rgba(255, 255, 255, 40);"
        "    border-radius: 6px;"
        "    font-size: 18px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(70, 80, 100, 255);"
        "    border: 1px solid rgba(255, 255, 255, 100);"
        "}"
        "QPushButton:pressed {"
        "    background-color: %1;"
        "    border: 1px solid white;"
        "    padding-left: 2px;"
        "    padding-top: 2px;"
        "}";

    QString numberButtonStyle = baseButtonStyle.arg(m_primaryColor.name());

    // 应用数字和符号按钮样式
    for (int i = 0; i < 10; ++i) {
        m_buttons[i]->setStyleSheet(numberButtonStyle);
    }
    m_buttonDecimal->setStyleSheet(numberButtonStyle);
    m_buttonNegate->setStyleSheet(numberButtonStyle);

    // 功能按钮样式（更显眼的颜色）
    QString actionButtonStyle = 
        "QPushButton {"
        "    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "        stop:0 rgba(80, 90, 110, 255), stop:1 rgba(60, 70, 85, 255));"
        "    color: white;"
        "    border: 1px solid rgba(255, 255, 255, 30);"
        "    border-radius: 6px;"
        "    font-size: 16px;"
        "}"
        "QPushButton:hover { background-color: rgba(90, 100, 120, 255); }"
        "QPushButton:pressed { background-color: %1; }";
    
    m_buttonDelete->setStyleSheet(actionButtonStyle.arg(m_primaryColor.name()));
    m_buttonClear->setStyleSheet(actionButtonStyle.arg(m_primaryColor.name()));
    m_buttonBackspace->setStyleSheet(actionButtonStyle.arg(m_primaryColor.name()));

    // 确定按钮样式 (醒目的强调色)
    QString enterButtonStyle = 
        "QPushButton {"
        "    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "        stop:0 #27AE60, stop:1 #2ECC71);"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    font-size: 18px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #2ECC71; }"
        "QPushButton:pressed { background-color: #27AE60; }";

    m_buttonEnter->setStyleSheet(enterButtonStyle);

    // 添加阴影效果
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(20);
    m_shadowEffect->setColor(QColor(0, 0, 0, 150));
    m_shadowEffect->setOffset(0, 4);
    setGraphicsEffect(m_shadowEffect);
}

void TechVirtualKeyboard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    QRectF rect = this->rect().adjusted(1, 1, -1, -1);

    // 绘制科技感背景渐变
    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0, QColor(20, 25, 45, 245));
    gradient.setColorAt(0.5, QColor(30, 35, 60, 235));
    gradient.setColorAt(1, QColor(20, 25, 45, 245));

    painter.setBrush(gradient);
    painter.setPen(QPen(m_primaryColor, 2));
    painter.drawRoundedRect(rect, 10, 10);  // 更小的圆角

    QWidget::paintEvent(event);
}

void TechVirtualKeyboard::updateAnimation()
{
    // 如果窗口不显示或已隐藏，跳过重绘
    if (!isVisible() || isHidden()) {
        return;
    }

    if (m_glowEnabled) {
        if (m_glowDirection) {
            m_glowIntensity += 0.05;
            if (m_glowIntensity >= 1.0) {
                m_glowIntensity = 1.0;
                m_glowDirection = false;
            }
        } else {
            m_glowIntensity -= 0.05;
            if (m_glowIntensity <= 0.4) {
                m_glowIntensity = 0.4;
                m_glowDirection = true;
            }
        }

        // 仅在显式需要（如边框呼吸感）时更新局部
        // 目前样式已改为QSS，paintEvent中只画背景
        update(); 
    }
}

void TechVirtualKeyboard::setTargetLineEdit(QLineEdit *lineEdit)
{
    m_targetLineEdit = lineEdit;
    m_isEditing = true;

    // 如果目标LineEdit有内容，初始化为预览框内容
    if (lineEdit && !lineEdit->text().isEmpty()) {
        m_currentText = lineEdit->text();
        updatePreview();
    } else {
        m_currentText.clear();
        updatePreview();
    }
}

void TechVirtualKeyboard::setInitialText(const QString &text)
{
    m_currentText = text;
    updatePreview();
}

void TechVirtualKeyboard::showAtWidget(QWidget *targetWidget)
{
    if (!targetWidget) return;

    // 同一目标重复点击时不重复 show/activate，避免 Wayland 下窗口序列异常。
    if (isVisible() && m_targetLineEdit && targetWidget == m_targetLineEdit) {
        m_currentText = m_targetLineEdit->text();
        updatePreview();
        return;
    }

    // 计算显示位置（目标控件下方）
    QPoint globalPos = targetWidget->mapToGlobal(QPoint(0, targetWidget->height() + 5));

    // 确保键盘不会超出屏幕
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    QRect screenRect = screen->availableGeometry();

    // 边界检查
    if (globalPos.x() + width() > screenRect.right()) {
        globalPos.setX(screenRect.right() - width());
    }
    if (globalPos.y() + height() > screenRect.bottom()) {
        globalPos.setY(targetWidget->mapToGlobal(QPoint(0, 0)).y() - height() - 5);
    }
    if (globalPos.x() < screenRect.left()) {
        globalPos.setX(screenRect.left());
    }

    move(globalPos);
    show();
    raise();
    const QString platform = QGuiApplication::platformName().toLower();
    if (!platform.contains("wayland")) {
        activateWindow();
    }

    // 清空预览框内容
    if (m_targetLineEdit) {
        m_currentText = m_targetLineEdit->text();
    } else {
        m_currentText.clear();
    }
    updatePreview();
}

bool TechVirtualKeyboard::eventFilter(QObject *watched, QEvent *event)
{
    // 处理鼠标按下事件
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *watchedWidget = qobject_cast<QWidget*>(watched);
        if (watchedWidget && m_targetLineEdit && (watchedWidget == m_targetLineEdit || m_targetLineEdit->isAncestorOf(watchedWidget))) {
            return QWidget::eventFilter(watched, event);
        }

        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint globalPos = mouseEvent->globalPos();

        // 检查点击是否在键盘内部
        if (!geometry().contains(globalPos) && isVisible()) {
            // 点击键盘外部，取消输入
            hide();
            emit inputCancelled();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void TechVirtualKeyboard::showEvent(QShowEvent *event)
{
    // 显示时确保注册
    AnimationManager::instance()->registerWidget(this);
    QWidget::showEvent(event);
}

void TechVirtualKeyboard::hideEvent(QHideEvent *event)
{
    // 隐藏时从渲染队列移除，彻底停止重绘，这是降CPU的关键
    AnimationManager::instance()->unregisterWidget(this);
    
    // 保证显示时再次触发刷新
    update();

    // 失去编辑状态
    m_isEditing = false;

    QWidget::hideEvent(event);
}

void TechVirtualKeyboard::updatePreview()
{
    if (m_inputPreview) {
        m_inputPreview->setText(m_currentText);
        // 将光标移动到末尾
        m_inputPreview->setCursorPosition(m_currentText.length());
    }
}

void TechVirtualKeyboard::applyButtonPressEffect(QPushButton *button)
{
    if (!button) return;

    // 使用样式表变化来创建按下效果，而不是移动按钮
    QString originalStyle = button->styleSheet();

    // 临时改变按钮样式，使其看起来被按下
    QString pressedStyle = originalStyle;
    pressedStyle.replace("border: 2px solid", "border: 1px solid");  // 边框变细
    pressedStyle.replace("font-size: 16px", "font-size: 15px");     // 字体稍小
    pressedStyle.replace("font-size: 14px", "font-size: 13px");     // 功能按钮字体

    button->setStyleSheet(pressedStyle);

    // 使用定时器恢复原始样式
    QTimer::singleShot(100, this, [button, originalStyle]() {
        if (button) {
            button->setStyleSheet(originalStyle);
        }
    });
}

// 槽函数实现
void TechVirtualKeyboard::onButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QString text = button->text();

    // 添加文本到当前输入
    m_currentText += text;
    updatePreview();

    // 应用按钮按下效果（不移动按钮）
    applyButtonPressEffect(button);
}

void TechVirtualKeyboard::onEnterClicked()
{
    // 确认输入，将内容传递给目标LineEdit
    if (m_targetLineEdit && m_isEditing) {
        m_targetLineEdit->setText(m_currentText);

        // 发送回车键事件给目标LineEdit
        QKeyEvent *enterEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::postEvent(m_targetLineEdit, enterEvent);

        // 清除焦点，防止继续输入
        m_targetLineEdit->clearFocus();

        // 发射确认信号
        emit inputConfirmed(m_currentText);
    }

    // 隐藏键盘
    hide();

    // 应用按钮按下效果
    applyButtonPressEffect(m_buttonEnter);
}

void TechVirtualKeyboard::onDeleteClicked()
{
    // 删除当前所有内容
    m_currentText.clear();
    updatePreview();

    // 应用按钮按下效果
    applyButtonPressEffect(m_buttonDelete);
}

void TechVirtualKeyboard::onClearClicked()
{
    // 清除所有内容
    m_currentText.clear();
    updatePreview();

    // 应用按钮按下效果
    applyButtonPressEffect(m_buttonClear);
}

void TechVirtualKeyboard::onBackspaceClicked()
{
    // 删除最后一个字符
    if (!m_currentText.isEmpty()) {
        m_currentText.chop(1);
        updatePreview();
    }

    // 应用按钮按下效果
    applyButtonPressEffect(m_buttonBackspace);
}

void TechVirtualKeyboard::onNegateClicked()
{
    // 切换正负号
    if (!m_currentText.isEmpty()) {
        if (m_currentText[0] == '-') {
            // 如果有负号，移除它
            m_currentText = m_currentText.mid(1);
        } else {
            // 如果没有负号，在开头添加
            m_currentText.prepend('-');
        }
        updatePreview();
    }

    // 应用按钮按下效果
    applyButtonPressEffect(m_buttonNegate);
}
