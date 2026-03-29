#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featureswitchmanager.h"
#include "featureswitchwidget.h"
#include "maindevicemodbusapi.h"
#include "mainmodbusconnector.h"
#include "mainmodbuslabelmapper.h"
#include "mainmodbuspoller.h"
#include "mainmodbusstatus.h"
#include <QMovie>
#include <QDateTime>
#include <QDebug>
#include <QLoggingCategory>
#include <QQuickWidget>
#include <QQmlContext>
#include <QQuickItem>
Q_LOGGING_CATEGORY(lcMainWindow, "app.mainwindow")
#include <QPainter>
#ifdef qDebug
#undef qDebug
#endif
#define qDebug() qCDebug(lcMainWindow)
#include <QComboBox>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QToolTip>
#include <QGuiApplication>
#include <QtMath>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <QSocketNotifier>

bool MainWindow::isBigFeatureEnabled(const QString &key) const
{
    if (!m_featureSwitchManager) {
        return true;
    }
    return m_featureSwitchManager->isBigFeatureEnabled(key);
}

bool MainWindow::isSmallFeatureEnabled(const QString &key) const
{
    if (!m_featureSwitchManager) {
        return true;
    }
    return m_featureSwitchManager->isSmallFeatureEnabled(key);
}

bool MainWindow::isFeatureEnabled(const QString &bigKey, const QString &smallKey) const
{
    if (!m_featureSwitchManager) {
        return true;
    }
    return m_featureSwitchManager->isFeatureEnabled(bigKey, smallKey);
}

/**
 * =============================================================================
 * MainWindow 实现文件
 * =============================================================================
 * 组织结构（对应 mainwindow.h）:
 * 1. 生命周期与核心初始化 (Life Cycle)
 * 2. UI 框架、背景与绘制 (UI Framework)
 * 3. 报警系统 (Alarm System)
 * 4. Modbus & 设备通信 (Modbus)
 * 5. 传感器与周边硬件 (Sensors)
 * 6. 机器人运动控制与配置 (Motion Control)
 * 7. 历史记录与日志 (History & Logging)
 * 8. 信号处理槽函数 (Slot Handlers)
 * 10. 内部辅助函数 (Helper Methods)
 * =============================================================================
 */

// ==========================================

void MainWindow::loadPollingRuntimeSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");

    m_mainModbusPollIntervalMs = settings.value("main_modbus_poll_ms", 500).toInt();
    m_mainUiPollIntervalMs = settings.value("main_ui_poll_ms", 200).toInt();
    m_mainReconnectIntervalMs = settings.value("main_reconnect_ms", 5000).toInt();
    m_agvPollIntervalMs = settings.value("agv_poll_ms", 200).toInt();
    m_agvReconnectIntervalMs = settings.value("agv_reconnect_ms", 5000).toInt();

    settings.endGroup();

    m_mainModbusPollIntervalMs = qBound(50, m_mainModbusPollIntervalMs, 60000);
    m_mainUiPollIntervalMs = qBound(50, m_mainUiPollIntervalMs, 60000);
    m_mainReconnectIntervalMs = qBound(500, m_mainReconnectIntervalMs, 120000);
    m_agvPollIntervalMs = qBound(50, m_agvPollIntervalMs, 60000);
    m_agvReconnectIntervalMs = qBound(500, m_agvReconnectIntervalMs, 120000);
}

void MainWindow::savePollingRuntimeSettings() const
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("Polling");
    settings.setValue("main_modbus_poll_ms", m_mainModbusPollIntervalMs);
    settings.setValue("main_ui_poll_ms", m_mainUiPollIntervalMs);
    settings.setValue("main_reconnect_ms", m_mainReconnectIntervalMs);
    settings.setValue("agv_poll_ms", m_agvPollIntervalMs);
    settings.setValue("agv_reconnect_ms", m_agvReconnectIntervalMs);
    settings.endGroup();
    settings.sync();
}

void MainWindow::applyPollingRuntimeSettings()
{
    if (m_modbusManager) {
        m_modbusManager->setPollInterval(m_mainModbusPollIntervalMs);
        m_modbusManager->setAutoReconnect(true, m_mainReconnectIntervalMs);
    }

    if (m_modbusPollTimer && m_modbusPollTimer->isActive()) {
        m_modbusPollTimer->setInterval(m_mainUiPollIntervalMs);
    }

    if (m_agvModbusManager) {
        m_agvModbusManager->setPollInterval(m_agvPollIntervalMs);
        m_agvModbusManager->setAutoReconnect(true, m_agvReconnectIntervalMs);
    }
}

void MainWindow::loadSliderLabelRuntimeSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");
    
    for (auto it = m_sliderLabelConfigs.begin(); it != m_sliderLabelConfigs.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());
        
        it.value().minValue = settings.value(keyMin, it.value().minValue).toDouble();
        it.value().maxValue = settings.value(keyMax, it.value().maxValue).toDouble();
    }
    
    settings.endGroup();
}

void MainWindow::saveSliderLabelRuntimeSettings() const
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("SliderLabelLimits");
    
    for (auto it = m_sliderLabelConfigs.begin(); it != m_sliderLabelConfigs.end(); ++it) {
        QString keyMin = QString("%1_min").arg(it.key());
        QString keyMax = QString("%1_max").arg(it.key());
        
        settings.setValue(keyMin, it.value().minValue);
        settings.setValue(keyMax, it.value().maxValue);
    }
    
    settings.endGroup();
    settings.sync();
}

void MainWindow::applySliderLabelRuntimeSettings()
{
    // 应用到所有缓存的实例
    for (auto it = m_sliderLabelConfigs.begin(); it != m_sliderLabelConfigs.end(); ++it) {
        const QString& name = it.key();
        const SliderLabelConfig& config = it.value();
        
        // 首页实例
        if (m_sliderLabelInstances.contains(name)) {
            m_sliderLabelInstances[name]->setRange(config.minValue, config.maxValue);
        }
        
        // 副本页实例（在 m_pageSliders 中）
        for (auto pageIt = m_pageSliders.begin(); pageIt != m_pageSliders.end(); ++pageIt) {
            for (TechSliderLabel* slider : pageIt.value()) {
                if (slider->objectName().contains(name.split("_").last())) {
                     slider->setRange(config.minValue, config.maxValue);
                }
            }
        }
    }
}

// 1. 生命周期与核心初始化 (Life Cycle)
// ==========================================

// ==========================================
// 2. UI 框架、背景与绘制 (UI Framework)
// ==========================================

/**
 * @brief 设置主要信号与槽的连接
 *
 * 将 UI 按钮、记录器、网络及其他模块的信号与本窗口的槽绑定，
 * 保证用户交互和模块事件能驱动界面更新与相关逻辑处理。
 */
// 信号槽连接
void MainWindow::setupConnections()
{
    // setupNavigationConnections();
    setupRecordAndPermissionConnections();
    setupControlConnections();
    setupSubsystemConnections();
}

void MainWindow::setupNavigationConnections()
{
    if (!isBigFeatureEnabled("ui_navigation")) {
        qDebug() << "UI导航功能已关闭，跳过导航连接";
        return;
    }

    connect(ui->TBtn_HomePage, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(0);
        updateNavButtonStyles(nullptr);
    });

    /* connect(ui->Btn_SwitchVerticalSupport, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(1);
        updateNavButtonStyles(ui->Btn_SwitchVerticalSupport);
    });

    connect(ui->Btn_SwitchHorizontalSupport, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(2);
        updateNavButtonStyles(ui->Btn_SwitchHorizontalSupport);
    });

    connect(ui->Btn_SwitchAGV, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(4);
        writeToAGVDevice(6, 2);
        updateNavButtonStyles(ui->Btn_SwitchAGV);
    });

    connect(ui->Btn_SwitchEOAT, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(3);
        updateNavButtonStyles(ui->Btn_SwitchEOAT);
    }); */
}

void MainWindow::setupRecordAndPermissionConnections()
{
    if (!isBigFeatureEnabled("operation_records") && !isBigFeatureEnabled("permission_system")) {
        qDebug() << "记录与权限功能均关闭，跳过相关连接";
        return;
    }

    connect(ui->TBtn_HistoryRecord, &QPushButton::clicked, this, [this]() {
        if (m_currentUserRole >= UserRole::Admin) {
            ui->StackedWidget->setCurrentIndex(6);
            updateRecordDisplay();
            showNotification("已进入操作记录页面");
        } else {
            const QString tip = "权限不足：查看历史记录需要管理员权限";
            showNotification(tip);
            updateStatusTip(tip);
        }
    });

    ui->TBtn_MoveMode->setText("关节模式");

    connect(ui->TBtn_MoveMode, &QPushButton::clicked, [=]() {
        m_isJointMode = !m_isJointMode;
        if (m_isJointMode) {
            writeToMainDevice(525, 2);
            ui->TBtn_MoveMode->setText("关节模式");
            showNotification("已切换至关节模式");
        } else {
            writeToMainDevice(525, 1);
            ui->TBtn_MoveMode->setText("坐标模式");
            showNotification("已切换至坐标模式");
        }
    });

    connect(ui->TBtn_PermissionPage, &QPushButton::clicked, [=]() {
        ui->StackedWidget->setCurrentIndex(5);
    });

    connect(m_recorder, &OperationRecorder::recordAdded, this, [this](const OperationRecord &record) {
        updateRecordDisplay();
        
        // 使用 mapping 转换显示内容
        if (m_mappingConfig && record.controlType != "Login" && record.controlType != "Logout") {
            QString mappedPage = m_mappingConfig->mapPageName(record.pageName);
            QString mappedControl = m_mappingConfig->mapControlName(record.controlName);
            QString mappedOp = m_mappingConfig->mapOperation(record.operation);
            QString mappedValue = m_mappingConfig->mapValue(record.newValue.toString());
            
            QString tip = QString("[%1] %2 -> %3 -> %4")
                .arg(record.timestamp.toString("hh:mm:ss"))
                .arg(mappedControl.isEmpty() ? record.controlName : mappedControl)
                .arg(mappedOp.isEmpty() ? record.operation : mappedOp)
                .arg(mappedValue.isEmpty() ? record.newValue.toString() : mappedValue);
            
            updateStatusTip(tip);
        }
    });
    connect(m_recorder, &OperationRecorder::recordsCleared,
            this, &MainWindow::updateRecordDisplay);
}

void MainWindow::setupControlConnections()
{
    if (!isBigFeatureEnabled("motion_control")) {
        qDebug() << "运动控制功能已关闭，跳过控制连接";
        return;
    }

    connect(ui->StackedWidget, &QStackedWidget::currentChanged,
            this, [this](int index) {
                const QString pageName = m_pageNames.value(index, "未知");
                qDebug() << "切换到页面:" << pageName;

                if (m_pageSliders.contains(pageName)) {
                    qDebug() << "当前页面有" << m_pageSliders[pageName].size()
                             << "个TechSliderLabel控件";
                }
            });

    connect(ui->TBtn_Stepmove, &QToolButton::clicked,
            this, &MainWindow::onStepMoveButtonClicked);

    m_controlModeBtn = findChild<QToolButton*>("TBtn_ControlMode");
    if (m_controlModeBtn) {
        connect(m_controlModeBtn, &QToolButton::clicked,
                this, &MainWindow::onControlModeClicked);
    } else {
        qWarning() << "未找到TBtn_ControlMode按钮";
    }

    m_btnForceControl = findChild<TechPushButton*>("btn_ForceControl");
    if (m_btnForceControl) {
        m_btnForceControl->setText(m_forcecontrolMode ? "力控开启" : "力控关闭");
        if (m_forcecontrolMode) {
            m_btnForceControl->setPrimaryColor(QColor("#00C8FF"));
            m_btnForceControl->setGlowColor(QColor(0, 200, 255, 180));
        } else {
            m_btnForceControl->setPrimaryColor(QColor("#7F8C8D"));
            m_btnForceControl->setGlowColor(QColor(127, 140, 141, 100));
        }
        connect(m_btnForceControl, &TechPushButton::clicked,
                this, &MainWindow::toggleForceControl);
        qDebug() << "力控按钮初始化完成，初始状态:"
                 << (m_forcecontrolMode ? "开启" : "关闭");
    } else {
        qWarning() << "未找到btn_ForceControl按钮";
    }
}

void MainWindow::setupSubsystemConnections()
{
    if (isFeatureEnabled("motion_control", "motion.steering_mode") && m_steeringModeSelector) {
        connect(m_steeringModeSelector, &SteeringModeSelector::modeChanged,
                this, &MainWindow::onSteeringModeChanged);
    }

    if (isBigFeatureEnabled("operation_records") && m_recorder) {
        connect(m_recorder, &OperationRecorder::tcpConnectionStatusChanged,
                this, &MainWindow::onTcpConnectionStatusChanged);
        connect(m_recorder, &OperationRecorder::tcpTransmissionComplete,
                this, &MainWindow::onTcpTransmissionComplete);
        connect(m_recorder, &OperationRecorder::tcpTransmissionError,
                this, &MainWindow::onTcpTransmissionError);
    }
}


// 样式设置
void MainWindow::setupStyles()
{
    if (!isFeatureEnabled("ui_navigation", "ui.styles")) {
        qDebug() << "UI样式功能已关闭，跳过样式设置";
        return;
    }

    //设定所有普通按钮的样式


    //模式切换btn
    // QList<QPushButton *>CommonBtns = {
    //     ui->Btn_SwitchHorizontalSupport,ui->Btn_SwitchVerticalSupport,ui->Btn_SwitchEOAT,ui->Btn_SwitchAGV
    // };
    //方向TBtn
    QList<QToolButton *>CommonTBtns = this->findChildren<QToolButton*>();
    //普通LineEdit
    // QList<QLineEdit *>StatusLEdit = {
    //     ui->lineEdit_46,ui->lineEdit_47
    // };
    //所有LEdit
    QList<QLineEdit*>AllLEdits = this->findChildren<QLineEdit*>();


    // 应用样式
    // applyPushButtonStyles(CommonBtns);
    applyToolButtonStyles(CommonTBtns);
    applyLineEditStyles(AllLEdits);

    //record
    // 设置蓝色背景
    ui->page_HistoryRecord->setStyleSheet(
        "QWidget#recordPage {"
        "    background-color: #1a5fb4;"  // 蓝色背景
        "}"

        "QLabel#recordTitle {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 20px;"
        "    font-weight: bold;"
        "}"

        "QLabel#timeLabel {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 12px;"
        "}"

        "QWidget#controlPanel {"
        "    background-color: rgba(30, 60, 120, 0.7);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "}"

        "QPushButton {"
        "    background-color: #2d7fda;"  // 按钮蓝色
        "    color: #ffffff;"  // 按钮文字白色
        "    border: 1px solid #4a9eff;"
        "    border-radius: 5px;"
        "    padding: 6px 12px;"
        "    font-size: 12px;"
        "}"

        "QPushButton:hover {"
        "    background-color: #4a9eff;"  // 悬停时浅蓝色
        "}"

        "QPushButton:pressed {"
        "    background-color: #1a5fb4;"  // 按下时深蓝色
        "}"

        "QComboBox {"
        "    background-color: #2d7fda;"  // 下拉框蓝色
        "    color: #ffffff;"  // 下拉框文字白色
        "    border: 1px solid #4a9eff;"
        "    border-radius: 5px;"
        "    padding: 4px 8px;"
        "    min-width: 100px;"
        "}"

        "QComboBox::drop-down {"
        "    border: none;"
        "}"

        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 1px solid #4a9eff;"
        "    padding-left: 8px;"
        "}"

        "QComboBox QAbstractItemView {"
        "    background-color: #2d7fda;"  // 下拉列表蓝色
        "    color: #ffffff;"  // 下拉列表文字白色
        "    selection-background-color: #4a9eff;"  // 选中项浅蓝色
        "}"

        "QTextEdit#recordDisplay {"
        "    background-color: rgba(30, 60, 120, 0.5);"  // 半透明深蓝色
        "    color: #ffffff;"  // 文字白色
        "    border: 1px solid #2d7fda;"
        "    border-radius: 8px;"
        "    font-family: Consolas, Courier New, monospace;"
        "    font-size: 12px;"
        "    padding: 10px;"
        "}"

        "QWidget#statsPanel {"
        "    background-color: rgba(30, 60, 120, 0.7);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "}"

        "QLabel {"
        "    color: #ffffff;"  // 所有标签文字白色
        "    font-size: 12px;"
        "}"

        "QLabel[objectName^='stats'] {"
        "    color: #ffffff;"  // 统计标签白色
        "    font-weight: bold;"
        "}"
        );
}

void MainWindow::applyPushButtonStyles(const QList<QPushButton*> &buttons)
{
    QString style = TransparentWidgetStyle("QPushButton");

    for(QPushButton *btn : buttons) {
        if(btn) {
            btn->setStyleSheet(style);
        } else {
            qDebug() << "发现空的QPushButton指针";
        }
    }
}


void MainWindow::applyToolButtonStyles(const QList<QToolButton*> &buttons)
{
    QString style = BlueWidgetStyle("QToolButton");

    for(QToolButton *btn : buttons) {
        if(btn) {
            btn->setStyleSheet(style);
        } else {
            qDebug() << "发现空的QToolButton指针";
        }
    }
}

QString MainWindow::BlueWidgetStyle(const QString &WidgetType )
{
    return QString(
               "%1 {"
               "    background-color: transparent    ;"
               "    color: white                 ;"
               "    border: none ;                "
               "    border-radius: 6px;           "
               "    padding: 1px 1px;             "
               "    font-weight: bold;"
               "    font-family: 'Microsoft YaHei', 'Segoe UI';"
               "   font-size: 14px;"
               "}"
               "%1:hover {"
               "    background-color: #2980B9;"
               "}"
               "%1:pressed {"
               "    background-color: #21618C;"
               "}"
               "%1:disabled {"
               "    background-color: #BDC3C7;"
               "    color: #7F8C8D;"
               "}"
               ).arg(WidgetType);
}
/**
 * @brief 返回一个透明风格的样式表模板
 *
 * 该函数根据传入的 `WidgetType` 返回一个最小样式表，用于将控件背景设为透明并移除边框，
 * 常用于工具按钮和导航按钮等需要融合背景的场景。
 *
 * @param WidgetType 要应用样式的控件类型（例如 `QPushButton`, `QToolButton`）
 * @return QString 用于 `setStyleSheet()` 的样式表字符串
 */
QString MainWindow::TransparentWidgetStyle(const QString &WidgetType )
{
    return QString(
               "%1 { background: transparent; border: none; }"
               ).arg(WidgetType);
}


// 动画设置
void MainWindow::setupAnimations()
{
    if (!isFeatureEnabled("ui_navigation", "ui.animations")) {
        qDebug() << "UI动画功能已关闭，跳过动画设置";
        return;
    }

    /**
     * @brief 配置并启动界面动画（如果有）
     *
     * 该函数集中管理界面上可复用的 QMovie/QPropertyAnimation 等动画对象，
     * 并将其与相应控件绑定。例如速度指示器的闪烁、报警图标的动画等。
     * 在当前实现中，动画内容可能为空（保留扩展点）。
     */

}
// 历史记录页面初始化
void MainWindow::initializeHistoryPage()
{
    /**
     * @brief 初始化历史记录页面 UI 元素
     *
     * 包括设置记录列表、过滤器、导出/清除按钮的初始状态与连接回调。
     * 该函数确保历史记录页面在首次显示前完成必需的控件配置。
     */

}
//显示背景
void MainWindow::loadBackgroundImage()
{
    /**
     * @brief 加载并缓存主窗口背景图片
     *
     * 优先从资源文件加载图片并缓存到 `m_backgroundPixmap`，以便在 `paintEvent`
     * 中直接绘制，避免每次重绘都进行磁盘/资源读取造成性能开销。
     */
    // 只在初始化时加载一次图片
    QPixmap pix;
    if(pix.load(":/Picture/background9.png"))
    {
        // 缩放到窗口大小并缓存
        m_backgroundPixmap = pix.scaled(this->size(),
                                        Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);
        m_backgroundLoaded = true;
        qDebug() << "背景图片预加载成功，尺寸:" << m_backgroundPixmap.size();
    }
    else
    {
        qDebug() << "无法加载背景图片";
        m_backgroundLoaded = false;
    }
}

// 修改 paintEvent 函数
void MainWindow::paintEvent(QPaintEvent *)
{
    // 如果已经预加载了图片，直接绘制缓存的图片
    if (m_backgroundLoaded)
    {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_backgroundPixmap);
        return;
    }

    // 备用方案：如果预加载失败，使用简单背景色
    QPainter painter(this);
    painter.fillRect(this->rect(), QColor(25, 148, 225));
}

// 修改事件过滤器(虚拟键盘)
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 处理LineEdit点击事件
    if (event->type() == QEvent::MouseButtonPress) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(obj);
        if (lineEdit && lineEdit->isEnabled() && this->isAncestorOf(lineEdit)) {
            // // 检查是否为管理员页面的密码输入框
            // if (lineEdit->objectName() == "passwordEdit") {
            //     // 对于密码框，我们可能需要特殊处理
            //     // 或者直接返回false让系统处理
            //     return false;
            // }

            // 显示虚拟键盘
            if (m_virtualKeyboard) {
                m_virtualKeyboard->setTargetLineEdit(lineEdit);
                m_virtualKeyboard->showAtWidget(lineEdit);
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}


// 修改applyLineEditStyles，移除只读设置
void MainWindow::applyLineEditStyles(const QList<QLineEdit*> &lineEdits)
{
    for(QLineEdit *edit : lineEdits) {
        if(edit) {
            // 设置数字验证器
            QRegularExpression  regExp("[0-9]*\\.?[0-9]*");
            QRegularExpressionValidator *validator = new QRegularExpressionValidator(regExp, this);
            edit->setValidator(validator);

            // 设置样式
            edit->setStyleSheet(
                "QLineEdit {"
                "    background-color: white;"
                "    border: 2px solid #3498DB;"
                "    border-radius: 6px;"
                "    padding: 8px 12px;"
                "    font-size: 12px;"
                "    color: #2C3E50;"
                "    selection-background-color: #3498DB;"
                "}"
                "QLineEdit:focus {"
                "    border-color: #2980B9;"
                "    background-color: #F8F9FA;"
                "}"
                );
        } else {
            qDebug() << "发现空的QLineEdit指针";
        }
    }
}



//科技感按钮
void MainWindow::initTechButtons() {
    // 1. 清空列表
    m_techButtons.clear();

    // 2. 查找所有 TechPushButton 类型的子对象
    // 此方法会将所有已提升的按钮加入到 m_techButtons 列表中
    QList<TechPushButton*> foundButtons = this->findChildren<TechPushButton*>();

    // 3. 遍历列表，应用统一的科技感配置
    for (TechPushButton* btn : foundButtons) {
        // 启用基本效果
        // btn->enableHoverAnimation(true);
        // btn->enableClickAnimation(true);
        // btn->setTextGlow(true);

        // 根据不同需求选择样式，以全息和能量风格为例：
        // a) 全息样式 (Holographic)
        btn->setButtonStyle(TechPushButton::StyleHolographic);
        btn->setPrimaryColor(QColor(0, 200, 255, 150)); // 半透明蓝
        btn->setSecondaryColor(QColor(255, 0, 255, 150)); // 半透明紫
        // btn->enableScanLine(true); // 启用扫描线动画

        // b) 能量样式 (Energy)
        // btn->setButtonStyle(TechPushButton::StyleEnergy);
        // btn->setPrimaryColor(QColor(255, 100, 0)); // 橙色
        // btn->setSecondaryColor(QColor(255, 220, 0)); // 黄色
        // btn->enablePulseEffect(true); // 启用脉冲动画

        // 4. 连接到信号槽（例如，统一连接到处理函数）
        // connect(btn, &TechPushButton::clicked, this, &MainWindow::onTechButtonClicked);

        // 5. 存储到成员变量列表以备后用
        m_techButtons.append(btn);
    }



}





void MainWindow::initSpeedGaugeUI()
{
    // 初始化 4 个环形仪表 (TechArcGauge)
    // 映射关系：widget_test1 -> arcGauge_1, widget_test2 -> arcGauge_2, ...
    struct ArcConfig {
        QWidget* placeholder;
        QString name;
        QString label;
        QString suffix;
        double min;
        double max;
        int precision;
    };

    QList<ArcConfig> configs = {
        {ui->widget_test1, "robot_ArcGauge_J1Angle", "悬臂角度", "°", -90, 90, 1},
        {ui->widget_test2, "robot_ArcGauge_J2Height", "升降高度", "mm", -850, 1150, 0},
        {ui->widget_test3, "robot_ArcGauge_J3Length", "总伸展长度", "mm", 0, 1600, 0},
        {ui->widget_test4, "robot_ArcGauge_J4Angle", "末端角度", "°", -180, 180, 1}
    };

    for (int i = 0; i < configs.size(); ++i) {
        const auto& cfg = configs[i];
        if (cfg.placeholder) {
            TechArcGauge *arcGauge = new TechArcGauge(cfg.placeholder->parentWidget());
            arcGauge->setGeometry(cfg.placeholder->geometry());
            arcGauge->setObjectName(cfg.name);
            arcGauge->setRange(cfg.min, cfg.max);
            arcGauge->setValue(0);
            
            // 如果是 J3，设置速度显示相关参数
            if (cfg.name == "robot_ArcGauge_J3Length") {
                arcGauge->setSecondMaximum(40.0); // J3 速度范围 0~40 mm/s
                arcGauge->setSecondSuffix("mm/s");
                arcGauge->setSecondValue(0.0);
            } else if (cfg.name == "robot_ArcGauge_J1Angle") {
                arcGauge->setSecondMaximum(2.0);  // J1 速度范围 0~2 °/s
                arcGauge->setSecondSuffix("°/s");
            } else if (cfg.name == "robot_ArcGauge_J2Height") {
                arcGauge->setSecondMaximum(15.0); // J2 速度范围 0~15 mm/s
                arcGauge->setSecondSuffix("mm/s");
            } else if (cfg.name == "robot_ArcGauge_J4Angle") {
                arcGauge->setSecondMaximum(2.0);  // J4 速度范围 0~2 °/s
                arcGauge->setSecondSuffix("°/s");
            }
            
            arcGauge->setLabelText(cfg.label);
            arcGauge->setSuffix(cfg.suffix);
            arcGauge->setPrecision(cfg.precision);
            arcGauge->setForceControlMode(true);
            
            cfg.placeholder->hide();
            arcGauge->show();

            // 存入映射表，Key 使用规范化后的名称
            m_arcGauges[cfg.name] = arcGauge;
        }
    }

    // 1. 创建 QQuickWidget 来承载 QML
    m_speedGaugeQml = new QQuickWidget(this);
    m_speedGaugeQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_speedGaugeQml->setSource(QUrl("qrc:/TechSpeedGauge.qml"));
    m_speedGaugeQml->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_speedGaugeQml->setClearColor(Qt::transparent);

    // 2. 将原本放置 TechSpeedGauge 的布局或位置替换掉
    if (ui->AGV_speedGauge) {
        QWidget *parent = ui->AGV_speedGauge->parentWidget();
        m_speedGaugeQml->setParent(parent);
        m_speedGaugeQml->setGeometry(ui->AGV_speedGauge->geometry());
        ui->AGV_speedGauge->hide(); // 隐藏旧的 C++ 控件
        m_speedGaugeQml->show();
        m_speedGaugeQml->raise();
    }

    // 3. 初始化 QML 属性
    QQuickItem *rootItem = m_speedGaugeQml->rootObject();
    if (rootItem) {
        rootItem->setProperty("maxValue", 900);
        rootItem->setProperty("title", "行驶速度");
        rootItem->setProperty("unit", "mm/s");
    }

    qDebug() << "QML 速度仪表初始化完成 - 量程: 0-900 mm/s";
}

// 示例：更新速度值
void MainWindow::updateSpeed(qreal newSpeed)
{
    // 更新 QML 属性实现平滑过渡（QML 内部有动画逻辑）
    if (m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
        m_speedGaugeQml->rootObject()->setProperty("currentValue", newSpeed);
    }
}

//模拟速度
void MainWindow::setupDataSimulation()
{
    // 创建定时器（用于测试，实际使用时注释掉）
    m_dataSimulator = new QTimer(this);
    connect(m_dataSimulator, &QTimer::timeout, this, [this]() {
        if (!(m_speedGaugeQml && m_speedGaugeQml->rootObject())) {
            qDebug() << "错误：m_speedGaugeQml 为空指针！";
            return;
        }
        // 生成随机速度波动（测试用）
        qreal current = m_speedGaugeQml->rootObject()->property("currentValue").toReal();
        qreal change = (QRandomGenerator::global()->bounded(100) / 10.0) - 5.0;
        qreal newSpeed = qBound(0.0, current + change, 900.0);

        updateSpeed(newSpeed);
    });

    // 临时测试：使用 pushButton_5 (工艺页) 手动触发力控报警
    // 因为 Btn_Test 在当前UI中不存在
    if (ui->pushButton_5) {
        ui->pushButton_5->setText("测试报警"); 
        connect(ui->pushButton_5, &QPushButton::clicked, this, [this]() {
            qDebug() << "测试按钮点击：手动触发力控超限报警";
            showAlarm("力控超限警报触发\n请点击下方按钮清除报警\n请手动移出超限位置", "#ff8800"); 
        });
    }
    
    // 原有的 Btn_Test 代码已删除
}

void MainWindow::initSliderEditUI()
{
    QList<TechSliderEdit*> sliders = this->findChildren<TechSliderEdit*>();
    qDebug() << "找到" << sliders.size() << "个TechSliderEdit控件";

    // 存储所有非AGV的TechSliderEdit，用于联动更新
    QList<TechSliderEdit*> nonAGVSliders;

    for (TechSliderEdit *slider : sliders) {
        QString objName = slider->objectName();

        // 根据控件对象名设置特定的值范围
        if (objName == "TechSliderEdit_HoriSupSec_RotationSpeed") {
            // 水平支撑段旋转速度：0-5 °/s，支持一位小数
            slider->setLabelText("旋转速度");
            slider->setRange(0, 5);
            slider->setValue(1); // 默认值设为中间值
            slider->setSuffix("°/s");
            slider->setPrecision(1); // 可以输入一位小数
            nonAGVSliders.append(slider);

            qDebug() << "初始化: TechSliderEdit_HoriSupSec_RotationSpeed, 范围:0-5 °/s, 默认值:1 °/s, 精度:1";
        }
        else if (objName == "TechSliderEdit_HoriSupSec_MoveSpeed") {
            // 水平支撑段移动速度：0-20 mm/s，只能输入整数
            slider->setLabelText("伸缩速度");
            slider->setRange(0, 20);
            slider->setValue(10); // 默认值设为中间值
            slider->setSuffix("mm/s");
            slider->setPrecision(0); // 只能输入整数
            nonAGVSliders.append(slider);

            qDebug() << "初始化: TechSliderEdit_HoriSupSec_MoveSpeed, 范围:0-20 mm/s, 默认值:10 mm/s";
        }
        else if (objName == "TechSliderEdit_VeSupSec_MoveSpeed") {
            // 垂直支撑段移动速度：0-30 mm/s，只能输入整数
            slider->setLabelText("升降速度");
            slider->setRange(0, 35);
            slider->setValue(15); // 默认值设为中间值
            slider->setSuffix("mm/s");
            slider->setPrecision(0); // 只能输入整数
            nonAGVSliders.append(slider);

            qDebug() << "初始化: TechSliderEdit_VeSupSec_MoveSpeed, 范围:0-30 mm/s, 默认值:15 mm/s";
        }
        else if (objName == "TechSliderEdit_EOAT_RotationSpeed") {
            // EOAT旋转速度：0-5 °/s，支持一位小数
            slider->setLabelText("全局速度");
            slider->setRange(0, 100);
            slider->setValue(3); // 默认值设为整数
            slider->setSuffix("%");
            slider->setPrecision(0); // 可以输入一位小数
            nonAGVSliders.append(slider);

            qDebug() << "初始化: TechSliderEdit_EOAT_RotationSpeed, 范围:0-5 °/s, 默认值:3 °/s, 精度:1";
        }
        // else if (objName == "SEdit_AGV_MoveSpeed") {
        //     // AGV运动速度：0-834 mm/s，可以输入小数
        //     slider->setLabelText("运动速度");
        //     slider->setRange(0, 834);
        //     slider->setValue(0);
        //     slider->setSuffix("mm/s");
        //     slider->setPrecision(0);

        //     qDebug() << "初始化: SEdit_AGV_MoveSpeed, 范围:0-834 mm/s, 默认值:0 mm/s";
        // }
        // else if (objName == "SEdit_AGV_Angle") {
        //     // AGV转向角度：-25-25 °，可以输入小数
        //     slider->setLabelText("转向角度");
        //     slider->setRange(-25, 25);
        //     slider->setValue(0);
        //     slider->setSuffix("°");
        //     slider->setPrecision(0);

        //     qDebug() << "初始化: SEdit_AGV_Angle, 范围:-25-25 °, 默认值:0 °";
        // }
        else {
            // 如果有其他未处理的TechSliderEdit控件，输出警告
            qWarning() << "未处理的TechSliderEdit控件:" << objName;
        }

        // 记录到slider列表
        m_sliders.append(slider);
    }

    // 为非AGV的TechSliderEdit连接联动更新信号
    for (TechSliderEdit *slider : nonAGVSliders) {
        connect(slider, &TechSliderEdit::valueChangedWithRecord,
                this, [this, slider, nonAGVSliders](double oldValue, double newValue) {
                    Q_UNUSED(oldValue);
                    // 只处理非AGV的TechSliderEdit
                    QString objName = slider->objectName();
                    if (objName != "SEdit_AGV_MoveSpeed" && objName != "SEdit_AGV_Angle") {
                        onNonAGVSliderEditChanged(slider, newValue, nonAGVSliders);
                    }
                });
    }
}

/**
 * @brief 处理非 AGV 的滑块编辑值变化并实现联动
 *
 * 当页面中非 AGV 的 `TechSliderEdit` 值发生变化时，调用此函数完成：
 * - 更新相关联的滑块标签或其他 UI 显示
 * - 将变化记录到 `OperationRecorder`（如需要）
 * - 执行必要的边界校验与同步写入（如果配置了 Modbus 写入）
 *
 * @param changedSlider 发生变化的滑块控件指针
 * @param newValue 新值
 * @param allNonAGVSliders 页面内所有非 AGV 的滑块列表（用于联动计算）
 */
void MainWindow::onNonAGVSliderEditChanged(TechSliderEdit *changedSlider, double newValue,
                                           const QList<TechSliderEdit*> &allNonAGVSliders)
{
    qDebug() << "非AGV TechSliderEdit值变化：" << changedSlider->objectName()
             << "新值:" << newValue;

    // 1. 计算百分比：值 / 最大值 × 100，取整
    double maxValue = changedSlider->maximum();
    double percentage = (newValue / maxValue) * 100.0;
    int percentageInt = qRound(percentage); // 四舍五入取整

    qDebug() << "计算百分比: (" << newValue << " / " << maxValue << ") × 100 = "
             << percentage << "% → 取整后:" << percentageInt << "%";

    // 2. 写入192.168.1.13设备的79地址
    writeToMainDevice(79, percentageInt);

    // 3. 更新所有其他非AGV TechSliderEdit的值
    for (TechSliderEdit *slider : allNonAGVSliders) {
        if (slider != changedSlider) {
            double sliderMax = slider->maximum();
            double newSliderValue = sliderMax * percentageInt / 100.0;

            // 四舍五入到整数（因为非AGV控件只能输入整数）
            newSliderValue = qRound(newSliderValue);

            // 确保值在范围内
            if (newSliderValue < slider->minimum()) newSliderValue = slider->minimum();
            if (newSliderValue > sliderMax) newSliderValue = sliderMax;

            qDebug() << "更新" << slider->objectName() << ": "
                     << sliderMax << " × " << percentageInt << "% / 100 = "
                     << newSliderValue;

            // 设置新值（会触发记录信号）
            slider->setValue(newSliderValue);
        }
    }
}

void MainWindow::initSliderLabelUI()
{
    // 清空现有记录
    m_sliderLabels.clear();
    m_sliderLabelInstances.clear();
    m_pageSliders.clear();

    // 获取首页的所有TechSliderLabel
    QWidget* mainPage = ui->StackedWidget->widget(0);  // 假设首页是索引0
    if (!mainPage) return;

    // 查找首页中的所有TechSliderLabel
    QList<TechSliderLabel*> mainPageSliders = mainPage->findChildren<TechSliderLabel*>();

    for (TechSliderLabel* sliderLabel : mainPageSliders) {
        QString objName = sliderLabel->objectName();

        // 检查是否在配置中
        if (m_sliderLabelConfigs.contains(objName)) {
            const SliderLabelConfig& config = m_sliderLabelConfigs[objName];

            // 设置UI属性
            sliderLabel->setLabelText(config.labelText);
            sliderLabel->setRange(config.minValue, config.maxValue);
            sliderLabel->setValue(config.defaultValue);
            sliderLabel->setSuffix(config.suffix);
            sliderLabel->setPrecision(config.precision); // 设置精度

            // 设置样式
            sliderLabel->setTechBlueStyle();

            // 添加到管理列表
            m_sliderLabelInstances[objName] = sliderLabel;
            m_sliderLabels.append(sliderLabel);

            // 添加到页面映射
            m_pageSliders["软件参数"].append(sliderLabel);  // 首页名称

            qDebug() << "初始化TechSliderLabel:" << objName
                     << "文本:" << config.labelText
                     << "范围:" << config.minValue << "-" << config.maxValue;
        } else {
            qWarning() << "未配置的TechSliderLabel:" << objName;
        }
    }
}














void MainWindow::on_TBtn_VeSupSec_Rise_released()
{
    qDebug() << "按钮释放，尝试停止动图...";

    if (m_verticalMovie && m_verticalMovie->state() == QMovie::Running) {
        m_verticalMovie->stop();
        qDebug() << "动图已停止。";
    }

}
/**
 * @brief 初始化并安装虚拟键盘
 *
 * 创建 `TechVirtualKeyboard` 并为所有 `QLineEdit` 安装事件过滤器，使得点击
 * 输入框时能弹出虚拟键盘进行输入。虚拟键盘通过 `eventFilter` 捕获点击事件，
 * 并展示在目标控件附近。
 */
void MainWindow::setupVirtualKeyboard()
{
    if (!isFeatureEnabled("ui_navigation", "ui.virtual_keyboard")) {
        qDebug() << "虚拟键盘功能已关闭，跳过初始化";
        return;
    }

    // 创建虚拟键盘
    m_virtualKeyboard = new TechVirtualKeyboard(this);

    // 为所有LineEdit安装事件过滤器
    QList<QLineEdit*> allLineEdits = this->findChildren<QLineEdit*>();
    for (QLineEdit *edit : allLineEdits) {
        edit->installEventFilter(this);
    }
}



/*********************************历史记录**********************/

void MainWindow::connectRecordSignals()
{
    /**
     * @brief 连接界面控件到操作记录器的记录信号
     *
     * 遍历页面中的滑块、按钮、工具按钮等控件，连接其产生的用户操作信号
     * 到 `OperationRecorder` 的记录函数，从而实现全局操作记录与审计。
     */
    // 连接所有TechSliderEdit的记录信号
    QList<TechSliderEdit*> allSliders = this->findChildren<TechSliderEdit*>();
    for (TechSliderEdit* slider : allSliders) {
        // 获取控件所在的页面
        QWidget *page = qobject_cast<QWidget*>(slider->parent());
        int pageIndex = -1;

        // 查找控件属于哪个StackedWidget页面
        while (page && page != this) {
            QStackedWidget *stack = qobject_cast<QStackedWidget*>(page->parent());
            if (stack) {
                pageIndex = stack->indexOf(page);
                break;
            }
            page = page->parentWidget();
        }

        if (pageIndex != -1) {
            // 使用lambda捕获页面信息
            connect(slider, &TechSliderEdit::valueChangedWithRecord,
                    this, [this, slider, pageIndex](double oldValue, double newValue) {
                        OperationRecord record;
                        record.timestamp = QDateTime::currentDateTime();
                        record.pageName = MappingConfig::instance()->mapPageName(QString::number(pageIndex));
                        record.controlName = MappingConfig::instance()->mapControlName(slider->objectName());
                        record.controlType = MappingConfig::instance()->mapControlType("TechSliderEdit");
                        record.operation = MappingConfig::instance()->mapOperation("valueChanged");
                        record.oldValue = QString::number(oldValue);
                        record.newValue = QString::number(newValue);

                        m_recorder->addRecord(record);
                    });
        }
    }
    // 连接所有TechPushButton的记录信号
    QList<TechPushButton*> allButtons = this->findChildren<TechPushButton*>();
    for (TechPushButton* button : allButtons) {
        connect(button, &TechPushButton::clicked,
                this, [this, button]() {
                    // 获取按钮所在页面
                    QWidget *page = qobject_cast<QWidget*>(button->parent());
                    int pageIndex = -1;

                    while (page && page != this) {
                        QStackedWidget *stack = qobject_cast<QStackedWidget*>(page->parent());
                        if (stack) {
                            pageIndex = stack->indexOf(page);
                            break;
                        }
                        page = page->parentWidget();
                    }

                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = MappingConfig::instance()->mapPageName(QString::number(pageIndex));
                    record.controlName = MappingConfig::instance()->mapControlName(button->objectName());
                    record.controlType = MappingConfig::instance()->mapControlType("TechPushButton");
                    record.operation = MappingConfig::instance()->mapOperation("clicked");
                    record.oldValue = "";
                    record.newValue = MappingConfig::instance()->mapValue(button->text());

                    m_recorder->addRecord(record);
                });
    }
    // 连接所有QToolButton的记录信号
    QList<QToolButton*> allToolButtons = this->findChildren<QToolButton*>();
    for (QToolButton* toolButton : allToolButtons) {
        // 跳过记录页面和管理员页面的按钮，避免记录操作记录的操作
        if (toolButton->objectName().contains("record", Qt::CaseInsensitive) ||
            toolButton->objectName().contains("admin", Qt::CaseInsensitive)) {
            continue;
        }

        // 记录点击事件
        connect(toolButton, &QToolButton::clicked,
                this, [this, toolButton]() {
                    // 获取按钮所在页面
                    QString pageName = getControlPageName(toolButton);

                    OperationRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.pageName = MappingConfig::instance()->mapPageName(pageName);
                    record.controlName = MappingConfig::instance()->mapControlName(toolButton->objectName());
                    record.controlType = MappingConfig::instance()->mapControlType("QToolButton");
                    record.operation = MappingConfig::instance()->mapOperation("clicked");
                    record.oldValue = "";
                    record.newValue = MappingConfig::instance()->mapValue(toolButton->text().isEmpty() ? toolButton->toolTip() : toolButton->text());

                    m_recorder->addRecord(record);

                    // 在状态栏显示通知
                    showNotification(QString("工具按钮点击: %1").arg(record.controlName));
                });

        // 如果按钮有toggle状态，也记录
        if (toolButton->isCheckable()) {
            connect(toolButton, &QToolButton::toggled,
                    this, [this, toolButton](bool checked) {
                        QString pageName = getControlPageName(toolButton);

                        OperationRecord record;
                        record.timestamp = QDateTime::currentDateTime();
                        record.pageName = pageName;
                        record.controlName = toolButton->objectName();
                        record.controlType = "QToolButton";
                        record.operation = "toggled";
                        record.oldValue = !checked;
                        record.newValue = checked;

                        m_recorder->addRecord(record);

                        showNotification(QString("工具按钮切换: %1 -> %2")
                                             .arg(record.controlName)
                                             .arg(checked ? "选中" : "未选中"));
                    });
        }
    }
}

void MainWindow::setupRecordUI()
{
    if (!isBigFeatureEnabled("operation_records")) {
        qDebug() << "操作记录功能已关闭，跳过记录UI初始化";
        return;
    }

    while (ui->StackedWidget->count() < 7) {
        QWidget *newPage = new QWidget();
        ui->StackedWidget->addWidget(newPage);
    }

    QWidget *recordPage = ui->StackedWidget->widget(6);
    if (!recordPage) {
        recordPage = new QWidget();
        ui->StackedWidget->insertWidget(6, recordPage);
    }

    // 清理旧布局
    if (recordPage->layout()) {
        QLayoutItem *item;
        while ((item = recordPage->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete recordPage->layout();
    }

    QVBoxLayout *layout = new QVBoxLayout(recordPage);
    layout->setContentsMargins(10, 0, 30, 10); // 增加左右边距，使中间的列表收窄
    layout->setSpacing(5);

    m_historyListQml = new QQuickWidget(this);
    m_historyListQml->setResizeMode(QQuickWidget::SizeRootObjectToView);
    connect(m_historyListQml, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
        if (status == QQuickWidget::Error && m_historyListQml) {
            const auto errs = m_historyListQml->errors();
            for (const auto &err : errs) {
                qWarning() << "HistoryList QML error:" << err.toString();
            }
        }
    });
    m_historyListQml->setSource(QUrl("qrc:/HistoryList.qml"));
    m_historyListQml->setClearColor(Qt::transparent);

    layout->addWidget(m_historyListQml);

    // 连接信号
    connect(m_recorder, &OperationRecorder::recordAdded, this, [this](const OperationRecord &record) {
        if (m_historyListQml && m_historyListQml->rootObject()) {
            QMetaObject::invokeMethod(m_historyListQml->rootObject(), "addRecord",
                Q_ARG(QVariant, record.timestamp.toString("hh:mm:ss")),
                Q_ARG(QVariant, record.pageName),
                Q_ARG(QVariant, record.controlName),
                Q_ARG(QVariant, record.operation),
                Q_ARG(QVariant, record.oldValue.toString()),
                Q_ARG(QVariant, record.newValue.toString()));
        }
    });

    connect(m_recorder, &OperationRecorder::recordsCleared, this, [this]() {
        if (m_historyListQml && m_historyListQml->rootObject()) {
            QMetaObject::invokeMethod(m_historyListQml->rootObject(), "clearRecords");
        }
    });

    qDebug() << "QML 操作记录列表初始化完成";
}

void MainWindow::updateRecordDisplay()
{
    // QML 列表会自动通过信号更新，无需主循环刷新
}

void MainWindow::onClearRecords()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认", "确定要清空所有操作记录吗？",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_recorder->clear();
    }
}

void MainWindow::onSaveRecords()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "保存操作记录", "operation_history.json", "JSON文件 (*.json)");

    if (!filename.isEmpty()) {
        if (m_recorder->saveToFile(filename)) {
            QMessageBox::information(this, "成功", "操作记录已保存");
        } else {
            QMessageBox::warning(this, "错误", "保存失败");
        }
    }
}

void MainWindow::onExportRecords()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "导出操作报告", "operation_report.txt", "文本文件 (*.txt)");

    if (!filename.isEmpty()) {
        if (m_recorder->exportToText(filename)) {
            QMessageBox::information(this, "成功", "操作报告已导出");
        } else {
            QMessageBox::warning(this, "错误", "导出失败");
        }
    }
}

void MainWindow::onFilterRecords()
{
    if (!isFeatureEnabled("operation_records", "records.filter_export")) {
        showNotification("记录筛选功能已关闭");
        return;
    }

    QComboBox *filterCombo = findChild<QComboBox*>();
    if (!filterCombo) return;

    QString filter = filterCombo->currentText();
    QPlainTextEdit *display = findChild<QPlainTextEdit*>("recordDisplay");

    if (!display) return;

    display->clear();
    const QList<OperationRecord> &records = m_recorder->records();

    if (filter == "显示全部") {
        for (const auto &record : records) {
            display->appendPlainText(record.toString());
        }
    } else if (m_pageNames.values().contains(filter)) {
        QList<OperationRecord> pageRecords = m_recorder->getPageRecords(filter);
        for (const auto &record : pageRecords) {
            display->appendPlainText(record.toString());
        }
    } else if (filter == "TechSliderEdit操作") {
        for (const auto &record : records) {
            if (record.controlType == "TechSliderEdit") {
                display->appendPlainText(record.toString());
            }
        }
    } else if (filter == "TechPushButton操作") {
        for (const auto &record : records) {
            if (record.controlType == "TechPushButton") {
                display->appendPlainText(record.toString());
            }
        }
    }
}
// 辅助函数：获取控件所在的页面名称
QString MainWindow::getControlPageName(QWidget *widget)
{
    QWidget *parent = widget->parentWidget();

    while (parent && parent != this) {
        QStackedWidget *stack = qobject_cast<QStackedWidget*>(parent);
        if (stack) {
            int pageIndex = stack->currentIndex();
            return m_pageNames.value(pageIndex, QString("页面%1").arg(pageIndex + 1));
        }

        // 如果父控件是StackedWidget的页面
        if (parent->parentWidget()) {
            QStackedWidget *stack2 = qobject_cast<QStackedWidget*>(parent->parentWidget());
            if (stack2) {
                int pageIndex = stack2->indexOf(parent);
                return m_pageNames.value(pageIndex, QString("页面%1").arg(pageIndex + 1));
            }
        }

        parent = parent->parentWidget();
    }

    return "未知页面";
}
/***************************************管理员界面**********************************/
void MainWindow::setupAdminPasswordPage()
{
    // 权限验证页面（第5页）
    QWidget *adminPage = new QWidget();
    adminPage->setObjectName("adminPasswordPage");
    ui->StackedWidget->insertWidget(5, adminPage); // 插入为第6页

    // 创建科技感背景
    QVBoxLayout *mainLayout = new QVBoxLayout(adminPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 背景容器
    QWidget *container = new QWidget(adminPage);
    container->setObjectName("adminContainer");
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setAlignment(Qt::AlignCenter);

    // 标题
    QLabel *titleLabel = new QLabel("权限验证", container);
    titleLabel->setObjectName("adminTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    // 角色选择下拉框
    QComboBox *roleComboBox = new QComboBox(container);
    roleComboBox->setObjectName("roleComboBox");
    roleComboBox->addItem("工程师 (Engineer)", QVariant::fromValue(static_cast<int>(UserRole::Engineer)));
    roleComboBox->addItem("管理员 (Admin)", QVariant::fromValue(static_cast<int>(UserRole::Admin)));
    roleComboBox->addItem("厂家 (Manufacturer)", QVariant::fromValue(static_cast<int>(UserRole::Manufacturer)));
    // QComboBox 没有 setAlignment 方法，移除该行

    // 密码输入框
    QLineEdit *passwordEdit = new QLineEdit(container);
    passwordEdit->setObjectName("passwordEdit");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText("请输入密码");
    passwordEdit->setAlignment(Qt::AlignCenter);

    // 密码提示
    QLabel *hintLabel = new QLabel("工程师: 456 | 管理员: 123", container);
    hintLabel->setObjectName("passwordHint");
    hintLabel->setAlignment(Qt::AlignCenter);

    // 登录按钮
    QPushButton *loginButton = new QPushButton("登录", container);
    loginButton->setObjectName("loginButton");

    // 功能开关管理按钮 (仅厂家)
    QPushButton *featureButton = new QPushButton("功能开关管理", container);
    featureButton->setObjectName("featureButton");
    featureButton->setStyleSheet(
        "background-color: #55007f; color: #ffaa00; font-weight: bold; border: 2px solid #ffaa00;"
    );
    featureButton->setVisible(false);

    // 注销按钮
    QPushButton *logoutButton = new QPushButton("注销 (返回操作员)", container);
    logoutButton->setObjectName("logoutButton");
    logoutButton->setVisible(false); // 默认隐藏，登录后显示

    // 网络配置 (左右排布：WIN7_IP 和 远程模拟器)
    QWidget *netConfigSection = new QWidget(container);
    netConfigSection->setObjectName("netConfigSection");
    QVBoxLayout *netMainLayout = new QVBoxLayout(netConfigSection);
    netMainLayout->setContentsMargins(0, 0, 0, 0);
    netMainLayout->setSpacing(10);

    // 第一行：WIN7_IP
    QHBoxLayout *row1Layout = new QHBoxLayout();
    QLabel *ipPrefix = new QLabel("WIN7_IP: 192.168.1.", netConfigSection);
    ipPrefix->setStyleSheet("color: #00ffff; font-family: 'Microsoft YaHei UI'; font-size: 14px;");
    QLineEdit *ipHostEdit = new QLineEdit(netConfigSection);
    ipHostEdit->setObjectName("ipHostEdit");
    ipHostEdit->setPlaceholderText("100");
    ipHostEdit->setText("100");
    ipHostEdit->setFixedWidth(40);
    ipHostEdit->setAlignment(Qt::AlignCenter);
    ipHostEdit->setValidator(new QIntValidator(0, 255, ipHostEdit));
    ipHostEdit->setStyleSheet("QLineEdit { background: rgba(0, 0, 0, 100); border: 1px solid #00c8ff; color: #ffaa00; border-radius: 4px; }");
    
    QPushButton *ipApplyBtn = new QPushButton("确认", netConfigSection);
    ipApplyBtn->setFixedWidth(50);
    ipApplyBtn->setStyleSheet("QPushButton { background-color: #004466; border: 1px solid #00c8ff; color: white; border-radius: 4px; font-size: 12px; }");

    row1Layout->addWidget(ipPrefix);
    row1Layout->addWidget(ipHostEdit);
    row1Layout->addWidget(ipApplyBtn);
    row1Layout->addStretch();

    // 第二行：远程模拟器
    QHBoxLayout *row2Layout = new QHBoxLayout();
    QLabel *simPrefix = new QLabel("远程模拟器: 192.168.1.", netConfigSection);
    simPrefix->setStyleSheet("color: #00ffff; font-family: 'Microsoft YaHei UI'; font-size: 14px;");
    QLineEdit *simHostEdit = new QLineEdit(netConfigSection);
    simHostEdit->setObjectName("simHostEdit");
    simHostEdit->setPlaceholderText("70");
    simHostEdit->setText("70");
    simHostEdit->setFixedWidth(40);
    simHostEdit->setAlignment(Qt::AlignCenter);
    simHostEdit->setValidator(new QIntValidator(0, 255, simHostEdit));
    simHostEdit->setStyleSheet("QLineEdit { background: rgba(0, 0, 0, 100); border: 1px solid #00c8ff; color: #ffaa00; border-radius: 4px; }");

    QPushButton *simApplyBtn = new QPushButton("确认", netConfigSection);
    simApplyBtn->setFixedWidth(50);
    simApplyBtn->setStyleSheet("QPushButton { background-color: #004466; border: 1px solid #00c8ff; color: white; border-radius: 4px; font-size: 12px; }");

    row2Layout->addWidget(simPrefix);
    row2Layout->addWidget(simHostEdit);
    row2Layout->addWidget(simApplyBtn);
    row2Layout->addStretch();

    netMainLayout->addLayout(row1Layout);
    netMainLayout->addLayout(row2Layout);
    netConfigSection->setVisible(false); // 默认隐藏

    // 错误提示
    QLabel *errorLabel = new QLabel("", container);
    errorLabel->setObjectName("errorLabel");
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setVisible(false);

    // 添加控件到容器
    containerLayout->addStretch(2);
    containerLayout->addWidget(titleLabel);
    containerLayout->addSpacing(30);
    containerLayout->addWidget(roleComboBox);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(passwordEdit);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(hintLabel);
    containerLayout->addSpacing(30);
    containerLayout->addWidget(loginButton);
    containerLayout->addWidget(featureButton);
    containerLayout->addWidget(netConfigSection); // 厂家登录后可见（包含本地IP及模拟器配置）
    containerLayout->addWidget(logoutButton);
    containerLayout->addSpacing(15);
    containerLayout->addWidget(errorLabel);
    containerLayout->addStretch(3);

    // 设置容器大小和居中
    container->setFixedSize(480, 560);

    // 添加容器到主布局
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->addStretch();
    centerLayout->addWidget(container);
    centerLayout->addStretch();

    mainLayout->addLayout(centerLayout);

    // 设置样式
    QString style = QString(
        "#adminPasswordPage {"
        "    background-color: #0a0a1a;"
        "}"
        "#adminContainer {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 rgba(20, 20, 40, 220),"
        "        stop:1 rgba(40, 20, 60, 200));"
        "    border: 2px solid #00c8ff;"
        "    border-radius: 15px;"
        "    padding: 30px;"
        "}"
        "#adminTitle {"
        "    font-family: 'Microsoft YaHei UI';"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    color: #00ffff;"
        "    padding: 10px;"
        "}"
        "#passwordEdit {"
        "    background-color: rgba(10, 10, 30, 180);"
        "    border: 2px solid #00c8ff;"
        "    border-radius: 8px;"
        "    padding: 12px 20px;"
        "    font-size: 16px;"
        "    color: #00ffff;"
        "    selection-background-color: #00c8ff;"
        "    min-width: 250px;"
        "}"
        "#roleComboBox {"
        "    background-color: rgba(10, 10, 30, 180);"
        "    border: 2px solid #00c8ff;"
        "    border-radius: 8px;"
        "    padding: 8px 15px;"
        "    font-size: 16px;"
        "    color: #00ffff;"
        "    min-width: 250px;"
        "}"
        "#roleComboBox::drop-down {"
        "    border: none;"
        "}"
        "#roleComboBox QAbstractItemView {"
        "    background-color: rgba(20, 20, 40, 240);"
        "    color: #00ffff;"
        "    selection-background-color: #0088ff;"
        "}"
        "#passwordEdit:focus {"
        "    border-color: #00ffff;"
        "}"
        "#passwordHint {"
        "    color: #8888ff;"
        "    font-size: 12px;"
        "    font-style: italic;"
        "}"
        "#loginButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #00c8ff,"
        "        stop:1 #0088ff);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 30px;"
        "    color: white;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    min-width: 150px;"
        "}"
        "#loginButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #00ffff,"
        "        stop:1 #00aaff);"
        "}"
        "#loginButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #0088cc,"
        "        stop:1 #0066aa);"
        "}"
        "#logoutButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #ff5555,"
        "        stop:1 #cc0000);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 30px;"
        "    color: white;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    min-width: 150px;"
        "}"
        "#logoutButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #ff7777,"
        "        stop:1 #ee2222);"
        "}"
        "#logoutButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #cc0000,"
        "        stop:1 #aa0000);"
        "}"
        "#errorLabel {"
        "    color: #ff5555;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    background-color: rgba(255, 50, 50, 0.1);"
        "    padding: 8px 15px;"
        "    border-radius: 5px;"
        "    border: 1px solid #ff5555;"
        "}"
        );

    // 使整个 adminPage 背景透明（仅 container 保持可见样式），并应用样式表
    adminPage->setAttribute(Qt::WA_TranslucentBackground);
    adminPage->setStyleSheet(style);

    // 连接 IP 确认按钮
    connect(ipApplyBtn, &QPushButton::clicked, this, [this, ipHostEdit]() {
        QString text = ipHostEdit->text();
        if (!text.isEmpty()) {
            updateTcpServerHost(text);
            showNotification("WIN7_IP 已更新: 192.168.1." + text);
        }
    });

    // 连接 模拟器 IP 确认按钮
    connect(simApplyBtn, &QPushButton::clicked, this, [this, simHostEdit]() {
        QString text = simHostEdit->text();
        if (!text.isEmpty()) {
            updateSimulatorHost(text);
            showNotification("模拟器 IP 已更新: 192.168.1." + text);
        }
    });

    // 连接登录按钮
    connect(loginButton, &QPushButton::clicked, this, [this, roleComboBox, passwordEdit, errorLabel, titleLabel, loginButton, logoutButton, hintLabel, featureButton, netConfigSection, ipHostEdit, simHostEdit]() {
        QString password = passwordEdit->text();
        UserRole selectedRole = static_cast<UserRole>(roleComboBox->currentData().toInt());
        QString roleName = roleComboBox->currentText();

        // 记录登录尝试
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "权限验证";
        record.controlName = "passwordEdit";
        record.controlType = "LoginAttempt";
        record.operation = "login_attempt";
        record.oldValue = "";
        record.newValue = QString("尝试登录 %1: %2").arg(roleName).arg(password.isEmpty() ? "(空)" : "******");
        m_recorder->addRecord(record);

        // 验证密码
        bool loginSuccess = false;
        if (selectedRole == UserRole::Admin && password == "123") {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Engineer && password == "456") {
            loginSuccess = true;
        } else if (selectedRole == UserRole::Manufacturer && password == "8888") {
            loginSuccess = true;
        }

        if (loginSuccess) {
            // 设置登录状态
            m_currentUserRole = selectedRole;

            // 记录成功登录
            OperationRecord successRecord;
            successRecord.timestamp = QDateTime::currentDateTime();
            successRecord.pageName = "权限验证";
            successRecord.controlName = "loginButton";
            successRecord.controlType = "LoginSuccess";
            successRecord.operation = "login_success";
            successRecord.oldValue = "";
            successRecord.newValue = QString("登录成功: %1").arg(roleName);
            m_recorder->addRecord(successRecord);

            // 更新界面状态
            titleLabel->setText(QString("当前权限: %1").arg(roleName));
            roleComboBox->setVisible(false);
            passwordEdit->setVisible(false);
            hintLabel->setVisible(false);
            loginButton->setVisible(false);
            logoutButton->setVisible(true);
            featureButton->setVisible(m_currentUserRole == UserRole::Manufacturer);
            netConfigSection->setVisible(m_currentUserRole == UserRole::Manufacturer);
            errorLabel->setVisible(false);
            passwordEdit->clear();

            // 登录成功后仅显示提示，不自动跳转到操作记录页面
            showNotification(QString("%1 登录成功").arg(roleName));
        } else {
            errorLabel->setText("密码错误！请重试。");
            errorLabel->setVisible(true);

            // 记录失败登录
            OperationRecord failRecord;
            failRecord.timestamp = QDateTime::currentDateTime();
            failRecord.pageName = "权限验证";
            failRecord.controlName = "loginButton";
            failRecord.controlType = "LoginFail";
            failRecord.operation = "login_fail";
            failRecord.oldValue = "";
            failRecord.newValue = QString("登录失败: %1").arg(password.isEmpty() ? "(空)" : "******");
            m_recorder->addRecord(failRecord);

            // 清空密码框并设置焦点
            passwordEdit->clear();
            passwordEdit->setFocus();
        }
    });

    // 连接注销按钮
    connect(logoutButton, &QPushButton::clicked, this, [this, titleLabel, roleComboBox, passwordEdit, hintLabel, loginButton, logoutButton, errorLabel, featureButton, netConfigSection]() {
        // 记录注销
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "权限验证";
        record.controlName = "logoutButton";
        record.controlType = "Logout";
        record.operation = "logout";
        record.oldValue = "";
        record.newValue = "注销，返回操作员权限";
        m_recorder->addRecord(record);

        // 恢复为操作员权限
        m_currentUserRole = UserRole::Operator;

        // 恢复界面状态
        titleLabel->setText("权限验证");
        roleComboBox->setVisible(true);
        passwordEdit->setVisible(true);
        hintLabel->setVisible(true);
        loginButton->setVisible(true);
        logoutButton->setVisible(false);
        featureButton->setVisible(false);
        netConfigSection->setVisible(false);
        errorLabel->setVisible(false);
        
        showNotification("已注销，当前为操作员权限");
    });

    // 回车键登录
    connect(passwordEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);


    // 当输入时隐藏错误提示
    connect(passwordEdit, &QLineEdit::textChanged, errorLabel, &QLabel::hide);

    // 连接功能开关按钮
    connect(featureButton, &QPushButton::clicked, this, [this]() {
        if (m_currentUserRole == UserRole::Manufacturer) {
            if (!m_featureSwitchWidget) {
                m_featureSwitchWidget = new FeatureSwitchWidget();
            }
            m_featureSwitchWidget->show();
            m_featureSwitchWidget->raise();
            m_featureSwitchWidget->activateWindow();
        }
    });
}
void MainWindow::showNotification(const QString &message)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (message == m_lastNotificationMessage && (nowMs - m_lastNotificationMs) < 800) {
        return;
    }
    m_lastNotificationMessage = message;
    m_lastNotificationMs = nowMs;

    // 在状态栏显示通知
    if (ui && ui->statusBar) {
        ui->statusBar->showMessage(message, 3000);
    }

    // Wayland下避免触发 requestActivate 警告与高频弹框开销
    const QString platform = QGuiApplication::platformName().toLower();
    if (!platform.contains("wayland")) {
        QToolTip::showText(QCursor::pos(), message, nullptr, QRect(), 2000);
    }
}

// 辅助函数：根据记录类型获取颜色
QString MainWindow::getRecordColor(const OperationRecord &record)
{
    if (record.controlType == "TechSliderEdit") {
        return "#a9d4ff";  // 浅蓝色
    } else if (record.controlType == "TechPushButton") {
        return "#ffffff";  // 白色
    } else if (record.controlType == "QToolButton") {
        return "#a9d4ff";  // 浅蓝色
    } else {
        return "#ffffff";  // 默认白色
    }
}

// 辅助函数：检查记录是否应该显示
bool MainWindow::shouldDisplayRecord(const OperationRecord &record, const QString &filter)
{
    if (filter.isEmpty() || filter.contains("显示全部记录")) {
        return true;
    }

    if (filter.contains("滑块操作") && record.controlType == "TechSliderEdit") {
        return true;
    }

    if (filter.contains("按钮操作") && record.controlType == "TechPushButton") {
        return true;
    }

    if (filter.contains("工具按钮") && record.controlType == "QToolButton") {
        return true;
    }

    if (filter.contains("登录记录") &&
        (record.controlType.contains("Login") || record.operation.contains("login"))) {
        return true;
    }

    // 检查页面筛选
    for (int i = 0; i < 5; i++) {
        if (m_pageNames.contains(i) && filter.contains(m_pageNames[i])) {
            return record.pageName == m_pageNames[i];
        }
    }

    return false;
}

// 辅助函数：应用样式表
void MainWindow::applyRecordPageStyle(QWidget *recordPage)
{
    QString style = QString(
        "#recordPage {"
        "    background-color: #1a5fb4;"  // 蓝色背景
        "    border: none;"  // 去掉边框
        "}"

        "#recordTitleWidget {"
        "    background: rgba(30, 60, 120, 0.7);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    padding: 10px;"
        "    margin-bottom: 5px;"
        "    border: 1px solid #2d7fda;"
        "}"

        "#recordTitle {"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "    color: #ffffff;"  // 白色文字
        "}"

        "#timeLabel {"
        "    color: #a9d4ff;"  // 浅蓝色文字
        "    font-size: 14px;"
        "}"

        "#controlPanel {"
        "    background: rgba(30, 60, 120, 0.8);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "    padding: 5px;"
        "    margin-bottom: 5px;"
        "}"

        // 按钮样式
        "QPushButton {"
        "    background-color: #2d7fda;"  // 蓝色
        "    color: #ffffff;"  // 白色文字
        "    border: 1px solid #4a9eff;"
        "    border-radius: 4px;"
        "    padding: 6px 12px;"
        "    font-size: 12px;"
        "    min-width: 80px;"
        "}"

        "QPushButton:hover {"
        "    background-color: #4a9eff;"  // 浅蓝色
        "    border-color: #7fbfff;"
        "}"

        "QPushButton:pressed {"
        "    background-color: #1a5fb4;"  // 深蓝色
        "}"

        // 下拉框样式
        "QComboBox {"
        "    background-color: #2d7fda;"  // 蓝色
        "    color: #ffffff;"  // 白色文字
        "    border: 1px solid #4a9eff;"
        "    border-radius: 4px;"
        "    padding: 4px 8px;"
        "    min-width: 120px;"
        "}"

        "QComboBox:hover {"
        "    border-color: #7fbfff;"
        "}"

        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"

        "QComboBox::down-arrow {"
        "    width: 12px;"
        "    height: 12px;"
        "    border-left: 6px solid transparent;"
        "    border-right: 6px solid transparent;"
        "    border-top: 6px solid #ffffff;"
        "}"

        "QComboBox QAbstractItemView {"
        "    background-color: #2d7fda;"
        "    color: #ffffff;"
        "    selection-background-color: #4a9eff;"
        "    border: 1px solid #4a9eff;"
        "}"

        // 标签样式
        "QLabel {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 12px;"
        "}"

        // 记录显示区域
        "#recordDisplay {"
        "    background: rgba(30, 60, 120, 0.6);"  // 半透明深蓝色
        "    color: #ffffff;"  // 白色文字
        "    border: 1px solid #2d7fda;"
        "    border-radius: 8px;"
        "    font-family: 'Segoe UI', Arial, sans-serif;"
        "    font-size: 12px;"
        "    padding: 10px;"
        "    selection-background-color: #4a9eff;"  // 选中文字背景
        "}"

        "#statsPanel {"
        "    background: rgba(30, 60, 120, 0.8);"  // 半透明深蓝色
        "    border-radius: 8px;"
        "    border: 1px solid #2d7fda;"
        "    padding: 5px;"
        "    margin-top: 5px;"
        "}"

        // 统计标签样式
        "#totalStats, #todayStats, #sliderStats, #buttonStats, #toolButtonStats {"
        "    color: #ffffff;"  // 白色文字
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    padding: 4px 8px;"
        "    background: rgba(45, 127, 218, 0.5);"  // 半透明蓝色
        "    border-radius: 4px;"
        "    border: 1px solid #4a9eff;"
        "}"
        );

    recordPage->setStyleSheet(style);
}

// 辅助函数：根据控件类型获取图标
QString MainWindow::getControlIcon(const QString &controlType)
{
    Q_UNUSED(controlType);
    // if (controlType == "TechSliderEdit") return "🎚️";
    // if (controlType == "TechPushButton") return "🔄";
    // if (controlType == "QToolButton") return "🔧";
    // if (controlType == "QLineEdit") return "⌨️";
    // if (controlType.contains("Login")) return "🔐";
    return "";
}

// mainwindow.cpp 中添加以下函数

// 获取TechSliderLabel的值
// 获取TechSliderLabel的值
double MainWindow::getSliderLabelValue(const QString &labelName)
{
    // 从首页查找对应的label
    if (m_sliderLabelInstances.contains(labelName)) {
        return m_sliderLabelInstances[labelName]->value();  // 修改这里：使用 value() 而不是 currentValue()
    }

    // 如果没有找到，尝试在首页中查找
    QWidget* mainPage = ui->StackedWidget->widget(0);
    if (mainPage) {
        TechSliderLabel* label = mainPage->findChild<TechSliderLabel*>(labelName);
        if (label) {
            return label->value();  // 修改这里：使用 value() 而不是 currentValue()
        }
    }

    return 0.0;
}

// 获取TechSliderEdit的值
double MainWindow::getSliderEditValue(const QString &sliderName)
{
    TechSliderEdit* slider = findChild<TechSliderEdit*>(sliderName);
    if (slider) {
        return slider->value();
    }
    return 0.0;
}

// 记录回转升降页面的操作
void MainWindow::recordVerticalSupportAction(int keyNumber, bool pressed)
{
    QString pageName = "回转升降";

    // 获取当前高度（label_Value2）
    double currentHeight = getSliderLabelValue("label_Value2");
    // 获取移动速度
    double moveSpeed = getSliderEditValue("TechSliderEdit_VeSupSec_MoveSpeed");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = QString("按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "pressed" : "released";
    record.oldValue = "";

    if (keyNumber == 1) {  // ○1 升降下降
        if (pressed) {
            record.newValue = QString("升降组件当前高度为%1mm，当前以%2mm/s速度下降")
                                  .arg(currentHeight, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("升降组件当前高度为%1mm，下降完成")
                                  .arg(currentHeight, 0, 'f', 1);
        }
    } else if (keyNumber == 2) {  // ○2 升降上升
        if (pressed) {
            record.newValue = QString("升降组件当前高度为%1mm，当前以%2mm/s速度上升")
                                  .arg(currentHeight, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("升降组件当前高度为%1mm，上升完成")
                                  .arg(currentHeight, 0, 'f', 1);
        }
    }

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

// 记录伸缩臂页面的操作
void MainWindow::recordHorizontalSupportAction(int keyNumber, bool pressed)
{
    QString pageName = "伸缩臂";

    // 获取当前角度（label_Value1）
    double currentAngle = getSliderLabelValue("label_Value1");
    // 获取旋转速度
    double rotationSpeed = getSliderEditValue("TechSliderEdit_HoriSupSec_RotationSpeed");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = QString("按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "pressed" : "released";
    record.oldValue = "";

    if (keyNumber == 3) {  // ○3 负方向旋转
        if (pressed) {
            record.newValue = QString("悬臂组件当前角度为%1°，当前以%2°/s速度负方向旋转")
                                  .arg(currentAngle, 0, 'f', 1)
                                  .arg(rotationSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前角度为%1°，旋转完成")
                                  .arg(currentAngle, 0, 'f', 1);
        }
    } else if (keyNumber == 4) {  // ○4 正方向旋转
        if (pressed) {
            record.newValue = QString("悬臂组件当前角度为%1°，当前以%2°/s速度正方向旋转")
                                  .arg(currentAngle, 0, 'f', 1)
                                  .arg(rotationSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前角度为%1°，旋转完成")
                                  .arg(currentAngle, 0, 'f', 1);
        }
    }

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

void MainWindow::recordHorizontalSupportMoveAction(int keyNumber, bool pressed)
{
    QString pageName = "伸缩臂";

    // 获取当前长度（label_Value3）
    double currentLength = getSliderLabelValue("label_Value3");
    // 获取移动速度
    double moveSpeed = getSliderEditValue("TechSliderEdit_HoriSupSec_MoveSpeed");

    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = QString("按键○%1").arg(keyNumber);
    record.controlType = "MatrixKey";
    record.operation = pressed ? "pressed" : "released";
    record.oldValue = "";

    if (keyNumber == 1) {  // ○1 缩短
        if (pressed) {
            record.newValue = QString("悬臂组件当前长度为%1mm，当前以%2mm/s速度缩短")
                                  .arg(currentLength, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前长度为%1mm，缩短完成")
                                  .arg(currentLength, 0, 'f', 1);
        }
    } else if (keyNumber == 2) {  // ○2 伸长
        if (pressed) {
            record.newValue = QString("悬臂组件当前长度为%1mm，当前以%2mm/s速度伸长")
                                  .arg(currentLength, 0, 'f', 1)
                                  .arg(moveSpeed, 0, 'f', 1);
        } else {
            record.newValue = QString("悬臂组件当前长度为%1mm，伸长完成")
                                  .arg(currentLength, 0, 'f', 1);
        }
    }

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

//连接电池
//矩阵键盘
void MainWindow::onMatrixKeyPressed(int keyNumber, bool pressed)
{
    qDebug() << "【主线程】按键信号接收 - 按键:" << keyNumber
             << "状态:" << (pressed ? "按下" : "释放");

    QString action = pressed ? "按下" : "释放";
    QString message = QString("矩阵按键 ○%1 被%2").arg(keyNumber).arg(action);
    ui->statusBar->showMessage(message, 2000);

    // 处理按键操作
    handleMatrixKeyAction(keyNumber, pressed);
}

// 在 handleMatrixKeyAction 函数中修改 ○1 按键的处理
void MainWindow::handleMatrixKeyAction(int keyNumber, bool pressed)
{
    // 获取当前页面
    int currentPage = ui->StackedWidget->currentIndex();
    QString pageName = m_pageNames.value(currentPage, "未知");

    // 特殊处理：如果是机械臂页面（索引为0），执行唯一的 500/514 寄存器逻辑并直接返回
    if (currentPage == 0 || pageName == "机械臂" || pageName == "page_Robot") {
        // 新增条件：只有当 TBtn_Stepmove 处于“点动模式”且 TBtn_MoveMode 处于“关节模式”时才执行
        if (!m_stepModeEnabled && m_isJointMode) {
            if (keyNumber >= 1 && keyNumber <= 8) {
                int value500 = 0;
                int value514 = 0;
                if (pressed) {
                    // 地址500逻辑：○1,○2->1; ○3,○4->2; ○5,○6->3; ○7,○8->4
                    value500 = (keyNumber + 1) / 2;

                    // 地址514逻辑：奇数按键写4，偶数按键写2
                    value514 = (keyNumber % 2 != 0) ? 4 : 2;

                    writeToMainDevice(500, value500);
                    writeToMainDevice(514, value514);
                } else {
                    // 松开时的逻辑：500寄存器不再写0，514寄存器写0
                    value500 = -1; // 用-1表示不操作
                    value514 = 0;
                    writeToMainDevice(514, 0);
                }

                qDebug() << "page_Robot (Index 0, 点动/关节) 按键 ○" << keyNumber << " " << (pressed ? "按下" : "释放")
                         << " -> 地址500写入:" << (pressed ? QString::number(value500) : "保持") 
                         << ", 地址514写入:" << value514;
            }
        } else {
            qDebug() << "机械臂页面按键忽略：当前未处于[点动+关节]模式";
        }
        return; // 机械臂页面不再执行后续逻辑
    }

    // 处理124地址的写入
    if (keyNumber >= 1 && keyNumber <= 4) {
        int value = getValueFor124Address(keyNumber, pressed);

        if (value != 0 || !pressed) {  // 按下时写入特定值，释放时写入0
            writeToMainDevice(124, value);
            qDebug() << "" << keyNumber << (pressed ? "按下" : "释放")
                     << "，地址124写入:" << value;
        }
    }

    // 处理AGV页面的特殊逻辑（原有逻辑保持不变）
    if (keyNumber == 1 || keyNumber == 2) {
        if (pageName == "AGV控制" || pageName == "page_AGV") {
            if (keyNumber == 1) {
                handleAGVKeyAction(keyNumber, pressed);
            } else if (keyNumber == 2) {
                handleAGVKey2Action(keyNumber, pressed);
            }
            return;
        }
    }

    // 新增：记录回转升降页面的操作
    if (pageName == "回转升降") {
        if (keyNumber == 1 || keyNumber == 2) {
            recordVerticalSupportAction(keyNumber, pressed);
        }
    }

    // 新增：记录伸缩臂页面的操作
    if (pageName == "伸缩臂") {
        if (keyNumber == 1 || keyNumber == 2) {
            // 新增：处理伸缩臂的移动操作（缩短/伸长）
            recordHorizontalSupportMoveAction(keyNumber, pressed);
        } else if (keyNumber == 3 || keyNumber == 4) {
            // 原有：处理伸缩臂的旋转操作
            recordHorizontalSupportAction(keyNumber, pressed);
        }
    }

    // 新增：记录EOAT控制页面的操作
    if (pageName == "EOAT控制") {
        // 获取当前角度（label_Value4）
        double currentAngle = getSliderLabelValue("label_Value4");
        // 获取旋转速度
        double rotationSpeed = getSliderEditValue("TechSliderEdit_EOAT_RotationSpeed");

        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = pageName;
        record.controlName = QString("按键○%1").arg(keyNumber);
        record.controlType = "MatrixKey";
        record.operation = pressed ? "pressed" : "released";
        record.oldValue = "";

        if (keyNumber == 1) {  // ○1 柔顺组件负方向旋转
            if (pressed) {
                record.newValue = QString("柔顺组件当前角度为%1°，当前以%2°/s速度负方向旋转")
                                      .arg(currentAngle, 0, 'f', 1)
                                      .arg(rotationSpeed, 0, 'f', 1);
            } else {
                record.newValue = QString("柔顺组件当前角度为%1°，旋转完成")
                                      .arg(currentAngle, 0, 'f', 1);
            }
        } else if (keyNumber == 2) {  // ○2 柔顺组件正方向旋转
            if (pressed) {
                record.newValue = QString("柔顺组件当前角度为%1°，当前以%2°/s速度正方向旋转")
                                      .arg(currentAngle, 0, 'f', 1)
                                      .arg(rotationSpeed, 0, 'f', 1);
            } else {
                record.newValue = QString("柔顺组件当前角度为%1°，旋转完成")
                                      .arg(currentAngle, 0, 'f', 1);
            }
        }

        if (keyNumber == 1 || keyNumber == 2) {
            m_recorder->addRecord(record);
            showNotification(record.newValue.toString());
        }
    }

    // 原有的其他按键处理逻辑保持不变
    switch (keyNumber) {
    case 9: // 按键○9
        if (pressed) {
            ui->StackedWidget->setCurrentIndex(0);
        }
        break;

    case 10: // 按键○10
        if (pressed) {
            ui->StackedWidget->setCurrentIndex(1);
        }
        break;

    default:
        break;
    }
}
// 新增：处理AGV页面的按键○1动作
void MainWindow::handleAGVKeyAction(int keyNumber, bool pressed)
{
    if (keyNumber == 1) {
        if (pressed) {
            // 按键○1按下
            if (m_agvOaEnabled) {
                // 避障开启时，给0地址写200
                writeToAGVDevice(0, 72);
                qDebug() << "按键○1按下，避障开启，地址0写入200";
                ui->statusBar->showMessage("AGV前进（避障开启）", 2000);
            } else {
                // 避障关闭时，给0地址写202
                writeToAGVDevice(0, 74);
                qDebug() << "按键○1按下，避障关闭，地址0写入202";
                ui->statusBar->showMessage("AGV前进（避障关闭）", 2000);
            }
        } else {
            // 按键○1释放
            if (m_agvOaEnabled) {
                // 避障开启时，给0地址写192
                writeToAGVDevice(0, 64);

                qDebug() << "按键○1释放，避障开启，地址0写入192";
                ui->statusBar->showMessage("AGV停止（避障开启）", 2000);
            } else {
                // 避障关闭时，给0地址写194
                writeToAGVDevice(0, 66);
                qDebug() << "按键○1释放，避障关闭，地址0写入194";
                ui->statusBar->showMessage("AGV停止（避障关闭）", 2000);
            }
        }

        // 记录操作
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "AGV控制";
        record.controlName = QString("按键○%1").arg(keyNumber);
        record.controlType = "MatrixKey";
        record.operation = pressed ? "pressed" : "released";
        record.oldValue = "";
        record.newValue = pressed ?
                              (m_agvOaEnabled ? "前进（避障开启）" : "前进（避障关闭）") :
                              (m_agvOaEnabled ? "停止（避障开启）" : "停止（避障关闭）");
        m_recorder->addRecord(record);
    }
}

int MainWindow::getValueFor124Address(int keyNumber, bool pressed)
{
    if (!pressed) {
        // 释放时统一写0
        return 0;
    }

    // 获取当前页面
    int currentPage = ui->StackedWidget->currentIndex();
    QString pageName = m_pageNames.value(currentPage, "未知");

    // 根据按键和页面确定值
    switch (keyNumber) {
    case 1: // 按键○1
        if (pageName == "回转升降") {  // page_VeSupSec
            return -2;
        } else if (pageName == "伸缩臂") {  // page_HoriSupSec
            return -3;
        } else if (pageName == "EOAT控制") {  // page_EOAT
            return -4;
        }
        break;

    case 2: // 按键○2
        if (pageName == "回转升降") {  // page_VeSupSec
            return 2;
        } else if (pageName == "伸缩臂") {  // page_HoriSupSec
            return 3;
        } else if (pageName == "EOAT控制") {  // page_EOAT
            return 4;
        }
        break;

    case 3: // 按键○3
        if (pageName == "伸缩臂") {  // page_HoriSupSec
            return -1;
        }
        break;

    case 4: // 按键○4
        if (pageName == "伸缩臂") {  // page_HoriSupSec
            return 1;
        }
        break;
    }

    // 默认返回0
    return 0;
}


void MainWindow::setupKeyManager()
{
    if (!isFeatureEnabled("input_devices", "input.matrix_key")) {
        qDebug() << "矩阵键输入功能已关闭，跳过键盘管理器";
        return;
    }

    qDebug() << "=== 设置键盘管理器 ===";
    qDebug() << "当前线程:" << QThread::currentThread();

/**
 * @brief 设置 Modbus 线程管理器并完成基本连接配置
 *
 * 包括：获取单例、连接其信号到本窗口槽函数、设置轮询间隔与自动重连。
 * 该函数不会直接处理具体寄存器映射，映射通过 `setupSliderModbusAddresses` 等函数完成。
 */
    // 1. 连接键盘按下信号
    connect(m_keyManager, &MatrixKeyThreadManager::keyPressed,
            this, &MainWindow::onMatrixKeyPressed);

    qDebug() << "信号连接完成";

    // 2. 启动键盘监控
    if (m_keyManager->start("/dev/input/event0")) {
        QString threadInfo;
        QThread* workerThread = m_keyManager->workerThread();
        if (workerThread) {
            QString threadId = QString::number((quintptr)workerThread, 16);
            threadInfo = QString(" [线程:0x%1]").arg(threadId);
        }

        ui->statusBar->showMessage(QString("矩阵按键监控已启动%1").arg(threadInfo), 3000);
        qDebug() << "键盘管理器启动成功";
        qDebug() << "工作线程:" << workerThread;
        qDebug() << "管理器线程:" << m_keyManager->thread();
    } else {
        ui->statusBar->showMessage("错误：无法启动矩阵按键监控！", 5000);
        qWarning() << "键盘管理器启动失败";
    }

    // 删除对 setupThreadMonitorUI() 和 updateThreadStatus() 的调用
}

//modbus TCP
void MainWindow::setupModbusManager()
{
    if (!isBigFeatureEnabled("modbus_main")) {
        qDebug() << "主控Modbus功能已关闭，跳过Modbus管理器";
        return;
    }

    // 获取Modbus管理器实例
    m_modbusManager = ModbusThreadManager::instance();

    qDebug() << "设置Modbus管理器，管理器地址:" << m_modbusManager;

    // 连接信号到槽函数
    connect(m_modbusManager, &ModbusThreadManager::connected,
            this, &MainWindow::onModbusConnected);
    connect(m_modbusManager, &ModbusThreadManager::disconnected,
            this, &MainWindow::onModbusDisconnected);
    connect(m_modbusManager, &ModbusThreadManager::errorOccurred,
            this, &MainWindow::onModbusError);
    connect(m_modbusManager, &ModbusThreadManager::registerValueChanged,
            this, &MainWindow::onModbusRegisterValueChanged,
            Qt::QueuedConnection);

    qDebug() << "Modbus信号连接完成";

    const MainModbusEndpoint endpoint = MainModbusConnector::selectEndpoint(
        isFeatureEnabled("tcp_transmission", "tcp.local_simulator"),
        isFeatureEnabled("tcp_transmission", "tcp.remote_simulator"));
    if (endpoint.host == "127.0.0.1") {
        qDebug() << "启用本机 TCP 模拟器模式：主设备 ->" << endpoint.host << ":" << endpoint.port;
    } else if (endpoint.port == 5020) {
        qDebug() << "启用远程 TCP 模拟器模式：主设备 ->" << endpoint.host << ":" << endpoint.port;
    }

    bool deviceConnected = MainModbusConnector::connectAndConfigure(
        m_modbusManager, endpoint, m_mainModbusPollIntervalMs, m_mainReconnectIntervalMs);
    qDebug() << "192.168.1.88 Modbus连接状态:" << (deviceConnected ? "已连接" : "连接失败");

    // 显示连接状态消息
    ui->statusBar->showMessage("正在连接192.168.1.88 Modbus...", 3000);

    qDebug() << "192.168.1.88 Modbus管理器设置完成";
}
// 修改setupSliderModbusAddresses函数，添加对TechSliderLabel的支持
void MainWindow::setupSliderModbusAddresses()
{
    // 为每个TechSliderEdit配置Modbus地址
    QList<TechSliderEdit*> allSliders = this->findChildren<TechSliderEdit*>();
    int baseAddress = 0;

    for (int i = 0; i < allSliders.size(); ++i) {
        TechSliderEdit *slider = allSliders[i];
        QString sliderName = slider->objectName();
        int modbusAddress = baseAddress + i;

        m_modbusManager->registerSlider(slider, modbusAddress);

        qDebug() << "注册slider到Modbus:" << sliderName
                 << "地址:" << modbusAddress << "(对应400"
                 << QString("%1").arg(modbusAddress + 40001, 3, 10, QChar('0')) << ")";
    }

    // 为每个TechSliderLabel配置Modbus地址
    QList<TechSliderLabel*> allSliderLabels = this->findChildren<TechSliderLabel*>();
    int labelBaseAddress = 100;  // 使用不同的地址范围

    for (int i = 0; i < allSliderLabels.size(); ++i) {
        TechSliderLabel *sliderLabel = allSliderLabels[i];
        QString sliderLabelName = sliderLabel->objectName();
        int modbusAddress = labelBaseAddress + i;

        m_modbusManager->registerSliderLabel(sliderLabel, modbusAddress);

        qDebug() << "注册sliderLabel到Modbus:" << sliderLabelName
                 << "地址:" << modbusAddress << "(对应400"
                 << QString("%1").arg(modbusAddress + 40001, 3, 10, QChar('0')) << ")";
    }
}
// Modbus连接成功槽函数
void MainWindow::onModbusConnected()
{
    qDebug() << "Modbus连接成功，启动交互任务...";
    MainModbusStatus::applyUiState(ui ? ui->statusBar : nullptr, MainModbusState::Connected);

    // 立即启动原本推迟的数据读取子系统
    if (isFeatureEnabled("startup_checks", "startup.write_registers")) {
        performStartupWrites();
    }

    if (isFeatureEnabled("modbus_main", "modbus_main.float_reading")) {
        setupModbusFloatReading();
    }

    if (isBigFeatureEnabled("tcp_transmission")) {
        enableTcpTransmission(true);
    }

    MainModbusStatus::appendOperationRecord(m_recorder, MainModbusState::Connected);
}

// Modbus断开连接槽函数
void MainWindow::onModbusDisconnected()
{
    qDebug() << "Modbus设备断开连接";
    MainModbusStatus::applyUiState(ui ? ui->statusBar : nullptr, MainModbusState::Disconnected);
    MainModbusStatus::appendOperationRecord(m_recorder, MainModbusState::Disconnected);
}

// Modbus错误槽函数
void MainWindow::onModbusError(const QString &error)
{
    qDebug() << "Modbus错误:" << error;
    MainModbusStatus::applyUiState(ui ? ui->statusBar : nullptr, MainModbusState::Error, error);
    MainModbusStatus::appendOperationRecord(m_recorder, MainModbusState::Error, error);
}
// 速度选择
void MainWindow::initSpeedModeSelector()
{
    if (!isFeatureEnabled("motion_control", "motion.speed_mode")) {
        qDebug() << "速度模式功能已关闭，跳过初始化";
        return;
    }

    // 创建速度模式选择器
    SpeedModeSelector *speedModeSelector = new SpeedModeSelector(this);
    speedModeSelector->setObjectName("speedModeSelector");

    // 设置位置和大小（根据你的UI布局调整）
    // 例如，放在某个布局或widget中
    // ui->verticalLayout->addWidget(speedModeSelector);

    // 设置按钮样式为全息风格
    speedModeSelector->setButtonStyle(TechPushButton::StyleHolographic);

    // 设置自定义颜色
    speedModeSelector->setActiveColor(QColor(0, 200, 255));     // 激活状态颜色
    speedModeSelector->setInactiveColor(QColor(80, 80, 100));   // 非激活状态颜色
    speedModeSelector->setTextColor(Qt::white);                 // 文字颜色

    // 连接模式改变信号
    connect(speedModeSelector, &SpeedModeSelector::modeChanged,
            this, [this](SpeedMode mode) {
                qDebug() << "速度模式改变为:" << mode;

                // 根据模式更新其他UI或执行操作
                switch(mode) {
                case MODE_LOW:
                    // 低速模式操作
                    updateSpeed(50.0);  // 更新仪表盘
                    showNotification("已切换至低速模式");
                    break;
                case MODE_MEDIUM:
                    // 中速模式操作
                    updateSpeed(100.0);
                    showNotification("已切换至中速模式");
                    break;
                case MODE_HIGH:
                    // 高速模式操作
                    updateSpeed(150.0);
                    showNotification("已切换至高速模式");
                    break;
                }

                // 记录操作
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "当前页面";  // 根据实际情况设置
                record.controlName = "speedModeSelector";
                record.controlType = "SpeedModeSelector";
                record.operation = "mode_changed";
                record.oldValue = "";  // 可以记录旧模式
                record.newValue = QString("模式%1").arg(mode);
                m_recorder->addRecord(record);
            });
}
//配置AGV的modbus


// 初始化Modbus变量表
void MainWindow::setupModbusVariables()
{
    if (!m_modbusVariables) {
        m_modbusVariables = new ModbusVariables(this);
    }

    // 如果需要从Excel文件加载
    // m_modbusVariables->loadFromExcel("上位机交互变量表(1).xls");
}

// 设置Modbus地址到Label的映射
void MainWindow::setupModbusLabels()
{
    m_modbusLabels = MainModbusLabelMapper::buildMap(this);

    qDebug() << "找到" << m_modbusLabels.size() << "个Modbus显示Label";
}

// 启动Modbus变量轮询
void MainWindow::startModbusPolling()
{
    if (MainModbusPoller::shouldSkipStart(m_modbusPollTimer)) {
        qDebug() << "主设备通用地址轮询已停用，仅保留四个SliderLabel地址轮询";
        return;
    }

    if (!isFeatureEnabled("modbus_main", "modbus_main.polling")) {
        qDebug() << "主控Modbus轮询功能已关闭";
        return;
    }

    if (!m_modbusManager || !m_modbusManager->isConnected()) {
        qDebug() << "Modbus未连接，无法启动轮询";
        return;
    }

    // 获取所有需要读取的变量
    QList<ModbusVariable> variables = m_modbusVariables->getAllVariables();

    // 添加所有变量到轮询列表
    for (const ModbusVariable &var : variables) {
        int modbusAddress = 0;
        int bitPos = -1;

        if (ModbusVariables::parseAddress(var.address, modbusAddress, bitPos)) {
            // 添加到轮询（这里需要扩展ModbusThreadManager）
            // 暂时先记录需要读取的地址
            qDebug() << "需要读取变量:" << var.name
                     << "地址:" << var.address
                     << "Modbus地址:" << modbusAddress;
        }
    }

    // 启动定时器轮询
    connect(m_modbusPollTimer, &QTimer::timeout, this, &MainWindow::pollModbusVariables);
    m_modbusPollTimer->start(m_mainUiPollIntervalMs);
}

// 轮询Modbus变量
void MainWindow::pollModbusVariables()
{
    if (MainModbusPoller::shouldSkipPoll()) {
        qDebug() << "pollModbusVariables已停用，避免轮询main其他地址";
        return;
    }
    static int currentIndex = 0;
    Q_UNUSED(MainModbusPoller::pollNextVariable(m_modbusManager, m_modbusVariables, currentIndex));
    return;
}

static QMap<int, quint16> g_registerCache; 

void MainWindow::updateSliderLabelValue(const QString& labelName, float value)
{
    // 根据首页控件名，更新所有相关页面的对应控件
    for (auto it = m_pageSliders.begin(); it != m_pageSliders.end(); ++it) {
        const QVector<TechSliderLabel*>& sliders = it.value();

        for (TechSliderLabel* slider : sliders) {
            if (slider) {
                QString objName = slider->objectName();
                // 检查是否是首页控件或相关副本
                if (objName == labelName || objName.startsWith(labelName + "_")) {
                    slider->setValue(value);
                    // 移除高频日志: qDebug() << "更新" << objName << " = " << value;
                }
            }
        }
    }

    // 更新对应的环形仪表 (TechArcGauge)
    if (m_arcGauges.contains(labelName)) {
        m_arcGauges[labelName]->setValue(static_cast<double>(value));
        
        // 特殊逻辑：根据不同仪表解析对应的速度寄存器
        if (labelName == "robot_ArcGauge_J3Length") {
            // J3 速度 = (48-51) 的 double + (52-55) 的 double, 范围 0~40
            const int v1_addr[4] = {48, 49, 50, 51};
            const int v2_addr[4] = {52, 53, 54, 55};
            if (g_registerCache.contains(48) && g_registerCache.contains(51) && 
                g_registerCache.contains(52) && g_registerCache.contains(55)) {
                double v1 = registersToDoubleDCBAFEHG(g_registerCache[48], g_registerCache[49], g_registerCache[50], g_registerCache[51]);
                double v2 = registersToDoubleDCBAFEHG(g_registerCache[52], g_registerCache[53], g_registerCache[54], g_registerCache[55]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v1 + v2));
            }
        } else if (labelName == "robot_ArcGauge_J1Angle") {
            // J1 速度 = 36-39, 范围 0~2
            if (g_registerCache.contains(36) && g_registerCache.contains(39)) {
                double v = registersToDoubleDCBAFEHG(g_registerCache[36], g_registerCache[37], g_registerCache[38], g_registerCache[39]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v));
            }
        } else if (labelName == "robot_ArcGauge_J2Height") {
            // J2 速度 = 40-43, 范围 0~15
            if (g_registerCache.contains(40) && g_registerCache.contains(43)) {
                double v = registersToDoubleDCBAFEHG(g_registerCache[40], g_registerCache[41], g_registerCache[42], g_registerCache[43]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v));
            }
        } else if (labelName == "robot_ArcGauge_J4Angle") {
            // J4 速度 = 56-59, 范围 0~2
            if (g_registerCache.contains(56) && g_registerCache.contains(59)) {
                double v = registersToDoubleDCBAFEHG(g_registerCache[56], g_registerCache[57], g_registerCache[58], g_registerCache[59]);
                m_arcGauges[labelName]->setSecondValue(qAbs(v));
            }
        }
    }
}

// 处理Modbus值变化
// 在mainwindow.cpp中添加这个函数
// 处理Modbus值变化

void MainWindow::onModbusRegisterValueChanged(int address, quint16 value)
{
    // [调试] 无论如何都会输出，用来确认数据到底回来没
    if (address < 25) { 
        // qWarning() << "[Modbus原始数据] 地址:" << address << "值:" << value;
    }

    // 更新寄存器缓存
    g_registerCache[address] = value;

    const QStringList targetLabels = {
        "robot_ArcGauge_J1Angle", "robot_ArcGauge_J2Height", "robot_ArcGauge_J3Length", "robot_ArcGauge_J4Angle"
    };

    for (const QString &labelName : targetLabels) {
        if (!m_sliderLabelConfigs.contains(labelName)) {
            continue;
        }

        const SliderLabelConfig &config = m_sliderLabelConfigs[labelName];
        const QVector<int> regs = {
            config.modbusAddress1,
            config.modbusAddress2,
            config.modbusAddress3,
            config.modbusAddress4
        };

        if (!regs.contains(address)) {
            continue;
        }

        bool ready = true;
        for (int reg : regs) {
            if (!g_registerCache.contains(reg)) {
                ready = false;
                break;
            }
        }

        if (!ready) {
            continue;
        }

        const quint16 reg1 = g_registerCache[config.modbusAddress1];
        const quint16 reg2 = g_registerCache[config.modbusAddress2];
        const quint16 reg3 = g_registerCache[config.modbusAddress3];
        const quint16 reg4 = g_registerCache[config.modbusAddress4];
        double value64 = registersToDoubleDCBAFEHG(reg1, reg2, reg3, reg4);

        // 如果开启了求和模式 (用于 J3Length = 12-15 + 16-19)
        if (config.isSumMode) {
            bool sumReady = true;
            for (int i = 0; i < 4; ++i) {
                if (!g_registerCache.contains(config.sumAddress[i])) {
                    sumReady = false;
                    break;
                }
            }

            if (sumReady) {
                const double sumPart = registersToDoubleDCBAFEHG(
                    g_registerCache[config.sumAddress[0]],
                    g_registerCache[config.sumAddress[1]],
                    g_registerCache[config.sumAddress[2]],
                    g_registerCache[config.sumAddress[3]]
                );
                value64 += sumPart;
            }
        }

        static QMap<QString, int> debugCounter;
        if (debugCounter[labelName]++ % 8 == 0) {
            // qWarning() << "[四控件读数]" << labelName
            //          << (config.isSumMode ? " (求和模式)" : "")
            //          << "解析值:" << value64;
        }

        updateSliderLabelValue(labelName, static_cast<float>(value64));
    }
    // ============ 新增：检测报警地址 ============
    if (address == 804) {
        // 立柱急停地址
        bool emergencyStop = (value == 1);
        if (emergencyStop != m_emergencyStopColumnFlag) {
            
            // 记录历史
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "立柱急停(804)";
                record.controlType = "报警监控";
                record.operation = emergencyStop ? "报警触发" : "报警恢复";
                record.oldValue = m_emergencyStopColumnFlag ? "触发" : "正常";
                record.newValue = emergencyStop ? "触发" : "正常";
                m_recorder->addRecord(record);
            }

            m_emergencyStopColumnFlag = emergencyStop;
            qDebug() << "立柱急停状态变化:" << (emergencyStop ? "触发" : "解除");

            // 立即检查报警条件
            QTimer::singleShot(0, this, &MainWindow::checkAlarmConditions);
        }
    } else if (address == 805) {
        // 底盘急停地址
        bool emergencyStop = (value == 1);
        if (emergencyStop != m_emergencyStopChassisFlag) {

            // 记录历史
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "底盘急停(805)";
                record.controlType = "报警监控";
                record.operation = emergencyStop ? "报警触发" : "报警恢复";
                record.oldValue = m_emergencyStopChassisFlag ? "触发" : "正常";
                record.newValue = emergencyStop ? "触发" : "正常";
                m_recorder->addRecord(record);
            }

            m_emergencyStopChassisFlag = emergencyStop;
            qDebug() << "底盘急停状态变化:" << (emergencyStop ? "触发" : "解除");

            // 立即检查报警条件
            QTimer::singleShot(0, this, &MainWindow::checkAlarmConditions);
        }
    } else if (address == 403) {
        // 力控超限地址
        bool forceLimit = (value == 1);
        if (forceLimit != m_forceLimitFlag) {

            // 记录历史
            if (m_recorder) {
                OperationRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.pageName = "报警系统";
                record.controlName = "力控超限(403)";
                record.controlType = "报警监控";
                record.operation = forceLimit ? "报警触发" : "报警恢复";
                record.oldValue = m_forceLimitFlag ? "触发" : "正常";
                record.newValue = forceLimit ? "触发" : "正常";
                m_recorder->addRecord(record);
            }

            m_forceLimitFlag = forceLimit;
            qDebug() << "力控超限状态变化:" << (forceLimit ? "触发" : "解除");

            // 新增：力控限位报警以后，自动执行一次力控关闭
            if (forceLimit && m_forcecontrolMode) {
                qDebug() << "检测到力控超限报警且力控处于开启状态，自动执行关力控";
                toggleForceControl();
            }

            // 立即检查报警条件
            QTimer::singleShot(0, this, &MainWindow::checkAlarmConditions);
        }
    }



    // ============ 新增：处理大六维力寄存器（612-623） ============
    if (address >= 612 && address <= 623) {
        // 调试输出：确认收到大六维力数据
         qDebug() << "收到大六维力寄存器数据 - 地址:" << address << "值:" << value;
        
        // 定义地址到大六维力标签的映射
        static QMap<int, QString> bigForceAddressToLabelMap = {
            {612, "FX"}, {613, "FX"},
            {614, "FY"}, {615, "FY"},
            {616, "FZ"}, {617, "FZ"},
            {618, "MX"}, {619, "MX"},
            {620, "MY"}, {621, "MY"},
            {622, "MZ"}, {623, "MZ"}
        };

        if (bigForceAddressToLabelMap.contains(address)) {
            QString labelName = bigForceAddressToLabelMap[address];

            // 获取对应的寄存器对地址
            int highAddr, lowAddr;
            if (labelName == "FX") {
                highAddr = 612; lowAddr = 613;
            } else if (labelName == "FY") {
                highAddr = 614; lowAddr = 615;
            } else if (labelName == "FZ") {
                highAddr = 616; lowAddr = 617;
            } else if (labelName == "MX") {
                highAddr = 618; lowAddr = 619;
            } else if (labelName == "MY") {
                highAddr = 620; lowAddr = 621;
            } else {  // MZ
                highAddr = 622; lowAddr = 623;
            }

            // 更新寄存器缓存
            static QMap<int, quint16> bigForceRegisterCache;
            bigForceRegisterCache[address] = value;

            // 只有当两个寄存器都有值时才计算
            if (bigForceRegisterCache.contains(highAddr) && bigForceRegisterCache.contains(lowAddr)) {
                quint16 high = bigForceRegisterCache[highAddr];
                quint16 low = bigForceRegisterCache[lowAddr];

                float floatValue = registersToFloat(high, low);

                // 调试输出
                static int debugCount = 0;
                if (debugCount++ % 6 == 0) // 减少刷屏，每6次（大约一组）打印一次
                   qDebug() << "【大" << labelName << "】" << "值:" << floatValue << " (Raw: " << high << "," << low << ")";

                // 更新标签显示
                updateBigForceLabel(labelName, floatValue);
            }
        }
    }

    // ============ 新增：处理小六维力寄存器（624-635） ============
    if (address >= 624 && address <= 635) {
        
        // 调试输出：确认收到小六维力数据
        // qDebug() << "收到小六维力寄存器数据 - 地址:" << address << "值:" << value;

        // 定义地址到小六维力标签的映射
        static QMap<int, QString> smallForceAddressToLabelMap = {
            {624, "FX"}, {625, "FX"},
            {626, "FY"}, {627, "FY"},
            {628, "FZ"}, {629, "FZ"},
            {630, "MX"}, {631, "MX"},
            {632, "MY"}, {633, "MY"},
            {634, "MZ"}, {635, "MZ"}
        };

        if (smallForceAddressToLabelMap.contains(address)) {
            QString labelName = smallForceAddressToLabelMap[address];

            // 获取对应的寄存器对地址
            int highAddr, lowAddr;
            if (labelName == "FX") {
                highAddr = 624; lowAddr = 625;
            } else if (labelName == "FY") {
                highAddr = 626; lowAddr = 627;
            } else if (labelName == "FZ") {
                highAddr = 628; lowAddr = 629;
            } else if (labelName == "MX") {
                highAddr = 630; lowAddr = 631;
            } else if (labelName == "MY") {
                highAddr = 632; lowAddr = 633;
            } else {  // MZ
                highAddr = 634; lowAddr = 635;
            }

            // 更新寄存器缓存
            static QMap<int, quint16> smallForceRegisterCache;
            smallForceRegisterCache[address] = value;

            // 只有当两个寄存器都有值时才计算
            if (smallForceRegisterCache.contains(highAddr) && smallForceRegisterCache.contains(lowAddr)) {
                quint16 high = smallForceRegisterCache[highAddr];
                quint16 low = smallForceRegisterCache[lowAddr];

                float floatValue = registersToFloat(high, low);

                // 调试输出
                static int smallDebugCount = 0;
                if (smallDebugCount++ % 6 == 0)
                   qDebug() << "【小" << labelName << "】"
                            << "高位(0x" << QString::number(high, 16).toUpper() << ")"
                            << "低位(0x" << QString::number(low, 16).toUpper() << ")"
                            << "值:" << floatValue;

                // 更新标签显示
                updateSmallForceLabel(labelName, floatValue);
            }
        }
    }
}
//浮点数辅助函数
// 将两个16位寄存器转换为32位浮点数（IEEE 754标准）
// 根据测试数据：高位=0x427B，低位=0xAD49，组合成0x427BAD49 = 62.9192
// 输入参数：high - 高位寄存器值，low - 低位寄存器值
// 简洁高效的registersToFloat函数
// 简洁高效的registersToFloat函数
float MainWindow::registersToFloat(quint16 high, quint16 low)
{
    // 打印详细的调试信息
   // qDebug() << "【浮点数转换】开始 - 高位:" << high << "(0x" << QString::number(high, 16).toUpper() << ")"
   //           << "低位:" << low << "(0x" << QString::number(low, 16).toUpper() << ")";

    // 合并为32位整数
    uint32_t combined = (static_cast<uint32_t>(high) << 16) | low;
    //qDebug() << "合并后的32位整数: 0x" << QString::number(combined, 16).toUpper();

    float result;
    memcpy(&result, &combined, sizeof(float));

    //qDebug() << "【浮点数转换结果】" << result;
    return result;
}

double MainWindow::registersToDoubleDCBAFEHG(quint16 reg1, quint16 reg2, quint16 reg3, quint16 reg4)
{
    // 修改：根据 64位大端 IEEE754 与 BADC FEHG 顺序转换
    // 从日志 [Modbus网络读] 收到字节数: 17 Hex: "... 00 00 00 00 00 00 3f 1a" 分析得到：
    // reg1: 0, 0
    // reg2: 0, 0
    // reg3: 0, 0
    // reg4: 3f, 1a
    // 假设目标是小值 (如 0.0001)，3f 应该是最高字节。
    // 这意味着寄存器顺序是 reg4, reg3, reg2, reg1 (或者 reg1/2/3/4 是相反的)
    // 且每个寄存器内是 Big Endian (0x3F1A -> 3F then 1A)
    
    // 重新映射：
    // 如果 Payload 是 00 00 00 00 00 00 3f 1a，而 3f 是指数位 (A)
    // A=3f, B=1a, C=00, D=00, E=00, F=00, G=00, H=00
    // A,B 来源 reg4 (high, low)
    // C,D 来源 reg3 (high, low)
    // E,F 来源 reg2 (high, low)
    // G,H 来源 reg1 (high, low)

    const quint8 A = static_cast<quint8>((reg4 >> 8) & 0xFF);
    const quint8 B = static_cast<quint8>(reg4 & 0xFF);
    const quint8 C = static_cast<quint8>((reg3 >> 8) & 0xFF);
    const quint8 D = static_cast<quint8>(reg3 & 0xFF);
    const quint8 E = static_cast<quint8>((reg2 >> 8) & 0xFF);
    const quint8 F = static_cast<quint8>(reg2 & 0xFF);
    const quint8 G = static_cast<quint8>((reg1 >> 8) & 0xFF);
    const quint8 H = static_cast<quint8>(reg1 & 0xFF);

    const quint64 combined =
        (static_cast<quint64>(A) << 56) |
        (static_cast<quint64>(B) << 48) |
        (static_cast<quint64>(C) << 40) |
        (static_cast<quint64>(D) << 32) |
        (static_cast<quint64>(E) << 24) |
        (static_cast<quint64>(F) << 16) |
        (static_cast<quint64>(G) << 8) |
        static_cast<quint64>(H);

    double result = 0.0;
    memcpy(&result, &combined, sizeof(double));
    return result;
}
//注册到sliderlabel

void MainWindow::setupModbusFloatReading()
{
    // 创建读取定时器
    m_modbusReadTimer = new QTimer(this);

    // 连接到读取函数
    connect(m_modbusReadTimer, &QTimer::timeout, this, &MainWindow::readAllFloatRegisters);

    // 清空之前的列表
    m_floatLabels.clear();

    // 查找所有TechSliderLabel控件
    QList<TechSliderLabel*> allSliderLabels = this->findChildren<TechSliderLabel*>();
    qDebug() << "找到" << allSliderLabels.size() << "个TechSliderLabel";

    // 选择特定的四个TechSliderLabel（根据对象名或位置）
    // 方法1：按对象名筛选（如果对象名有规律）
    QStringList targetNames = {
        "label_Value1", "label_Value2", "label_Value3", "label_Value4"  // 根据实际情况修改
    };

    for (const QString &name : targetNames) {
        TechSliderLabel* label = findChild<TechSliderLabel*>(name);
        if (label) {
            m_floatLabels.append(label);
            qDebug() << "添加TechSliderLabel:" << name;
        } else {
            qWarning() << "未找到TechSliderLabel:" << name;
        }
    }

    // 方法2：如果对象名没有规律，使用前4个或特定的4个
    if (m_floatLabels.isEmpty() && allSliderLabels.size() >= 4) {
        for (int i = 0; i < 4; ++i) {
            m_floatLabels.append(allSliderLabels[i]);
            qDebug() << "添加第" << i+1 << "个TechSliderLabel:"
                     << allSliderLabels[i]->objectName();
        }
    }

    if (m_floatLabels.size() < 4) {
        qWarning() << "警告：只找到" << m_floatLabels.size()
                   << "个TechSliderLabel，需要4个";
    }

    // 立即读取一次
    readAllFloatRegisters();

    // 每500毫秒读取一次
    m_modbusReadTimer->start(500);
}

void MainWindow::readAllFloatRegisters()
{
    // 修正轮询逻辑：J1-J4 以及其他状态数据在大全设备 (192.168.1.88) 上
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        static int warnCount = 0;
        if (warnCount++ % 10 == 0) {
            qWarning() << "[警告] 主 Modbus (192.168.1.88) 未连接，无法读取数据";
        }
        return;
    }

    // [调试日志] 
    static int timerExecCount = 0;
    if (timerExecCount++ % 20 == 0) {
        // qWarning() << "[轮询执行] 正在批量读取寄存器 (地址 0 - 71)...";
    }

    // 改为一次性读取 0 到 71 号寄存器 (共 72 个)
    // 这样涵盖了 J1-J4 (0,4,12,20) 以及后续可能的报警和状态位
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 0, 72);
}
// 配置所有TechSliderLabel的参数
void MainWindow::setupSliderLabelConfigs()
{
    qWarning() << "[配置中心] 正在初始化 J1-J4 地址映射...";
    m_sliderLabelConfigs.clear();

    // 所有四个控件都需要在三个页面中查找匹配
    QStringList allTargetPages = {"回转升降", "伸缩臂", "EOAT控制"};

    m_sliderLabelConfigs["robot_ArcGauge_J1Angle"] = {
        "悬臂组件当前角度:",           // labelText
        "°",                  // unit
        -90.0,               // minValue
        90.0,                // maxValue
        60.0,                // defaultValue
        "°",                 // suffix
        0,                   // modbusAddress1
        1,                   // modbusAddress2
        2,                   // modbusAddress3
        3,                   // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        1                    // 精度：1位小数
    };

    m_sliderLabelConfigs["robot_ArcGauge_J2Height"] = {
        "升降组件当前高度:",           // labelText
        "mm",                // unit
        -850.0,              // minValue
        1150.0,              // maxValue
        432.0,               // defaultValue
        "mm",                // suffix
        4,                   // modbusAddress1
        5,                   // modbusAddress2
        6,                   // modbusAddress3
        7,                   // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        0                    // 精度：0位小数
    };

    m_sliderLabelConfigs["robot_ArcGauge_J3Length"] = {
        "悬臂组件当前长度:",           // labelText
        "mm",                // unit
        0.0,                 // minValue
        1500.0,              // maxValue (修改最大值以容纳求和)
        560.0,               // defaultValue
        "mm",                // suffix
        12,                  // modbusAddress1
        13,                  // modbusAddress2
        14,                  // modbusAddress3
        15,                  // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        0,                   // 精度：0位小数
        true,                // isSumMode
        {16, 17, 18, 19}     // sumAddress
    };

    m_sliderLabelConfigs["robot_ArcGauge_J4Angle"] = {
        "末端组件当前角度:",    // labelText (修改为末端组件)
        "°",                  // unit
        -180.0,              // minValue
        180.0,               // maxValue
        -34.0,               // defaultValue
        "°",                 // suffix
        20,                  // modbusAddress1
        21,                  // modbusAddress2
        22,                  // modbusAddress3
        23,                  // modbusAddress4
        true,                // isMainPage
        allTargetPages,      // 在所有三个页面中查找
        1                    // 精度：1位小数
    };

    qDebug() << "SliderLabel配置初始化完成";
    qDebug() << "每个控件都需要在以下页面中查找匹配:" << allTargetPages;
}
void MainWindow::setupSliderLabelCopies()
{
    qDebug() << "=== 开始设置SliderLabel副本 ===";

    // 正则表达式，用于提取控件名中的数字部分
    QRegularExpression re("Value(\\d+)");

    // 为每个首页控件建立映射：数字 -> 首页控件
    QMap<int, TechSliderLabel*> mainControlsByNumber;

    for (int i = 1; i <= 4; i++) {
        QString controlName = QString("label_Value%1").arg(i);
        if (m_sliderLabelInstances.contains(controlName)) {
            mainControlsByNumber[i] = m_sliderLabelInstances[controlName];
            qDebug() << "首页控件映射: " << i << " -> " << controlName;
        }
    }

    // 定义要检查的三个目标页面
    QStringList targetPages = {"回转升降", "伸缩臂", "EOAT控制"};

    // 遍历每个目标页面
    for (const QString& pageName : targetPages) {
        qDebug() << "\n处理页面: " << pageName;

        // 找到目标页面的索引
        int pageIndex = -1;
        for (auto pageIt = m_pageNames.begin(); pageIt != m_pageNames.end(); ++pageIt) {
            if (pageIt.value() == pageName) {
                pageIndex = pageIt.key();
                break;
            }
        }

        if (pageIndex == -1) {
            qWarning() << "找不到页面:" << pageName;
            continue;
        }

        // 获取目标页面
        QWidget* targetPage = ui->StackedWidget->widget(pageIndex);
        if (!targetPage) {
            qWarning() << "页面" << pageIndex << "不存在";
            continue;
        }

        // 获取目标页面中所有的TechSliderLabel
        QList<TechSliderLabel*> targetSliders = targetPage->findChildren<TechSliderLabel*>();
        qDebug() << "  页面中包含" << targetSliders.size() << "个TechSliderLabel";

        // 遍历目标页面中的每个控件
        for (TechSliderLabel* targetSlider : targetSliders) {
            QString targetName = targetSlider->objectName();

            // 从控件名中提取数字
            QRegularExpressionMatch match = re.match(targetName);
            if (match.hasMatch()) {
                int targetNumber = match.captured(1).toInt();
                qDebug() << "  检查控件:" << targetName << "，提取数字:" << targetNumber;

                // 查找对应的首页控件
                if (mainControlsByNumber.contains(targetNumber)) {
                    TechSliderLabel* original = mainControlsByNumber[targetNumber];
                    QString originalName = original->objectName();

                    // 获取配置
                    if (m_sliderLabelConfigs.contains(originalName)) {
                        const SliderLabelConfig& config = m_sliderLabelConfigs[originalName];

                        qDebug() << "    匹配成功! 将" << originalName << "的配置复制给" << targetName;

                        // 复制配置
                        targetSlider->setLabelText(config.labelText);
                        targetSlider->setRange(config.minValue, config.maxValue);
                        targetSlider->setValue(config.defaultValue);
                        targetSlider->setSuffix(config.suffix);
                        targetSlider->setPrecision(config.precision); // 设置精度
                        targetSlider->setTechBlueStyle();

                        // 添加到管理列表
                        m_pageSliders[pageName].append(targetSlider);
                    } else {
                        qWarning() << "    警告: 找不到" << originalName << "的配置";
                    }
                } else {
                    qDebug() << "    没有找到编号为" << targetNumber << "的首页控件";
                }
            } else {
                qDebug() << "  控件" << targetName << "不包含'ValueX'模式，跳过";
            }
        }
    }

    // 验证结果
    qDebug() << "\n=== 复制结果验证 ===";
    for (const QString& pageName : targetPages) {
        if (m_pageSliders.contains(pageName)) {
            const QVector<TechSliderLabel*>& sliders = m_pageSliders[pageName];
            qDebug() << "页面[" << pageName << "]有" << sliders.size() << "个已配置的SliderLabel:";
            for (TechSliderLabel* slider : sliders) {
                qDebug() << "  - " << slider->objectName() << "标签:" << slider->labelText();
            }
        } else {
            qDebug() << "页面[" << pageName << "]没有已配置的SliderLabel";
        }
    }

    qDebug() << "=== SliderLabel副本设置完成 ===";
}


void MainWindow::setupAGVModbus()
{
    if (!isBigFeatureEnabled("modbus_agv")) {
        qDebug() << "AGV Modbus功能已关闭，跳过初始化";
        return;
    }

    // 创建AGV Modbus管理器
    m_agvModbusManager = new AGVModbusManager(this);
    qDebug() << "创建AGV Modbus管理器完成";

    // 连接信号槽
    connect(m_agvModbusManager, &AGVModbusManager::connected,
            this, &MainWindow::onAGVModbusConnected);
    connect(m_agvModbusManager, &AGVModbusManager::disconnected,
            this, &MainWindow::onAGVModbusDisconnected);
    connect(m_agvModbusManager, &AGVModbusManager::errorOccurred,
            this, &MainWindow::onAGVModbusError);
    connect(m_agvModbusManager, &AGVModbusManager::bitVariableChanged,
            this, &MainWindow::onAGVBitVariableChanged);
    connect(m_agvModbusManager, &AGVModbusManager::wordVariableChanged,
            this, &MainWindow::onAGVWordVariableChanged);
    connect(m_agvModbusManager, &AGVModbusManager::updateFaultsLabel,
            this, &MainWindow::onAGVUpdateFaultsLabel);
    connect(m_agvModbusManager, &AGVModbusManager::updateProgressBar,
            this, &MainWindow::onAGVUpdateProgressBar);
    connect(m_agvModbusManager, &AGVModbusManager::updateStatusLabel,
            this, &MainWindow::onAGVUpdateStatusLabel);
    connect(m_agvModbusManager, &AGVModbusManager::addFaultCodeToList,
            this, &MainWindow::onAGVAddFaultCodeToList);
    connect(m_agvModbusManager, &AGVModbusManager::clearFaultCodes,
            this, &MainWindow::onAGVClearFaultCodes);
    connect(m_agvModbusManager, &AGVModbusManager::heartbeatReceived,
            this, &MainWindow::onAGVHeartbeatReceived);

    // 连接转向切换检查槽函数
    connect(m_agvModbusManager, &AGVModbusManager::registerValueChanged,
            this, &MainWindow::checkSteeringSwitchCompletion);

    // 添加registerValueChanged信号连接用于调试
    connect(m_agvModbusManager, &AGVModbusManager::registerValueChanged,
            this, [this](int address, quint16 value) {
                qDebug() << "[AGV] 寄存器值变化 - 地址:" << address
                         << "值:" << value
                         << "(0x" << QString::number(value, 16).toUpper() << ")";
            });

    // 配置
    m_agvModbusManager->setPollInterval(m_agvPollIntervalMs);
    m_agvModbusManager->setAutoReconnect(true, m_agvReconnectIntervalMs);

    // 连接到设备 - 88 -> 100
    QString agvHost = "192.168.1.100";
    quint16 agvPort = 502;

    // 如果开启本机 TCP 模拟器模式，则重定向到本机 AGV 模拟端口
    if (isFeatureEnabled("tcp_transmission", "tcp.local_simulator")) {
        agvHost = "127.0.0.1";
        agvPort = 5021;
        qDebug() << "启用本机 TCP 模拟器模式：AGV ->" << agvHost << ":" << agvPort;
    } else if (isFeatureEnabled("tcp_transmission", "tcp.remote_simulator")) {
        agvHost = "192.168.1.70";
        agvPort = 5021;
        qDebug() << "启用远程 TCP 模拟器模式：AGV ->" << agvHost << ":" << agvPort;
    }

    m_agvModbusManager->connectToDevice(agvHost, agvPort);

    qDebug() << "192.168.1.100 AGV Modbus管理器初始化完成";

    // 添加定时器检查连接状态
    QTimer::singleShot(2000, this, [this]() {
        if (m_agvModbusManager && m_agvModbusManager->isConnected()) {
            qDebug() << "AGV Modbus已成功连接";
            ui->statusBar->showMessage("AGV Modbus已连接", 3000);
        } else {
            qDebug() << "AGV Modbus未连接";
            ui->statusBar->showMessage("AGV Modbus连接失败", 3000);
        }
    });

    // 连接信号槽
    bool connected1 = connect(m_agvModbusManager, &AGVModbusManager::updateProgressBar,
                              this, &MainWindow::onAGVUpdateProgressBar, Qt::QueuedConnection);
    qDebug() << "updateProgressBar信号连接状态:" << (connected1 ? "成功" : "失败");
}











/**
 * @brief 设置 AGV 页面相关的 UI 并进行初始化
 *
 * 在 AGV 控制页面中查找并缓存常用控件（电池进度条、故障列表、故障/状态标签等），
 * 并对速度仪表（`TechSpeedGauge`）做单位与量程调整。该函数假定 UI 已经加载完毕，
 * 仅执行查找与初始化，不创建页面控件。
 *
 * 用法示例：
 * @code
 * // 在 MainWindow 构造函数或 UI 初始化后调用
 * setupAGVUI();
 * @endcode
 */
void MainWindow::setupAGVUI()
{
    if (!isBigFeatureEnabled("modbus_agv")) {
        return;
    }

    // 查找AGV页面上的控件
    QWidget *agvPage = ui->StackedWidget->widget(4);  // AGV控制页面

    if (agvPage) {
        // 先清理可能存在的旧缓存，防止重复添加
        m_agvStatusLabels.clear();

        // 查找故障列表
        m_agvFaultListWidget = agvPage->findChild<QListWidget*>("listWidget_faultCodes");

        // 查找故障标签
        m_agvFaultsLabel = agvPage->findChild<QLabel*>("label_faults");

        // 查找状态标签（根据您的UI命名）
        QStringList statusLabelNames = {
            "label_front_touch", "label_back_touch", "label_left_touch", "label_right_touch",
            "label_front_slow", "label_front_stop", "label_back_slow", "label_back_stop",
            "label_jog_running", "label_steering_align", "label_transverse_mode",
            "label_rotate_mode", "label_speed", "label_jog_displacement",
            "label_battery1_text", "label_battery2_text", "label_agv_connection"
        };

        for (const QString &name : statusLabelNames) {
            QLabel *label = agvPage->findChild<QLabel*>(name);
            if (label) {
                m_agvStatusLabels.append(label);
                // 设置初始文本
                if (name.contains("_touch") || name.contains("_slow") ||
                    name.contains("_stop") || name.contains("_running") ||
                    name.contains("_align") || name.contains("_mode")) {
                    label->setText("无动作");
                } else if (name == "label_agv_connection") {
                    label->setText("未连接");
                } else if (name == "label_faults") {
                    label->setText("无故障");
                }
            }
        }
        if (m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
            QQuickItem *rootItem = m_speedGaugeQml->rootObject();
            rootItem->setProperty("minValue", 0);
            rootItem->setProperty("maxValue", 900);
            rootItem->setProperty("unit", "mm/s");
            rootItem->setProperty("currentValue", 0);
            qDebug() << "QML AGV速度仪表初始化完成，量程:0-900 mm/s";
        } else {
            qWarning() << "未找到QML AGV速度仪表";
        }

        qDebug() << "找到" << m_agvStatusLabels.size() << "个AGV状态标签";
    }

    qDebug() << "找到" << m_agvStatusLabels.size() << "个AGV状态标签";





    if (agvPage) {
        QList<QProgressBar*> allBars = agvPage->findChildren<QProgressBar*>();
        for (QProgressBar* bar : allBars) {
            qDebug() << "进度条:" << bar->objectName()
                     << "当前值:" << bar->value()
                     << "范围:" << bar->minimum() << "-" << bar->maximum();
        }

        QList<BatteryWidget*> allBatteryWidgets = agvPage->findChildren<BatteryWidget*>();
        for (BatteryWidget* bar : allBatteryWidgets) {
            qDebug() << "BatteryWidget:" << bar->objectName() << "当前值:" << bar->level();
        }
    }
}

// 添加槽函数实现
/**
 * @brief 处理 AGV Modbus 连接成功事件
 *
 * 更新状态栏并记录连接事件到操作记录器。
 */
void MainWindow::onAGVModbusConnected()
{
    qDebug() << "AGV Modbus连接成功";
    m_agvDisconnectedWarnedAddresses.clear();
    ui->statusBar->showMessage("AGV Modbus已连接", 3000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
        agvIndicator->setToolTip("AGV Modbus连接正常");
    }

    // 记录连接事件
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "AGV Modbus连接";
    record.controlType = "ModbusTCP";
    record.operation = "connected";
    record.oldValue = "";
    record.newValue = "连接成功";
    m_recorder->addRecord(record);
}

/**
 * @brief 处理 AGV Modbus 断开连接事件
 *
 * 更新 UI 中的连接状态显示并记录断开事件。
 */
void MainWindow::onAGVModbusDisconnected()
{
    qDebug() << "AGV Modbus连接断开";
    ui->statusBar->showMessage("AGV Modbus连接断开", 3000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
        agvIndicator->setToolTip("AGV Modbus已断开");
    }

    // 更新连接状态标签
    if (m_agvFaultsLabel) {
        m_agvFaultsLabel->setText("连接断开");
    }

    // 记录断开事件
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "AGV Modbus连接";
    record.controlType = "ModbusTCP";
    record.operation = "disconnected";
    record.oldValue = "";
    record.newValue = "连接断开";
    m_recorder->addRecord(record);
}

/**
 * @brief 处理 AGV Modbus 错误通知
 * @param error 错误描述字符串
 *
 * 在收到错误时显示到状态栏并记录到操作记录。
 */
void MainWindow::onAGVModbusError(const QString &error)
{
    qDebug() << "AGV Modbus错误:" << error;
    ui->statusBar->showMessage(QString("AGV Modbus错误: %1").arg(error), 5000);

    // 更新状态栏指示器
    QLabel *agvIndicator = ui->statusBar->findChild<QLabel*>("agvModbusStatusIndicator");
    if (agvIndicator) {
        agvIndicator->setStyleSheet("color: #ffaa00; font-weight: bold; font-size: 10px;"); // 橙色表示错误/警告
        agvIndicator->setToolTip(QString("AGV Modbus错误: %1").arg(error));
    }

    // 记录错误事件
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "AGV Modbus连接";
    record.controlType = "ModbusTCP";
    record.operation = "error";
    record.oldValue = "";
    record.newValue = error;
    m_recorder->addRecord(record);
}

void MainWindow::onAGVBitVariableChanged(int address, int bitPos, bool value)
{
    // 处理障碍物传感器数据 (地址50)
    if (address == 50 && m_speedGaugeQml && m_speedGaugeQml->rootObject()) {
        // 获取当前状态（从 gauge 中读取或自行维护，这里我们根据 bitPos 更新）
        // 1: 前触边, 2: 后触边, 3: 左触边, 4: 右触边
        // 5,6: 前避障, 7,8: 后避障
        
        static bool f = false, b = false, l = false, r = false;
        
        if (bitPos == 1 || bitPos == 5 || bitPos == 6) {
            // 前方障碍物：触边 OR 减速 OR 停止
            static bool t=false, s1=false, s2=false;
            if(bitPos == 1) t = value;
            if(bitPos == 5) s1 = value;
            if(bitPos == 6) s2 = value;
            f = t || s1 || s2;
        } else if (bitPos == 2 || bitPos == 7 || bitPos == 8) {
            // 后方障碍物
            static bool bt=false, bs1=false, bs2=false;
            if(bitPos == 2) bt = value;
            if(bitPos == 7) bs1 = value;
            if(bitPos == 8) bs2 = value;
            b = bt || bs1 || bs2;
        } else if (bitPos == 3) {
            // 左侧触边
            l = value;
        } else if (bitPos == 4) {
            // 右侧触边
            r = value;
        }

        QQuickItem *rootItem = m_speedGaugeQml->rootObject();
        rootItem->setProperty("obstacleFront", f);
        rootItem->setProperty("obstacleBack", b);
        rootItem->setProperty("obstacleLeft", l);
        rootItem->setProperty("obstacleRight", r);
    }
}

void MainWindow::onAGVWordVariableChanged(int address, quint16 value)
{
    // 特别处理行驶速度（地址104）
    if (address == 104 && isFeatureEnabled("modbus_agv", "agv.speed_gauge")) {
        // 行驶速度 (mm/s)
        qreal speedValue = static_cast<qreal>(value);

        updateSpeed(speedValue);

        // 同时更新原来的label_speed（如果需要保持兼容）
        // 直接调用onAGVUpdateStatusLabel函数，而不是发射信号
        onAGVUpdateStatusLabel("label_speed", QString("%1 mm/s").arg(value));
    }
}






void MainWindow::onAGVUpdateFaultsLabel(const QString &text)
{
    if (m_agvFaultsLabel) {
        m_agvFaultsLabel->setText(text);
    }
}
void MainWindow::onAGVUpdateProgressBar(const QString &name, int value)
{
    // 在 AGV 页面中动态查找控件进行更新
    QWidget *agvPage = ui->StackedWidget->widget(4);
    if (!agvPage) return;

    // 优先尝试查找并更新提升后的 BatteryWidget
    BatteryWidget *bw = agvPage->findChild<BatteryWidget*>(name);
    if (bw) {
        bw->setLevel(static_cast<double>(value));
        return;
    }

    // 后备方案：查找传统 QProgressBar (如果 UI 还没来得及替换或作为回退)
    QProgressBar *progressBar = agvPage->findChild<QProgressBar*>(name);
    if (progressBar) {
        progressBar->setValue(value);
        progressBar->update();
    }
}

void MainWindow::onAGVAddFaultCodeToList(const QString &faultCode)
{
    if (!isFeatureEnabled("modbus_agv", "agv.fault_codes")) {
        return;
    }

    qDebug() << "[MainWindow] 添加故障代码到列表:" << faultCode;
    if (m_agvFaultListWidget) {
        m_agvFaultListWidget->addItem(faultCode);
        qDebug() << "  成功添加到列表";
    } else {
        qDebug() << "  错误：故障列表控件未找到";
    }
}

void MainWindow::onAGVClearFaultCodes()
{
    if (!isFeatureEnabled("modbus_agv", "agv.fault_codes")) {
        return;
    }

    if (m_agvFaultListWidget) {
        m_agvFaultListWidget->clear();
    }
}

void MainWindow::onAGVHeartbeatReceived()
{
    // 心跳接收，可以用于更新连接状态或显示最后通信时间
    static int heartbeatCount = 0;
    heartbeatCount++;

    if (heartbeatCount % 10 == 0) {
        qDebug() << "AGV心跳 - 计数:" << heartbeatCount;
    }
}
void MainWindow::onAGVUpdateStatusLabel(const QString &name, const QString &text)
{
    // 查找对应的标签并更新运动速度
    for (QLabel *label : m_agvStatusLabels) {
        if (label && label->objectName() == name) {
            label->setText(text);
            break;
        }
    }
}
// ============ 使能按钮功能实现 ============

// 处理使能按钮状态变化
void MainWindow::onEnableButtonStateChanged(bool enabled)
{
    if (!isFeatureEnabled("input_devices", "input.enable_button")) {
        return;
    }

    // 这个槽函数在主线程中执行
    qDebug() << "[主线程] 收到使能按钮状态:" << (enabled ? "按下" : "松开");

    // 如果状态发生变化
    if (enabled != m_lastEnableButtonState) {
        m_lastEnableButtonState = enabled;

        QString statusText = enabled ? "使能按钮: 按下" : "使能按钮: 松开";
        qDebug() << "=== 使能按钮状态变化: " << statusText << " ===";

        // 更新状态栏
        ui->statusBar->showMessage(statusText, 2000);

        // 处理使能状态变化
        processEnableButton(enabled);

        // 记录到操作记录
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "系统";
        record.controlName = "使能按钮";
        record.controlType = "EnableButton";
        record.operation = "状态变化";
        record.oldValue = !enabled;
        record.newValue = enabled;
        m_recorder->addRecord(record);
    }
}

// 处理使能按钮错误
void MainWindow::onEnableButtonError(const QString &error)
{
    qWarning() << "使能按钮错误:" << error;
    ui->statusBar->showMessage(error, 5000);
}

/**
 * @brief 初始化并启动使能按钮的监控线程
 *
 * 该函数打开设备文件 `/dev/buttonstop`，创建 `EnableButtonWorker` 并将其
 * 移动到工作线程中以异步读取按键状态，接收信号并在主线程中更新 UI。
 */
void MainWindow::setupEnableButton()
{
    if (!isFeatureEnabled("input_devices", "input.enable_button")) {
        qDebug() << "使能按钮功能已关闭，跳过初始化";
        return;
    }

    qDebug() << "初始化使能按钮...";

    // 检查设备文件是否存在
    if (access("/dev/buttonstop", F_OK) == -1) {
        qWarning() << "错误：/dev/buttonstop 设备文件不存在";
        ui->statusBar->showMessage("使能按钮设备不存在", 5000);
        return;
    }

    // 注意：使用阻塞模式打开，不在主线程中读取
    m_enableButtonFd = open("/dev/buttonstop", O_RDONLY);
    if (m_enableButtonFd < 0) {
        QString error = QString("无法打开使能按钮设备 /dev/buttonstop: %1 (errno: %2)")
                            .arg(strerror(errno)).arg(errno);
        qWarning() << error;
        ui->statusBar->showMessage(error, 5000);
        return;
    }

    qDebug() << "使能按钮设备打开成功，文件描述符:" << m_enableButtonFd;

    // 创建工作线程读取使能按钮

    // 使用Qt的线程机制
    QThread *enableThread = new QThread(this);

    // 创建Worker对象
    EnableButtonWorker *worker = new EnableButtonWorker(m_enableButtonFd);
    worker->moveToThread(enableThread);

    // 连接信号槽
    connect(enableThread, &QThread::started, worker, &EnableButtonWorker::startPolling);
    connect(worker, &EnableButtonWorker::buttonStateChanged,
            this, &MainWindow::onEnableButtonStateChanged);
    connect(worker, &EnableButtonWorker::errorOccurred,
            this, &MainWindow::onEnableButtonError);

    // 线程结束时清理
    connect(enableThread, &QThread::finished, worker, &EnableButtonWorker::deleteLater);
    connect(enableThread, &QThread::finished, enableThread, &QThread::deleteLater);

    // 启动线程
    enableThread->start();

    qDebug() << "使能按钮监控线程已启动";
    ui->statusBar->showMessage("使能按钮监控已启动", 3000);
}

void MainWindow::pollEnableButton()
{
    if (m_enableButtonFd < 0) {
        return;
    }

    char data[8];
    ssize_t bytesRead = read(m_enableButtonFd, data, sizeof(data));

    // 调试输出
    static int debugCount = 0;
    if (debugCount++ % 20 == 0) {  // 每20次输出一次，避免日志过多
        qDebug() << "轮询使能按钮，读取结果:" << bytesRead << "字节";
    }

    if (bytesRead != sizeof(data)) {
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下没有数据是正常的
                return;
            }
            qWarning() << "读取使能按钮数据失败:" << strerror(errno);
        } else if (bytesRead > 0) {
            qWarning() << "读取到不完整数据:" << bytesRead << "字节";
        }
        return;
    }

    // 转换为字符串以便调试
    QString dataStr;
    for (int i = 0; i < 8; i++) {
        dataStr.append(QChar(data[i]));
    }

    qDebug() << "收到使能按钮数据:" << dataStr;

    // 解析状态
    // 根据规格书和你的测试，byte[0] 是 '1' 或 '0'
    bool enabled = (data[0] == '1');

    // 详细调试
    qDebug() << "解析使能按钮状态: data[0] = " << data[0]
             << " (ASCII: " << (int)data[0] << ")"
             << " -> " << (enabled ? "按下" : "松开");

    // 如果状态发生变化
    if (enabled != m_lastEnableButtonState) {
        m_lastEnableButtonState = enabled;

        qDebug() << "=== 使能按钮状态变化 ==="
                 << (enabled ? "按下" : "松开");

        // 更新状态栏
        QString statusText = enabled ? "使能按钮: 按下" : "使能按钮: 松开";
        ui->statusBar->showMessage(statusText, 2000);

        // 处理使能状态变化
        processEnableButton(enabled);

        // 记录到操作记录
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "系统";
        record.controlName = "使能按钮";
        record.controlType = "EnableButton";
        record.operation = "状态变化";
        record.oldValue = !enabled;
        record.newValue = enabled;
        m_recorder->addRecord(record);
    }
}


void MainWindow::onEnableButtonActivated(int socket)
{
    qDebug() << "=== 使能按钮读取触发 ===";

    char data[8] = {0};
    ssize_t bytesRead = read(socket, data, sizeof(data));

    qDebug() << "读取到" << bytesRead << "字节数据";

    if (bytesRead < 0) {
        if (errno == EAGAIN) {
            qDebug() << "没有数据（非阻塞模式正常返回）";
            return;
        }
        qWarning() << "读取使能按钮数据失败:" << strerror(errno);
        return;
    }

    // 打印所有字节的16进制值
    QString hexData;
    for (int i = 0; i < bytesRead; i++) {
        hexData += QString("0x%1 ").arg((unsigned char)data[i], 2, 16, QChar('0')).toUpper();
    }
    qDebug() << "原始数据（十六进制）:" << hexData;

    // 打印每个字节的二进制值
    QString binaryData;
    for (int i = 0; i < bytesRead; i++) {
        binaryData += QString("byte[%1]: ").arg(i);
        for (int bit = 7; bit >= 0; bit--) {
            binaryData += ((data[i] >> bit) & 1) ? "1" : "0";
        }
        binaryData += " ";
    }
    qDebug() << "原始数据（二进制）:" << binaryData;

    // 解析使能按钮状态（根据规格书，byte 0表示S1开关）
    bool enabled = (data[0] == 1);

    qDebug() << "解析结果: byte[0] =" << (int)data[0] << "->" << (enabled ? "激活" : "未激活");

    // 如果状态发生变化
    if (enabled != m_lastEnableButtonState) {
        m_lastEnableButtonState = enabled;

        // 记录状态变化
        qDebug() << "使能按钮状态变化:" << (enabled ? "激活" : "未激活");

        // 更新状态栏
        ui->statusBar->showMessage(QString("使能按钮: %1").arg(enabled ? "激活" : "未激活"), 2000);

        // 处理使能状态变化
        processEnableButton(enabled);

        // 记录到操作记录
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "系统";
        record.controlName = "使能按钮";
        record.controlType = "EnableButton";
        record.operation = "状态变化";
        record.oldValue = !enabled;
        record.newValue = enabled;
        m_recorder->addRecord(record);
    } else {
        qDebug() << "状态未变化，忽略";
    }
}




/**
 * @brief 处理来自外部使能按钮的状态变化
 * @param enabled 是否处于使能/按下状态
 *
 * 根据当前步进/点动模式分别处理使能按键的行为，包含发送 Modbus 写命令
 * 以启用/禁用运动控制，并记录操作。
 */
void MainWindow::processEnableButton(bool enabled)
{
    if (m_stepModeEnabled) {
        // 步进模式下的使能按钮处理
        if (enabled) {
            onEnableButtonPressedStepMode();
        } else {
            onEnableButtonReleasedStepMode();
        }
    } else {
        // 原有的点动模式处理逻辑
        if (enabled) {
            // 使能按钮按下（激活）时的处理
            qDebug() << "外部使能按钮激活，允许运动控制";

            // 执行原来的 onEnableButtonPressed 逻辑
            // writeToMainDevice(19, 1);
            // writeToMainDevice(119, 1);

            qDebug() << "使能按钮按下，跳过地址19和119写入";
            ui->statusBar->showMessage("使能按钮按下，运动控制已激活", 2000);
        } else {
            // 使能按钮释放（未激活）时的处理
            qDebug() << "外部使能按钮未激活，禁止运动控制";

            // 执行原来的 onEnableButtonReleased 逻辑
            // writeToMainDevice(19, 0);
            // writeToMainDevice(119, 0);

            qDebug() << "使能按钮释放，跳过地址19和119写入";
            ui->statusBar->showMessage("使能按钮释放，运动控制已禁用", 2000);
        }
    }

}

// 修改 MainWindow::performStartupWrites() 函数
void MainWindow::performStartupWrites()
{
    qDebug() << "=== 启动写寄存器功能已关闭，跳过所有开机写入 ===";
    return;
}

// 修改 writeToAGVDevice 函数以支持负数（如果需要）
/**
 * @brief 向 AGV 设备写入单个寄存器（支持负值转换）
 * @param address 寄存器地址
 * @param value 要写入的数值（可以为负数，内部会转换为补码）
 * @note 若未连接会尝试延迟重试
 */
void MainWindow::writeToAGVDevice(int address, int value)
{
    if (!m_agvModbusManager || !m_agvModbusManager->isConnected()) {
        if (!m_agvDisconnectedWarnedAddresses.contains(address)) {
            qWarning() << "AGV Modbus未连接，无法写入地址" << address;
            m_agvDisconnectedWarnedAddresses.insert(address);
        }
        return;
    }

    m_agvDisconnectedWarnedAddresses.remove(address);

    qDebug() << "[AGV] 写入地址:" << address << "值:" << value;

    // 转换负数为无符号数（如果需要）
    quint16 writeValue;
    if (value < 0) {
        writeValue = static_cast<quint16>(65536 + value);  // 负数转换为补码
        qDebug() << "[AGV] 负数转换:" << value << "->" << writeValue;
    } else {
        writeValue = static_cast<quint16>(value);
    }

    // 执行写入
    bool writeSuccess = m_agvModbusManager->writeSingleRegister(address, writeValue);

    if (!writeSuccess) {
        qWarning() << "[AGV] 写入请求发送失败 - 地址:" << address;
    }
}







/**
 * @brief 向主设备写入单个寄存器
 * @param address 寄存器地址
 * @param value 要写入的数值
 */
void MainWindow::writeToMainDevice(int address, int value)
{
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        qWarning() << "主Modbus未连接，无法写入地址" << address;
        return;
    }

    qDebug() << "[主设备] 写入地址:" << address << "(&MB" << (address + 1) << ")"
             << "值:" << value;

    MainDeviceModbusApi::writeRegister(m_modbusManager, address, value);
}
void MainWindow::onControlModeClicked()
{
    // 切换控制模式
    if (m_controlMode == WIRED_MODE) {
        m_controlMode = WIRELESS_MODE;
        m_controlModeBtn->setText("无线控制");
        
        // 先发送第一个命令
        writeToAGVDevice(6,1);
        
        // 延时发送第二个命令，避免Modbus指令冲突
        QTimer::singleShot(100, this, [this]() {
            if(m_agvOaEnabled)
            {
                // 给192.168.1.88设备的0地址写64
                writeToAGVDevice(0, 192);
            }
            else
            {
                // 给192.168.1.88设备的0地址写66
                writeToAGVDevice(0, 194);
            }
        });

        qDebug() << "切换到无线控制模式";
        ui->statusBar->showMessage("已切换到无线控制模式", 2000);

        // 更新状态栏显示
        QLabel *controlModeLabel = ui->statusBar->findChild<QLabel*>("statusBarControlModeLabel");
        if (controlModeLabel) {
            controlModeLabel->setText("无线控制");
            controlModeLabel->setStyleSheet("color: #ffff00; font-weight: bold; font-size: 11px;");
        }
    } else {
        m_controlMode = WIRED_MODE;
        m_controlModeBtn->setText("有线控制");

        // ... 省略逻辑 ...
        
        qDebug() << "切换到有线控制模式";
        ui->statusBar->showMessage("已切换到有线控制模式", 2000);

        // 更新状态栏显示
        QLabel *controlModeLabel = ui->statusBar->findChild<QLabel*>("statusBarControlModeLabel");
        if (controlModeLabel) {
            controlModeLabel->setText("有线控制");
            controlModeLabel->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 11px;");
        }
    }

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "TBtn_ControlMode";
    record.controlType = "QToolButton";
    record.operation = "mode_switch";
    record.oldValue = (m_controlMode == WIRED_MODE) ? "无线控制" : "有线控制";
    record.newValue = (m_controlMode == WIRED_MODE) ? "有线控制" : "无线控制";
    m_recorder->addRecord(record);
}
// void MainWindow::onEnableButtonPressed()
// {
//     // 给192.168.1.13的19地址写1
//     writeToMainDevice(19, 1);
//     // 给192.168.1.13的119地址写1
//     writeToMainDevice(119, 1);

//     qDebug() << "使能按钮按下，地址119写入1";
//     ui->statusBar->showMessage("使能按钮按下", 1000);
// }

// void MainWindow::onEnableButtonReleased()
// {
//     // 给192.168.1.13的119地址写0
//     writeToMainDevice(119, 0);

//     qDebug() << "使能按钮释放，地址119写入0";
//     ui->statusBar->showMessage("使能按钮释放", 1000);
// }
QString MainWindow::getCurrentPageName() const
{
    int currentIndex = ui->StackedWidget->currentIndex();
    return m_pageNames.value(currentIndex, QString("未知页面"));
}
// 设置AGV避障开关控制
void MainWindow::setupAGVOAControl()
{
    if (!isBigFeatureEnabled("motion_control")) {
        return;
    }

    // 查找避障开关按钮
    m_techBtnAGV_OA = findChild<TechPushButton*>("techBtn_AGV_OA");
    if (m_techBtnAGV_OA) {
        // 初始状态为避障开启
        m_agvOaEnabled = true;
        m_techBtnAGV_OA->setText("避障开启");
        m_techBtnAGV_OA->setPrimaryColor(QColor("#00C8FF"));
        m_techBtnAGV_OA->setGlowColor(QColor(0, 200, 255, 180));

        // 连接点击信号
        connect(m_techBtnAGV_OA, &TechPushButton::clicked,
                this, &MainWindow::onAGVOABtnClicked);

        qDebug() << "AGV避障开关按钮初始化完成";
    } else {
        qWarning() << "未找到techBtn_AGV_OA按钮";
    }
}

// 设置AGV运动速度控制
void MainWindow::setupAGVMoveSpeedControl()
{
    if (!isBigFeatureEnabled("motion_control")) {
        return;
    }

    m_editAGV_MoveSpeed = findChild<TechSliderEdit*>("SEdit_AGV_MoveSpeed");
    if (m_editAGV_MoveSpeed) {
        // 设置参数 - 修改值域为 0-834
        m_editAGV_MoveSpeed->setLabelText("运动速度");
        m_editAGV_MoveSpeed->setRange(0, 834);  // 修改：0-834
        m_editAGV_MoveSpeed->setValue(0);       // 修改：初始值设为0
        m_editAGV_MoveSpeed->setSuffix("mm/s");
        m_editAGV_MoveSpeed->setPrecision(0);

        // 连接值变化信号
        connect(m_editAGV_MoveSpeed, &TechSliderEdit::valueChanged,
                this, &MainWindow::onAGVMoveSpeedChanged);

        qDebug() << "AGV运动速度控件初始化完成，范围:0-834，初始值:0";  // 修改日志
    } else {
        qWarning() << "未找到SEdit_AGV_MoveSpeed控件";
    }
}


// 设置AGV转向角度控制
void MainWindow::setupAGVAngleControl()
{
    if (!isBigFeatureEnabled("motion_control")) {
        return;
    }

    m_editAGV_Angle = findChild<TechSliderEdit*>("SEdit_AGV_Angle");
    if (m_editAGV_Angle) {
        // 设置参数
        m_editAGV_Angle->setLabelText("转向角度");
        m_editAGV_Angle->setRange(-25, 25);
        m_editAGV_Angle->setValue(0);
        m_editAGV_Angle->setSuffix("°");
        m_editAGV_Angle->setPrecision(0);  // 改为0，只显示整数
        m_editAGV_Angle->setPresetButtonsVisible(false); // 禁用预设按钮

        // 连接值变化信号
        connect(m_editAGV_Angle, &TechSliderEdit::valueChanged,
                this, &MainWindow::onAGVAngleChanged);

        qDebug() << "AGV转向角度控件初始化完成，范围:-25-25，初始值:0，精度:整数";
    } else {
        qWarning() << "未找到SEdit_AGV_Angle控件";
    }
}

// AGV避障开关按钮点击槽函数
void MainWindow::onAGVOABtnClicked()
{
    // 切换状态
    m_agvOaEnabled = !m_agvOaEnabled;

    if (m_agvOaEnabled) {
        // 避障开启
        m_techBtnAGV_OA->setText("避障开启");
        m_techBtnAGV_OA->setPrimaryColor(QColor("#00C8FF"));
        m_techBtnAGV_OA->setGlowColor(QColor(0, 200, 255, 180));
        // 给0地址写192
        writeToAGVDevice(0, 64);

        qDebug() << "AGV避障开启，地址0写入192";
        ui->statusBar->showMessage("AGV避障开启", 2000);
    } else {
        // 避障关闭
        m_techBtnAGV_OA->setText("避障关闭");
        m_techBtnAGV_OA->setPrimaryColor(QColor("#7F8C8D"));
        m_techBtnAGV_OA->setGlowColor(QColor(127, 140, 141, 100));
        // 给0地址写194
        writeToAGVDevice(0, 66);
        qDebug() << "AGV避障关闭，地址0写入194";
        ui->statusBar->showMessage("AGV避障关闭", 2000);
    }

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "techBtn_AGV_OA";
    record.controlType = "TechPushButton";
    record.operation = "oa_mode_changed";
    record.oldValue = m_agvOaEnabled ? "避障关闭" : "避障开启";
    record.newValue = m_agvOaEnabled ? "避障开启" : "避障关闭";
    m_recorder->addRecord(record);
}

// AGV运动速度变化槽函数
void MainWindow::onAGVMoveSpeedChanged(double value)
{
    int intValue = static_cast<int>(value);
    int modbusValue = 0;

    // 计算Modbus传输值
    if (intValue == 834) {
        // 最大值的特殊处理：直接传输50000
        modbusValue = 50000;
        qDebug() << "AGV运动速度:834（最大值），地址3写入:50000（最大值）";
    } else {
        // 正常值：乘以60
        modbusValue = intValue * 60;
        qDebug() << "AGV运动速度:" << intValue << "，乘以60后:" << modbusValue << "，地址3写入:" << modbusValue;
    }

    // 写入3地址
    writeToAGVDevice(3, modbusValue);

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "SEdit_AGV_MoveSpeed";
    record.controlType = "TechSliderEdit";
    record.operation = "move_speed_changed";
    record.oldValue = "";
    record.newValue = QString("%1 mm/s -> Modbus: %2").arg(intValue).arg(modbusValue);
    m_recorder->addRecord(record);
}

// AGV转向角度变化槽函数
void MainWindow::onAGVAngleChanged(double value)
{
    int intValue = static_cast<int>(value);  // 直接取整，不需要乘以10

    // 写入4地址，负数会自动转换为补码
    writeToAGVDevice(4, intValue);

    qDebug() << "AGV转向角度:" << value << "°，地址4写入:" << intValue;

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "SEdit_AGV_Angle";
    record.controlType = "TechSliderEdit";
    record.operation = "angle_changed";
    record.oldValue = "";
    record.newValue = QString::number(value);
    m_recorder->addRecord(record);
}


// 新增：处理AGV页面的按键○2动作
void MainWindow::handleAGVKey2Action(int keyNumber, bool pressed)
{
    if (keyNumber == 2) {
        // 查找避障按钮
        TechPushButton* oaButton = findChild<TechPushButton*>("techBtn_AGV_OA");
        if (!oaButton) {
            qWarning() << "未找到techBtn_AGV_OA按钮";
            return;
        }

        QString buttonText = oaButton->text();

        if (pressed) {
            // 按键○2按下
            if (buttonText == "避障开启") {
                // 避障开启时，给0地址写196
                writeToAGVDevice(0, 68);
                qDebug() << "按键○2按下，避障开启，地址0写入196";
                ui->statusBar->showMessage("AGV后退（避障开启）", 2000);
            } else if (buttonText == "避障关闭") {
                // 避障关闭时，给0地址写198
                writeToAGVDevice(0, 70);
                qDebug() << "按键○2按下，避障关闭，地址0写入198";
                ui->statusBar->showMessage("AGV后退（避障关闭）", 2000);
            }
        } else {
            // 按键○2释放
            if (buttonText == "避障开启") {
                // 避障开启时，给0地址写192
                writeToAGVDevice(0, 64);

                qDebug() << "按键○2释放，避障开启，地址0写入192";
                ui->statusBar->showMessage("AGV停止（避障开启）", 2000);
            } else if (buttonText == "避障关闭") {
                // 避障关闭时，给0地址写194
                writeToAGVDevice(0, 66);
                qDebug() << "按键○2释放，避障关闭，地址0写入194";
                ui->statusBar->showMessage("AGV停止（避障关闭）", 2000);
            }
        }

        // 记录操作
        OperationRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.pageName = "AGV控制";
        record.controlName = QString("按键○%1").arg(keyNumber);
        record.controlType = "MatrixKey";
        record.operation = pressed ? "pressed" : "released";
        record.oldValue = "";
        record.newValue = pressed ?
                              (buttonText == "避障开启" ? "后退（避障开启）" : "后退（避障关闭）") :
                              (buttonText == "避障开启" ? "停止（避障开启）" : "停止（避障关闭）");
        m_recorder->addRecord(record);
    }
}
//运动模式选择

// 新增：初始化转向模式控制
void MainWindow::setupSteeringModeControl()
{
    if (!isFeatureEnabled("motion_control", "motion.steering_mode")) {
        qDebug() << "转向模式功能已关闭，跳过初始化";
        return;
    }

    // 如果已经存在控件，先查找
    m_steeringModeSelector = findChild<SteeringModeSelector*>("steeringModeSelector");

    if (!m_steeringModeSelector) {
        // 在UI中放置SteeringModeSelector，可以通过提升Widget实现
        // 或者动态创建
        QWidget *agvPage = ui->StackedWidget->widget(4);  // AGV控制页面
        if (agvPage) {
            m_steeringModeSelector = agvPage->findChild<SteeringModeSelector*>("steeringModeSelector");
        }
    }

    if (m_steeringModeSelector) {
        // 设置样式为全息风格
        m_steeringModeSelector->setButtonStyle(TechPushButton::StyleHolographic);

        // 设置自定义颜色
        m_steeringModeSelector->setActiveColor(QColor(0, 200, 255));     // 激活状态颜色
        m_steeringModeSelector->setInactiveColor(QColor(80, 80, 100));   // 非激活状态颜色
        m_steeringModeSelector->setTextColor(Qt::white);                 // 文字颜色

        // 连接模式切换信号到报警逻辑
        connect(m_steeringModeSelector, &SteeringModeSelector::modeChanged,
                this, &MainWindow::onSteeringModeChanged);

        qDebug() << "转向模式选择器初始化完成";
    } else {
        qWarning() << "未找到转向模式选择器控件";
    }
}

// 新增：转向模式改变槽函数
void MainWindow::onSteeringModeChanged(SteeringMode mode, int modbusValue)
{
    qDebug() << "转向模式改变为:" << mode << "，Modbus值:" << modbusValue;

    // 向192.168.1.88的2地址写入对应值
    writeToAGVDevice(2, modbusValue);

    if (!isFeatureEnabled("alarm_system", "alarm.steering_switch")) {
        qDebug() << "转向模式切换报警功能已关闭，跳过报警窗口逻辑";
        m_isSwitchingSteeringMode = false;
        m_targetSteeringWaitBit = -1;
        if (m_isSteeringAlarmActive) {
            m_isSteeringAlarmActive = false;
            updateAlarmDisplay();
        }
        return;
    }

    // ============ 新增：转向模式切换报警逻辑 ============
    // 1、2、3按钮之间互相切换不报警
    // 切到1、2、3（从4、5切过来）：保持延时9秒
    // 切到4（横向）：判断50地址的10位和11位都为1
    // 切到5（旋转）：判断50地址的10位和12位都为1

    bool oldInGroupB = (m_lastSteeringMode == STEER_LATERAL || m_lastSteeringMode == STEER_ROTATE);
    bool newInGroupB = (mode == STEER_LATERAL || mode == STEER_ROTATE);
    
    // 更新上一次模式
    m_lastSteeringMode = mode;
    
    // 重置位等待标志
    m_isSwitchingSteeringMode = false;
    m_targetSteeringWaitBit = -1;

    if (newInGroupB) {
        // 切到4 或 5 -> 启动位信号检测
        qDebug() << "切换至模式4(横向)或5(旋转)，启动位信号检测";
        
        m_isSteeringAlarmActive = true;
        showAlarm("正在更换底盘模式", "#FFFF00", false);
        
        m_isSwitchingSteeringMode = true;
        if (mode == STEER_LATERAL) {
            m_targetSteeringWaitBit = 11; // 切到4(横向/Modbus 3) -> 等Bit 11
        } else {
            m_targetSteeringWaitBit = 12; // 切到5(旋转/Modbus 4) -> 等Bit 12
        }

        // 新增：9秒超时强制隐藏报警
        QTimer::singleShot(9000, this, [this, mode]() {
            // 如果9秒后仍在切换当前模式且还在等待该模式的位信号
            if (m_isSwitchingSteeringMode && m_lastSteeringMode == mode && m_isSteeringAlarmActive) {
                qDebug() << "9秒切换超时，强制关闭转向切换报警。当前模式:" << mode;
                m_isSwitchingSteeringMode = false;
                m_targetSteeringWaitBit = -1;
                m_isSteeringAlarmActive = false;
                hideAlarm();
            }
        });

    } else if (oldInGroupB) {
        // 从4/5 切到 1/2/3 -> 9秒延时
        qDebug() << "从模式4/5切换至1/2/3，启动9秒延时";
        
        m_isSteeringAlarmActive = true;
        showAlarm("正在更换底盘模式", "#FFFF00", false);
        
        QTimer::singleShot(9000, this, [this]() {
            // 如果在这9秒内又切换回了4或5（正在等待位信号），则不关闭报警
            if (m_isSwitchingSteeringMode) {
                qDebug() << "9秒延时结束，但当前处于位信号等待模式，不关闭报警";
                return;
            }
            qDebug() << "9秒延时结束，关闭报警";
            m_isSteeringAlarmActive = false;
            hideAlarm();
        });

    } else {
        // 1/2/3 互相切换 -> 不做任何操作，维持原状
        qDebug() << "转向切换在模式1,2,3之间，不改变当前报警状态";
    }
    // ================================================

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = "AGV控制";
    record.controlName = "steeringModeSelector";
    record.controlType = "SteeringModeSelector";
    record.operation = "steering_mode_changed";
    record.oldValue = "";
    record.newValue = QString("%1 (值:%2)").arg(m_steeringModeSelector->modeText(mode)).arg(modbusValue);
    m_recorder->addRecord(record);

    // 显示通知
    showNotification(QString("转向模式: %1").arg(m_steeringModeSelector->modeText(mode)));
}
// 新增：设置TCP传输UI
void MainWindow::setupTcpTransmissionUI()
{
    if (!isBigFeatureEnabled("tcp_transmission")) {
        return;
    }

    // 在状态栏添加连接状态指示器
    if (ui->statusBar) {
        // 创建中间的状态容器，用于放置时间、权限、运行模式
        QWidget *centerWidget = new QWidget(ui->statusBar);
        QHBoxLayout *centerLayout = new QHBoxLayout(centerWidget);
        centerLayout->setContentsMargins(0, 15, 0, 0); // 增加上边距，使文字整体下移
        centerLayout->setSpacing(15); // 紧凑排布
        centerLayout->setAlignment(Qt::AlignCenter); // 居中对齐

        // 时间日期标签
        QLabel *timeLabel = new QLabel(centerWidget);
        timeLabel->setObjectName("statusBarTimeLabel");
        timeLabel->setStyleSheet("color: #ffffff; font-family: 'Consolas'; font-size: 11px;");
        timeLabel->setFixedHeight(12); // 固定高度，与右侧 MAIN 指示灯 12px 一致
        centerLayout->addWidget(timeLabel);

        // 权限等级指示灯
        QWidget *roleWidget = new QWidget(centerWidget);
        QHBoxLayout *roleLayout = new QHBoxLayout(roleWidget);
        roleLayout->setContentsMargins(0,0,0,0);
        roleLayout->setSpacing(2);
        QLabel *roleLed = new QLabel("●", roleWidget);
        roleLed->setObjectName("statusBarRoleLed");
        roleLed->setStyleSheet("color: #aaaaaa; font-size: 12px;"); // 稍小一点的指示灯
        roleLed->setFixedHeight(12);
        QLabel *roleText = new QLabel("操作员", roleWidget);
        roleText->setObjectName("statusBarRoleText");
        roleText->setStyleSheet("color: #ffffff; font-size: 11px;");
        roleText->setFixedHeight(12);
        roleLayout->addWidget(roleLed);
        roleLayout->addWidget(roleText);
        centerLayout->addWidget(roleWidget);

        // 系统运行模式 (点动/步进)
        QLabel *runModeLabel = new QLabel(m_stepModeEnabled ? "步进模式" : "点动模式", centerWidget);
        runModeLabel->setObjectName("statusBarRunModeLabel");
        runModeLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                    .arg(m_stepModeEnabled ? "#00ff00" : "#00ccff"));
        runModeLabel->setFixedHeight(12);
        centerLayout->addWidget(runModeLabel);
        
        // 控制模式 (有线/无线)
        QLabel *controlModeLabel = new QLabel(m_controlMode == WIRED_MODE ? "有线控制" : "无线控制", centerWidget);
        controlModeLabel->setObjectName("statusBarControlModeLabel");
        controlModeLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;")
                                        .arg(m_controlMode == WIRED_MODE ? "#ffffff" : "#ffff00"));
        controlModeLabel->setFixedHeight(12);
        centerLayout->addWidget(controlModeLabel);

        centerWidget->setLayout(centerLayout);
        ui->statusBar->addPermanentWidget(centerWidget, 1); // 占据中间大部分空间
        centerLayout->addWidget(controlModeLabel);

        centerWidget->setLayout(centerLayout);
        ui->statusBar->addPermanentWidget(centerWidget, 1); // 占据中间大部分空间

        // 2. TCP, AGV 和 MAIN 状态指示器竖直排布并在右侧对齐
        QWidget *statusGroupWidget = new QWidget(ui->statusBar);
        QVBoxLayout *statusGroupLayout = new QVBoxLayout(statusGroupWidget);
        statusGroupLayout->setContentsMargins(0, 0, 10, 0);
        statusGroupLayout->setSpacing(0);
        statusGroupLayout->setAlignment(Qt::AlignRight); // 向右对齐

        auto createIndicator = [statusGroupWidget](const QString& objName, const QString& text) {
            QLabel *label = new QLabel(text, statusGroupWidget);
            label->setObjectName(objName);
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            label->setStyleSheet("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
            label->setFixedHeight(12);
            return label;
        };

        QLabel *tcpStatusIndicator = createIndicator("tcpStatusIndicator", "TCP: ●");
        tcpStatusIndicator->setToolTip("TCP连接状态");
        
        QLabel *agvModbusStatusIndicator = createIndicator("agvModbusStatusIndicator", "AGV: ●");
        agvModbusStatusIndicator->setToolTip("AGV Modbus连接状态");

        QLabel *mainModbusStatusIndicator = createIndicator("mainModbusStatusIndicator", "MAIN: ●");
        mainModbusStatusIndicator->setToolTip("主设备 Modbus连接状态");

        statusGroupLayout->addWidget(tcpStatusIndicator);
        statusGroupLayout->addWidget(agvModbusStatusIndicator);
        statusGroupLayout->addWidget(mainModbusStatusIndicator);
        
        statusGroupWidget->setLayout(statusGroupLayout);
        ui->statusBar->addPermanentWidget(statusGroupWidget);

        // 启动时间更新定时器
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateStatusBarTime);
        timer->start(1000);
        updateStatusBarTime(); // 立即执行一次

        // 初始化一次颜色
        if (m_modbusManager && m_modbusManager->isConnected()) {
            mainModbusStatusIndicator->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
            mainModbusStatusIndicator->setToolTip("主设备 Modbus连接正常");
        }
    }
}

void MainWindow::updateStatusBarTime()
{
    QLabel *timeLabel = ui->statusBar->findChild<QLabel*>("statusBarTimeLabel");
    if (timeLabel) {
        timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    }

    // 顺便在这里同步一下权限指示灯（因为它是全局变量变化的，没法通过信号捕捉）
    QLabel *roleLed = ui->statusBar->findChild<QLabel*>("statusBarRoleLed");
    QLabel *roleText = ui->statusBar->findChild<QLabel*>("statusBarRoleText");
    if (roleLed && roleText) {
        QString color = "#aaaaaa";
        QString text = "操作员";
        switch(m_currentUserRole) {
            case UserRole::Operator: color = "#aaaaaa"; text = "操作员"; break;
            case UserRole::Engineer: color = "#55aaff"; text = "工程师"; break;
            case UserRole::Admin: color = "#55ff55"; text = "管理员"; break;
            case UserRole::Manufacturer: color = "#ffaa00"; text = "厂家"; break;
        }
        roleLed->setStyleSheet(QString("color: %1; font-size: 14px;").arg(color));
        roleText->setText(text);
    }
}

// 新增：启用/禁用TCP传输
void MainWindow::enableTcpTransmission(bool enabled)
{
    if (!isBigFeatureEnabled("tcp_transmission")) {
        qDebug() << "TCP传输功能已关闭，忽略请求";
        return;
    }

    m_tcpTransmissionEnabled = enabled;

    if (m_recorder) {
        m_recorder->enableTcpTransmission(enabled);

        // 设置服务器地址
        m_recorder->setTcpServer(WIN7_IP, WIN7_PORT);

        if (enabled) {
            qDebug() << "启用TCP传输，服务器:" << WIN7_IP << ":" << WIN7_PORT;
            ui->statusBar->showMessage("TCP传输已启用，正在连接服务器...", 3000);
        } else {
            qDebug() << "禁用TCP传输";
            ui->statusBar->showMessage("TCP传输已禁用", 3000);
        }
    }
}

void MainWindow::updateTcpServerHost(const QString &hostSuffix)
{
    if (m_recorder) {
        QString newIp = "192.168.1." + hostSuffix;
        m_recorder->setTcpServer(newIp, WIN7_PORT);
        qDebug() << "更新TCP服务器IP为:" << newIp;
    }
}

void MainWindow::updateSimulatorHost(const QString &hostSuffix)
{
    QString newIp = "192.168.1." + hostSuffix;
    qDebug() << "尝试更新模拟器IP为:" << newIp;

    // 如果当前处于远程模拟器模式，则重新连接以使新 IP 生效
    if (isFeatureEnabled("tcp_transmission", "tcp.remote_simulator")) {
        if (m_modbusManager) {
            m_modbusManager->disconnectFromDevice();
            MainModbusConnector::connectAndConfigure(
                m_modbusManager,
                MainModbusEndpoint{newIp, 5020},
                m_mainModbusPollIntervalMs,
                m_mainReconnectIntervalMs);
            qDebug() << "[MainModbus] 已切换模拟器并重新连接:" << newIp << ":5020";
        }
        if (m_agvModbusManager) {
            m_agvModbusManager->disconnectFromDevice();
            m_agvModbusManager->connectToDevice(newIp, 5021);
            qDebug() << "[AGVModbus] 已切换模拟器并重新连接:" << newIp << ":5021";
        }
    } else {
        qDebug() << "当前未启用远程模拟器模式，仅打印新 IP 会在下次切换模式时生效";
    }
}

// 新增：TCP传输复选框槽函数
void MainWindow::onEnableTcpTransmission(bool checked)
{
    enableTcpTransmission(checked);
}

// 新增：发送所有记录槽函数
void MainWindow::onSendAllRecords()
{
    if (!isFeatureEnabled("tcp_transmission", "tcp.send_all")) {
        showNotification("TCP全量发送功能已关闭");
        return;
    }

    if (!m_tcpTransmissionEnabled) {
        QMessageBox::warning(this, "警告", "请先启用TCP传输");
        return;
    }

    if (!m_recorder->isTcpConnected()) {
        QMessageBox::warning(this, "警告", "TCP连接未建立，无法发送记录");
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认",
                                  QString("确定要发送所有 %1 条记录到服务器吗？").arg(m_recorder->recordCount()),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_recorder->sendAllRecordsToServer();
        ui->statusBar->showMessage("开始发送所有记录到服务器...", 3000);
    }
}

// 新增：TCP连接状态变化槽函数
void MainWindow::onTcpConnectionStatusChanged(bool connected)
{
    // 更新状态栏指示器
    QLabel *tcpStatusIndicator = ui->statusBar->findChild<QLabel*>("tcpStatusIndicator");
    if (tcpStatusIndicator) {
        if (connected) {
            tcpStatusIndicator->setText("TCP: ●");
            tcpStatusIndicator->setStyleSheet("color: #55ff55; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
            tcpStatusIndicator->setToolTip("TCP连接正常");
        } else {
            tcpStatusIndicator->setText("TCP: ●");
            tcpStatusIndicator->setStyleSheet("color: #ff5555; font-weight: bold; font-size: 10px; font-family: 'Consolas';");
            tcpStatusIndicator->setToolTip("TCP连接断开");
        }
    }

    // 更新记录页面状态标签
    QWidget *recordPage = ui->StackedWidget->widget(6);
    if (recordPage) {
        QLabel *tcpStatusLabel = recordPage->findChild<QLabel*>("tcpStatusLabel");
        if (tcpStatusLabel) {
            if (connected) {
                tcpStatusLabel->setText("TCP: 已连接");
                tcpStatusLabel->setStyleSheet("color: #55ff55; font-weight: bold;");
            } else {
                tcpStatusLabel->setText("TCP: 未连接");
                tcpStatusLabel->setStyleSheet("color: #ff5555; font-weight: bold;");
            }
        }
    }

    // 显示通知
    if (connected) {
        showNotification("TCP服务器连接成功");
    } else {
        showNotification("TCP服务器连接断开");
    }
}

// 新增：TCP传输完成槽函数
void MainWindow::onTcpTransmissionComplete()
{
    qDebug() << "所有记录已发送到TCP服务器";
    showNotification("所有记录已发送到服务器");
}

// 新增：TCP传输错误槽函数
void MainWindow::onTcpTransmissionError(const QString &error)
{
    // 在记录页面显示错误
    QWidget *recordPage = ui->StackedWidget->widget(6);
    if (recordPage) {
        QLabel *tcpStatusLabel = recordPage->findChild<QLabel*>("tcpStatusLabel");
        if (tcpStatusLabel) {
            tcpStatusLabel->setText("TCP: 错误");
            tcpStatusLabel->setStyleSheet("color: #ffaa00; font-weight: bold;");
            tcpStatusLabel->setToolTip(error);
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString notificationText = QString("TCP传输错误: %1").arg(error);
    const bool shouldNotify = (notificationText != m_lastTcpErrorNotification)
            || ((nowMs - m_lastTcpErrorNotificationMs) > 2000);

    if (shouldNotify) {
        qWarning() << "TCP传输错误:" << error;
        m_lastTcpErrorNotification = notificationText;
        m_lastTcpErrorNotificationMs = nowMs;
        showNotification(notificationText);
    } else {
        qDebug() << "TCP传输错误(已抑制重复):" << error;
    }
}
//六维力
// 在mainwindow.cpp中添加以下函数实现
void MainWindow::setupBigForceLabels()
{
    if (!isFeatureEnabled("force_sensor", "force.big_sensor")) {
        return;
    }

    // 查找六个大六维力标签
    m_labelBigFX = findChild<QLabel*>("labelBigFX");
    m_labelBigFY = findChild<QLabel*>("labelBigFY");
    m_labelBigFZ = findChild<QLabel*>("labelBigFZ");
    m_labelBigMX = findChild<QLabel*>("labelBigMX");
    m_labelBigMY = findChild<QLabel*>("labelBigMY");
    m_labelBigMZ = findChild<QLabel*>("labelBigMZ");

    // 创建标签名称到控件指针的映射
    if (m_labelBigFX) {
        m_bigForceLabels["FX"] = m_labelBigFX;
        m_labelBigFX->setText("原始值FX: 0.00");
        m_labelBigFX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigFX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigFY) {
        m_bigForceLabels["FY"] = m_labelBigFY;
        m_labelBigFY->setText("原始值FY: 0.00");
        m_labelBigFY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigFY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigFZ) {
        m_bigForceLabels["FZ"] = m_labelBigFZ;
        m_labelBigFZ->setText("原始值FZ: 0.00");
        m_labelBigFZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigFZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigMX) {
        m_bigForceLabels["MX"] = m_labelBigMX;
        m_labelBigMX->setText("原始值MX: 0.00");
        m_labelBigMX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigMX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigMY) {
        m_bigForceLabels["MY"] = m_labelBigMY;
        m_labelBigMY->setText("原始值MY: 0.00");
        m_labelBigMY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigMY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    if (m_labelBigMZ) {
        m_bigForceLabels["MZ"] = m_labelBigMZ;
        m_labelBigMZ->setText("原始值MZ: 0.00");
        m_labelBigMZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelBigMZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 120px;"
            "}"
            );
    }

    qDebug() << "大六维力标签初始化完成，找到" << m_bigForceLabels.size() << "个标签";
}
void MainWindow::setupSmallForceLabels()
{
    if (!isFeatureEnabled("force_sensor", "force.small_sensor")) {
        return;
    }

    // 查找六个小六维力标签
    m_labelSmallFX = findChild<QLabel*>("labelSmallFX");
    m_labelSmallFY = findChild<QLabel*>("labelSmallFY");
    m_labelSmallFZ = findChild<QLabel*>("labelSmallFZ");
    m_labelSmallMX = findChild<QLabel*>("labelSmallMX");
    m_labelSmallMY = findChild<QLabel*>("labelSmallMY");
    m_labelSmallMZ = findChild<QLabel*>("labelSmallMZ");

    // 创建标签名称到控件指针的映射
    if (m_labelSmallFX) {
        m_smallForceLabels["FX"] = m_labelSmallFX;
        m_labelSmallFX->setText("原始值FX: 0.00");
        m_labelSmallFX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallFX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallFY) {
        m_smallForceLabels["FY"] = m_labelSmallFY;
        m_labelSmallFY->setText("原始值FY: 0.00");
        m_labelSmallFY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallFY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallFZ) {
        m_smallForceLabels["FZ"] = m_labelSmallFZ;
        m_labelSmallFZ->setText("原始值FZ: 0.00");
        m_labelSmallFZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallFZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallMX) {
        m_smallForceLabels["MX"] = m_labelSmallMX;
        m_labelSmallMX->setText("原始值MX: 0.00");
        m_labelSmallMX->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallMX->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallMY) {
        m_smallForceLabels["MY"] = m_labelSmallMY;
        m_labelSmallMY->setText("原始值MY: 0.00");
        m_labelSmallMY->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallMY->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    if (m_labelSmallMZ) {
        m_smallForceLabels["MZ"] = m_labelSmallMZ;
        m_labelSmallMZ->setText("原始值MZ: 0.00");
        m_labelSmallMZ->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 左对齐
        m_labelSmallMZ->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"  // 统一字体大小为12px
            "    font-weight: bold;"
            "    padding: 3px 8px;"
            "    min-width: 100px;"
            "}"
            );
    }

    qDebug() << "小六维力标签初始化完成，找到" << m_smallForceLabels.size() << "个标签";
}


void MainWindow::setupForceReading()
{
    // 已根据要求删除六维力传感器轮询功能，以减少主 Modbus 队列负载
    qDebug() << "六维力传感器轮询功能已禁用";
    return;
}

void MainWindow::setupBigForceReading()
{
    // 立即读取一次
    readBigForceRegisters();

    // 每500毫秒读取一次（与原有浮点数读取保持相同频率）
    QTimer::singleShot(500, this, [this]() {
        if (m_modbusManager && m_modbusManager->isConnected()) {
            readBigForceRegisters();
            // 继续定时读取
            QTimer::singleShot(500, this, [this]() { setupBigForceReading(); });
        } else {
            // Modbus未连接，等待2秒后重试
            QTimer::singleShot(2000, this, [this]() { setupBigForceReading(); });
        }
    });
}
void MainWindow::readBigForceRegisters()
{
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        qDebug() << "Modbus未连接，无法读取大六维力数据";
        return;
    }

    // 读取612-623地址（12个寄存器，6个浮点数）
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 612, 12);
}
void MainWindow::readSmallForceRegisters()
{
    if (!MainDeviceModbusApi::isReady(m_modbusManager)) {
        qDebug() << "Modbus未连接，无法读取小六维力数据";
        return;
    }

    // 读取624-635地址（12个寄存器，6个浮点数）
    MainDeviceModbusApi::readHoldingRegisters(m_modbusManager, 624, 12);
}
void MainWindow::updateBigForceLabel(const QString& labelName, float value)
{
    if (!m_bigForceLabels.contains(labelName)) {
        return;
    }

    // 存储当前原始值
    m_bigForceCurrentValues[labelName] = value;

    QLabel* label = m_bigForceLabels[labelName];
    if (!label) {
        return;
    }

    // 根据去皮标志计算显示值
    float displayValue = value;
    QString prefix = "原始值";

    if (m_isForcePeeled && m_bigForceOffsets.contains(labelName)) {
        float offset = m_bigForceOffsets[labelName];
        displayValue = value - offset;  // 显示值 = 当前值 - 基准值
        prefix = "去皮值";

        // 调试输出
        static QMap<QString, int> debugCounters;
        int count = debugCounters.value(labelName, 0);
        if (count++ % 10 == 0) {  // 每10次输出一次，避免日志过多
            qDebug() << "大" << labelName << ": 原始=" << value
                     << ", 基准=" << offset
                     << ", 显示=" << displayValue;
            debugCounters[labelName] = count;
        }
    }

    // 格式化显示（保留2位小数），保持左对齐
    QString displayText = QString("%1%2: %3")
        .arg(prefix)
        .arg(labelName)
        .arg(displayValue, 0, 'f', 2);

    label->setText(displayText);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 根据数值大小设置颜色（可选）
    if (m_isForcePeeled && qAbs(displayValue) > 10.0) {
        label->setStyleSheet("color: #ff5555; font-weight: bold;");
    } else {
        label->setStyleSheet("color: #ffffff;");
    }
}

void MainWindow::updateSmallForceLabel(const QString& labelName, float value)
{
    if (!m_smallForceLabels.contains(labelName)) {
        return;
    }

    // 存储当前原始值
    m_smallForceCurrentValues[labelName] = value;

    QLabel* label = m_smallForceLabels[labelName];
    if (!label) {
        return;
    }

    // 根据去皮标志计算显示值
    float displayValue = value;
    QString prefix = "原始值";

    if (m_isForcePeeled && m_smallForceOffsets.contains(labelName)) {
        float offset = m_smallForceOffsets[labelName];
        displayValue = value - offset;  // 显示值 = 当前值 - 基准值
        prefix = "去皮值";

        // 调试输出
        static QMap<QString, int> debugCounters;
        int count = debugCounters.value(labelName, 0);
        if (count++ % 10 == 0) {  // 每10次输出一次，避免日志过多
            qDebug() << "小" << labelName << ": 原始=" << value
                     << ", 基准=" << offset
                     << ", 显示=" << displayValue;
            debugCounters[labelName] = count;
        }
    }

    // 格式化显示（保留2位小数），保持左对齐
    QString displayText = QString("%1%2: %3")
        .arg(prefix)
        .arg(labelName)
        .arg(displayValue, 0, 'f', 2);

    label->setText(displayText);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 根据数值大小设置颜色（可选）
    if (m_isForcePeeled && qAbs(displayValue) > 5.0) {
        label->setStyleSheet("color: #ff5555; font-weight: bold;");
    } else {
        label->setStyleSheet("color: #ffffff;");
    }
}

void MainWindow::setupForceDisplayModeButtons()
{
    if (!isBigFeatureEnabled("force_sensor")) {
        return;
    }

    m_btnBigForceControl = findChild<TechPushButton*>("Btn_bigForceControl");
    m_btnSmallForceControl = findChild<TechPushButton*>("Btn_smallForceControl");

    if (!m_btnBigForceControl || !m_btnSmallForceControl) {
        qWarning() << "未找到Btn_bigForceControl或Btn_smallForceControl按钮";
        return;
    }

    m_btnBigForceControl->enableClickAnimation(true);
    m_btnSmallForceControl->enableClickAnimation(true);
    m_btnBigForceControl->enableHoverAnimation(true);
    m_btnSmallForceControl->enableHoverAnimation(true);
    m_btnBigForceControl->setTextGlow(true);
    m_btnSmallForceControl->setTextGlow(true);

    m_btnBigForceControl->setButtonStyle(TechPushButton::StyleHolographic);
    m_btnSmallForceControl->setButtonStyle(TechPushButton::StyleHolographic);

    connect(m_btnBigForceControl, &TechPushButton::clicked, this, [this]() {
        setForceDisplayMode(ForceDisplayBig);
    });
    connect(m_btnSmallForceControl, &TechPushButton::clicked, this, [this]() {
        setForceDisplayMode(ForceDisplaySmall);
    });

    setForceDisplayMode(m_forceDisplayMode);
    qDebug() << "大/小六维力模式按钮初始化完成";
}

void MainWindow::setForceDisplayMode(ForceDisplayMode mode)
{
    if (!m_btnBigForceControl || !m_btnSmallForceControl) {
        return;
    }

    m_forceDisplayMode = mode;

    const QColor activeColor(0, 200, 255);
    const QColor inactiveColor(80, 80, 100);
    const QColor inactiveTextColor(200, 200, 200);

    // 发送Modbus指令 (新增)
    if (mode == ForceDisplayBig) {
        writeToMainDevice(404, 0); // 大力模式：写0
        qDebug() << "切换至大力传感器模式，地址404写入0";
    } else {
        writeToMainDevice(404, 1); // 小力模式：写1
        qDebug() << "切换至小力传感器模式，地址404写入1";
    }

    // 先全部设为非激活
    m_btnBigForceControl->setPrimaryColor(inactiveColor);
    m_btnBigForceControl->setGlowColor(inactiveColor);
    m_btnBigForceControl->setTextColor(inactiveTextColor);
    m_btnBigForceControl->enablePulseEffect(false);

    m_btnSmallForceControl->setPrimaryColor(inactiveColor);
    m_btnSmallForceControl->setGlowColor(inactiveColor);
    m_btnSmallForceControl->setTextColor(inactiveTextColor);
    m_btnSmallForceControl->enablePulseEffect(false);

    // 激活当前模式
    if (mode == ForceDisplayBig) {
        m_btnBigForceControl->setPrimaryColor(activeColor);
        m_btnBigForceControl->setGlowColor(activeColor.lighter(150));
        m_btnBigForceControl->setTextColor(Qt::white);
        // m_btnBigForceControl->enablePulseEffect(true);
    } else {
        m_btnSmallForceControl->setPrimaryColor(activeColor);
        m_btnSmallForceControl->setGlowColor(activeColor.lighter(150));
        m_btnSmallForceControl->setTextColor(Qt::white);
        // m_btnSmallForceControl->enablePulseEffect(true);
    }

    m_btnBigForceControl->update();
    m_btnSmallForceControl->update();
}

void MainWindow::setupForceClearButton()
{
    if (!isFeatureEnabled("force_sensor", "force.clear_zero")) {
        return;
    }

    m_btnForceClear = findChild<QPushButton*>("btn_ForceClear");
    if (m_btnForceClear) {
        // 连接按下和释放信号
        connect(m_btnForceClear, &QPushButton::pressed, this, &MainWindow::onForceClearPressed);
        connect(m_btnForceClear, &QPushButton::released, this, &MainWindow::onForceClearReleased);

        // 设置简化按钮样式
        m_btnForceClear->setStyleSheet(
            "QPushButton {"
            "    padding: 10px;"
            "    font-size: 14px;"
            "}"
            );

        qDebug() << "去皮按钮初始化完成";
    } else {
        qWarning() << "未找到btn_ForceClear按钮";
    }
}

void MainWindow::onForceClearPressed()
{
    qDebug() << "去皮按钮按下";

    // 获取当前页面名称（用于操作记录）
    QString pageName = getCurrentPageName();

    // 记录去皮前的值（用于操作记录）
    QMap<QString, float> beforeValues;

    // 1. 只记录大六维力的当前值作为基准值
    qDebug() << "=== 记录大六维力基准值 ===";
    for (auto it = m_bigForceCurrentValues.begin(); it != m_bigForceCurrentValues.end(); ++it) {
        QString labelName = it.key();
        float currentValue = it.value();

        beforeValues[QString("大%1").arg(labelName)] = currentValue;
        m_bigForceOffsets[labelName] = currentValue;

        qDebug() << "大六维力 " << labelName << " 基准值: " << currentValue;
    }

    // 2. 只记录小六维力的当前值作为基准值
    qDebug() << "=== 记录小六维力基准值 ===";
    for (auto it = m_smallForceCurrentValues.begin(); it != m_smallForceCurrentValues.end(); ++it) {
        QString labelName = it.key();
        float currentValue = it.value();

        beforeValues[QString("小%1").arg(labelName)] = currentValue;
        m_smallForceOffsets[labelName] = currentValue;

        qDebug() << "小六维力 " << labelName << " 基准值: " << currentValue;
    }

    // 3. 设置去皮标志（注意：不在按钮按下时立即更新显示）
    m_isForcePeeled = true;

    // 4. 给192.168.1.13设备的401地址写1
    writeToMainDevice(401, 1);

    // 5. 记录操作到历史记录
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = pageName;
    record.controlName = "去皮按钮";
    record.controlType = "ForceClear";
    record.operation = "force_clear_pressed";
    record.oldValue = "未去皮";

    // 构建详细的值字符串
    QString valuesStr = "大六维力基准值: ";
    for (auto it = beforeValues.begin(); it != beforeValues.end(); ++it) {
        if (it.key().startsWith("大")) {
            valuesStr += QString("%1=%2; ").arg(it.key()).arg(it.value(), 0, 'f', 2);
        }
    }
    valuesStr += "小六维力基准值: ";
    for (auto it = beforeValues.begin(); it != beforeValues.end(); ++it) {
        if (it.key().startsWith("小")) {
            valuesStr += QString("%1=%2; ").arg(it.key()).arg(it.value(), 0, 'f', 2);
        }
    }

    record.newValue = valuesStr;
    m_recorder->addRecord(record);

    // 6. 显示通知（但不要立即更新标签显示）
    showNotification("已记录当前值为基准值，后续显示将自动减去基准值");

    qDebug() << "去皮操作完成：已记录基准值，去皮标志设为true";
}
void MainWindow::onForceClearReleased()
{
    qDebug() << "去皮按钮释放";

    // 给192.168.1.13设备的401地址写0
    writeToMainDevice(401, 0);

    // 注意：我们不在这里清除去皮标志，因为去皮状态应该保持
    // 如果需要取消去皮，可以添加专门的取消去皮功能
}

void MainWindow::toggleForceControl()
{
    if (!isFeatureEnabled("motion_control", "motion.force_control")) {
        showNotification("力控功能已关闭");
        return;
    }

    // 切换状态
        m_forcecontrolMode = !m_forcecontrolMode;

        if (m_forcecontrolMode) {
            // 力控开启
            m_btnForceControl->setText(m_isForcePeeled ? "力控开启(去皮)" : "力控开启");
            m_btnForceControl->setPrimaryColor(QColor("#00C8FF"));
            m_btnForceControl->setGlowColor(QColor(0, 200, 255, 180));
            writeToMainDevice(400, 1);
            qDebug() << "力控模式：开启，地址400写入1";
            ui->statusBar->showMessage("力控开启模式已启用", 2000);
        } else {
            // 力控关闭
            m_btnForceControl->setText(m_isForcePeeled ? "力控关闭(去皮)" : "力控关闭");
            m_btnForceControl->setPrimaryColor(QColor("#7F8C8D"));
            m_btnForceControl->setGlowColor(QColor(127, 140, 141, 100));
            writeToMainDevice(400, 0);
            qDebug() << "力控模式：关闭，地址400写入0";
            ui->statusBar->showMessage("力控关闭模式已启用", 2000);
        }

    // 更新所有 TechSliderLabel 的力控状态
    QList<TechSliderLabel*> allSliderLabels = this->findChildren<TechSliderLabel*>();
    for (TechSliderLabel *label : allSliderLabels) {
        label->setForceControlMode(m_forcecontrolMode);
    }

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "btn_ForceControl";
    record.controlType = "TechPushButton";
    record.operation = "force_control_toggled";
    record.oldValue = !m_forcecontrolMode;
    record.newValue = m_forcecontrolMode;
    m_recorder->addRecord(record);

    // 立即检查报警条件（因为力控状态变化可能影响报警显示）
    checkAlarmConditions();

    qDebug() << "力控按钮点击，新状态:" << (m_forcecontrolMode ? "开启" : "关闭");
}

// void MainWindow::on_TBtn_Stepmove_clicked()
// {
//      writeToMainDevice(5,2);
//     // writeToMainDevice(402,1);

//     writeToMainDevice(19,1);

//     writeToMainDevice(44,1);
//     writeToMainDevice(10,1);


// }


// void MainWindow::on_Btn_SwitchAGV_2_clicked()
// {
//      writeToMainDevice(5,1);
//     // writeToMainDevice(402,0);

//     writeToMainDevice(19,0);
//     writeToMainDevice(10,0);

// }
// 添加步进/点动模式切换函数
void MainWindow::onStepMoveButtonClicked()
{
    if (!isFeatureEnabled("motion_control", "motion.step_mode")) {
        showNotification("步进模式功能已关闭");
        return;
    }

    // 切换模式
    m_stepModeEnabled = !m_stepModeEnabled;

    if (m_stepModeEnabled) {
        // 切换到步进模式
        ui->TBtn_Stepmove->setText("步进模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：步进模式");

        // 给501寄存器写入2
        writeToMainDevice(501, 2);

        qDebug() << "切换到步进模式，地址501写入2";
        ui->statusBar->showMessage("已切换到步进模式", 2000);

        // 更新状态栏显示
        QLabel *runModeLabel = ui->statusBar->findChild<QLabel*>("statusBarRunModeLabel");
        if (runModeLabel) {
            runModeLabel->setText("步进模式");
            runModeLabel->setStyleSheet("color: #00ff00; font-weight: bold; font-size: 11px;");
        }

    } else {
        // 切换到点动模式
        ui->TBtn_Stepmove->setText("点动模式");
        ui->TBtn_Stepmove->setToolTip("当前模式：点动模式");

        // 更新状态栏显示
        QLabel *runModeLabel = ui->statusBar->findChild<QLabel*>("statusBarRunModeLabel");
        if (runModeLabel) {
            runModeLabel->setText("点动模式");
            runModeLabel->setStyleSheet("color: #00ccff; font-weight: bold; font-size: 11px;");
        }

        // 给501寄存器写入1
        writeToMainDevice(501, 1);

        qDebug() << "切换到点动模式，地址501写入1";
        ui->statusBar->showMessage("已切换到点动模式", 2000);
    }

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "TBtn_Stepmove";
    record.controlType = "QToolButton";
    record.operation = "step_mode_changed";
    record.oldValue = m_stepModeEnabled ? "点动模式" : "步进模式";
    record.newValue = m_stepModeEnabled ? "步进模式" : "点动模式";
    m_recorder->addRecord(record);
}

// 步进模式下使能按钮按下
void MainWindow::onEnableButtonPressedStepMode()
{
    qDebug() << "步进模式下使能按钮按下";

    // 给192.168.1.13设备的19地址写1，10地址写1
    // writeToMainDevice(19, 1);
    writeToMainDevice(10, 1);

    // 写入步进值到寄存器
    writeStepMoveRegisters();

    // 检查各个输入框是否有内容，有则记录历史记录
    if (m_editJ1MoveStep && !m_editJ1MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value1"); // J1当前角度
        QString stepValue = m_editJ1MoveStep->text();
        recordStepMoveAction("悬臂组件(J1)", currentAngle, stepValue, true);
    }

    if (m_editJ2MoveStep && !m_editJ2MoveStep->text().isEmpty()) {
        double currentHeight = getSliderLabelValue("label_Value2"); // J2当前高度
        QString stepValue = m_editJ2MoveStep->text();
        recordStepMoveAction("升降组件(J2)", currentHeight, stepValue, true);
    }

    if (m_editJ3MoveStep && !m_editJ3MoveStep->text().isEmpty()) {
        double currentLength = getSliderLabelValue("label_Value3"); // J3当前长度
        QString stepValue = m_editJ3MoveStep->text();
        recordStepMoveAction("伸缩臂(J3)", currentLength, stepValue, true);
    }

    if (m_editJ4MoveStep && !m_editJ4MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value4"); // J4当前角度
        QString stepValue = m_editJ4MoveStep->text();
        recordStepMoveAction("柔顺组件(J4)", currentAngle, stepValue, true);
    }

    ui->statusBar->showMessage("步进模式：使能按钮按下，开始步进运动", 2000);
}

// 步进模式下使能按钮释放
void MainWindow::onEnableButtonReleasedStepMode()
{
    qDebug() << "步进模式下使能按钮释放";

    // 给192.168.1.13设备的19地址写0，10地址写0，500-503地址写0
    // writeToMainDevice(19, 0);
    writeToMainDevice(10, 0);

    // 清空步进寄存器
    clearStepMoveRegisters();

    // 检查各个输入框是否有内容，有则记录历史记录
    if (m_editJ1MoveStep && !m_editJ1MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value1"); // J1当前角度
        recordStepMoveEnd("悬臂组件(J1)", currentAngle);
    }

    if (m_editJ2MoveStep && !m_editJ2MoveStep->text().isEmpty()) {
        double currentHeight = getSliderLabelValue("label_Value2"); // J2当前高度
        recordStepMoveEnd("升降组件(J2)", currentHeight);
    }

    if (m_editJ3MoveStep && !m_editJ3MoveStep->text().isEmpty()) {
        double currentLength = getSliderLabelValue("label_Value3"); // J3当前长度
        recordStepMoveEnd("伸缩臂(J3)", currentLength);
    }

    if (m_editJ4MoveStep && !m_editJ4MoveStep->text().isEmpty()) {
        double currentAngle = getSliderLabelValue("label_Value4"); // J4当前角度
        recordStepMoveEnd("柔顺组件(J4)", currentAngle);
    }

    ui->statusBar->showMessage("步进模式：使能按钮释放，步进结束", 2000);
}

// 设置步进模式控制
void MainWindow::setupStepMoveControl()
{
    if (!isFeatureEnabled("motion_control", "motion.step_mode")) {
        return;
    }

    m_btnStepMove = findChild<QToolButton*>("TBtn_Stepmove");
    if (m_btnStepMove) {
        // 设置初始状态为点动模式
        m_btnStepMove->setText("点动模式");
        m_btnStepMove->setToolTip("当前模式：点动模式");

        // 样式设置
        m_btnStepMove->setStyleSheet(
            "QToolButton {"
            "    background-color: #3498DB;"
            "    color: white;"
            "    border: 2px solid #2980B9;"
            "    border-radius: 8px;"
            "    padding: 8px 16px;"
            "    font-weight: bold;"
            "    font-size: 14px;"
            "}"
            "QToolButton:hover {"
            "    background-color: #4A9EFF;"
            "}"
            "QToolButton:pressed {"
            "    background-color: #1A5FB4;"
            "}"
            );

        qDebug() << "步进/点动模式按钮初始化完成";
    } else {
        qWarning() << "未找到TBtn_Stepmove按钮";
    }
}

// 设置步进值输入框
void MainWindow::setupStepMoveLineEdits()
{
    if (!isFeatureEnabled("motion_control", "motion.step_mode")) {
        return;
    }

    // 查找四个步进值输入框
    m_editJ1MoveStep = findChild<QLineEdit*>("LEdit_HoriSupSec_J1MoveStep");
    m_editJ2MoveStep = findChild<QLineEdit*>("LEdit_VeSupSec_J2MoveStep");
    m_editJ3MoveStep = findChild<QLineEdit*>("LEdit_HoriSupSec_J3MoveStep");
    m_editJ4MoveStep = findChild<QLineEdit*>("LEdit_EOAT_J4MoveStep");

    // 设置验证器，允许整数（可正可负）
    QRegularExpression regExp("^-?\\d+$");  // 匹配整数，可正可负
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regExp, this);

    if (m_editJ1MoveStep) {
        m_editJ1MoveStep->setValidator(validator);
        m_editJ1MoveStep->setPlaceholderText("输入J1步进值(整数)");
        connect(m_editJ1MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ1MoveStepChanged);
    }

    if (m_editJ2MoveStep) {
        m_editJ2MoveStep->setValidator(validator);
        m_editJ2MoveStep->setPlaceholderText("输入J2步进值(整数)");
        connect(m_editJ2MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ2MoveStepChanged);
    }

    if (m_editJ3MoveStep) {
        m_editJ3MoveStep->setValidator(validator);
        m_editJ3MoveStep->setPlaceholderText("输入J3步进值(整数)");
        connect(m_editJ3MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ3MoveStepChanged);
    }

    if (m_editJ4MoveStep) {
        m_editJ4MoveStep->setValidator(validator);
        m_editJ4MoveStep->setPlaceholderText("输入J4步进值(整数)");
        connect(m_editJ4MoveStep, &QLineEdit::textChanged,
                this, &MainWindow::onJ4MoveStepChanged);
    }

    qDebug() << "步进值输入框初始化完成";
}

// J1步进值变化
void MainWindow::onJ1MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入500地址
            writeToMainDevice(500, value);
            qDebug() << "J1步进值:" << value << "，写入地址500";
        }
    }
}

// J2步进值变化
void MainWindow::onJ2MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入501地址
            writeToMainDevice(501, value);
            qDebug() << "J2步进值:" << value << "，写入地址501";
        }
    }
}

// J3步进值变化
void MainWindow::onJ3MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入502地址
            writeToMainDevice(502, value);
            qDebug() << "J3步进值:" << value << "，写入地址502";
        }
    }
}

// J4步进值变化
void MainWindow::onJ4MoveStepChanged(const QString &text)
{
    if (!text.isEmpty()) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            // 写入503地址
            writeToMainDevice(503, value);
            qDebug() << "J4步进值:" << value << "，写入地址503";
        }
    }
}

// 写入步进寄存器
void MainWindow::writeStepMoveRegisters()
{
    // 将当前输入框的值写入对应的寄存器
    if (m_editJ1MoveStep && !m_editJ1MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ1MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(500, value);
        }
    }

    if (m_editJ2MoveStep && !m_editJ2MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ2MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(501, value);
        }
    }

    if (m_editJ3MoveStep && !m_editJ3MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ3MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(502, value);
        }
    }

    if (m_editJ4MoveStep && !m_editJ4MoveStep->text().isEmpty()) {
        bool ok;
        int value = m_editJ4MoveStep->text().toInt(&ok);
        if (ok) {
            writeToMainDevice(503, value);
        }
    }
}

// 清空步进寄存器
void MainWindow::clearStepMoveRegisters()
{
    // 清空500-503地址
    for (int i = 500; i <= 503; i++) {
        writeToMainDevice(i, 0);
    }

    // 清空输入框内容
    if (m_editJ1MoveStep) m_editJ1MoveStep->clear();
    if (m_editJ2MoveStep) m_editJ2MoveStep->clear();
    if (m_editJ3MoveStep) m_editJ3MoveStep->clear();
    if (m_editJ4MoveStep) m_editJ4MoveStep->clear();

    qDebug() << "已清空步进寄存器(500-503)和输入框内容";
}

// 记录步进动作开始
void MainWindow::recordStepMoveAction(const QString &jointName, double currentValue, const QString &stepValue, bool start)
{
    Q_UNUSED(start);
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "StepMove_" + jointName;
    record.controlType = "StepMove";
    record.operation = "step_move_start";
    record.oldValue = "";
    QString msg;
    if (jointName.contains("J1")) {
        msg = QString("悬臂组件当前角度为%1°，开始步进%2°").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J2")) {
        msg = QString("升降组件当前高度为%1mm，开始步进%2mm").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J3")) {
        msg = QString("伸缩臂当前长度为%1mm，开始步进%2mm").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else if (jointName.contains("J4")) {
        msg = QString("末端组件当前角度为%1°，开始步进%2°").arg(currentValue, 0, 'f', 1).arg(stepValue);
    } else {
        msg = QString("%1当前%2为%3，步进%4°")
                  .arg(jointName)
                  .arg(jointName.contains("角度") ? "角度" :
                           jointName.contains("高度") ? "高度" : "长度")
                  .arg(currentValue, 0, 'f', 1)
                  .arg(stepValue);
    }
    record.newValue = msg;

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}

// 记录步进动作结束
void MainWindow::recordStepMoveEnd(const QString &jointName, double currentValue)
{
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "StepMove_" + jointName;
    record.controlType = "StepMove";
    record.operation = "step_move_end";
    record.oldValue = "";
    QString msg;
    if (jointName.contains("J1")) {
        msg = QString("悬臂组件当前角度为%1°，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J2")) {
        msg = QString("升降组件当前高度为%1mm，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J3")) {
        msg = QString("伸缩臂当前长度为%1mm，步进结束").arg(currentValue, 0, 'f', 1);
    } else if (jointName.contains("J4")) {
        msg = QString("末端组件当前角度为%1°，步进结束").arg(currentValue, 0, 'f', 1);
    } else {
        msg = QString("%1当前%2为%3，步进结束")
                  .arg(jointName)
                  .arg(jointName.contains("角度") ? "角度" :
                           jointName.contains("高度") ? "高度" : "长度")
                  .arg(currentValue, 0, 'f', 1);
    }
    record.newValue = msg;

    m_recorder->addRecord(record);
    showNotification(record.newValue.toString());
}





// 修改：原有的清除报警按钮函数
void MainWindow::on_TBtn_RemoveWarning_clicked()
{
    qDebug() << "用户点击清除报警按钮";

    // 写入29地址清除报警
    writeToMainDevice(29, 1);
    
    // 清除力控超限错误，给403写0（写到192.168.1.13）
    ModbusThreadManager::instance()->writeSingleRegister(403, 0);

    // 清除所有报警状态
    if (m_emergencyStopAlarm) {
        m_emergencyStopAlarm = false;
        qDebug() << "用户清除了紧急停止报警";
    }

    if (m_forceLimitAlarm) {
        m_forceLimitAlarm = false;
        qDebug() << "用户清除了力控超限报警";
    }

    // 更新报警显示
    updateAlarmDisplay();

    // 显示通知
    showNotification("报警已清除");

    // 记录操作
    OperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.pageName = getCurrentPageName();
    record.controlName = "TBtn_RemoveWarning";
    record.controlType = "QToolButton";
    record.operation = "clear_alarm";
    record.oldValue = "";
    record.newValue = "用户清除报警";
    m_recorder->addRecord(record);
}
// 新增：设置报警系统
void MainWindow::setupAlarmSystem()
{
    if (!isBigFeatureEnabled("alarm_system")) {
        qDebug() << "报警系统已关闭，跳过初始化";
        return;
    }

    qDebug() << "初始化报警系统...";

    // 初始化报警状态
    m_emergencyStopAlarm = false;
    m_forceLimitAlarm = false;
    m_emergencyStopColumnFlag = false;
    m_emergencyStopChassisFlag = false;
    m_forceLimitFlag = false;

    // 创建报警检测定时器
    if (!m_alarmCheckTimer) {
        m_alarmCheckTimer = new QTimer(this);
        connect(m_alarmCheckTimer, &QTimer::timeout, this, &MainWindow::checkAlarmConditions);
        m_alarmCheckTimer->start(500);  // 每500ms检查一次，更频繁
    }

    qDebug() << "报警系统初始化完成，定时器已启动";
}
// 新增：检查报警条件
void MainWindow::checkAlarmConditions()
{
    if (!isBigFeatureEnabled("alarm_system")) {
        return;
    }

    // 调试输出当前报警状态
    qDebug() << "=== 检查报警条件 ===";
    qDebug() << "立柱急停标志:" << m_emergencyStopColumnFlag;
    qDebug() << "底盘急停标志:" << m_emergencyStopChassisFlag;
    qDebug() << "力控超限标志:" << m_forceLimitFlag;
    qDebug() << "紧急停止报警:" << m_emergencyStopAlarm;
    qDebug() << "力控超限报警:" << m_forceLimitAlarm;

    // 1. 检查紧急停止报警（804/805地址）
    bool newEstopState = false;
    if (isFeatureEnabled("alarm_system", "alarm.emergency_stop")) {
        newEstopState = (m_emergencyStopColumnFlag || m_emergencyStopChassisFlag);
    }
    if (newEstopState != m_emergencyStopAlarm) {
        if (newEstopState) {
            qDebug() << "!!! 触发紧急停止报警 !!!";
        } else {
            qDebug() << "解除紧急停止报警";
        }
        m_emergencyStopAlarm = newEstopState;
    }

    // 2. 检查力控超限报警（403地址）
    // 只要力控超限标志位为1就触发报警
    bool newForceLimitState = false;
    if (isFeatureEnabled("alarm_system", "alarm.force_limit")) {
        newForceLimitState = m_forceLimitFlag;
    }

    if (newForceLimitState != m_forceLimitAlarm) {
        if (m_forceLimitFlag) {
            qDebug() << "!!! 触发力控超限报警 !!!";
        } else {
            qDebug() << "解除力控超限报警";
        }
        m_forceLimitAlarm = newForceLimitState;
    }

    // 3. 统一更新显示 (使用updateAlarmDisplay处理优先级和内容)
    updateAlarmDisplay();

    qDebug() << "=== 报警检查结束 ===";
}

// 新增：显示报警
void MainWindow::showAlarm(const QString &message, const QString &color, bool closable)
{
    if (!isFeatureEnabled("alarm_system", "alarm.popup")) {
        return;
    }

    qDebug() << "showAlarm被调用，消息:" << message << "颜色:" << color << "可关闭:" << closable;

    // 确保在主线程中执行
    if (QThread::currentThread() != this->thread()) {
        qDebug() << "showAlarm不在主线程，将使用invokeMethod";
        QMetaObject::invokeMethod(this, "showAlarm",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, message),
                                  Q_ARG(QString, color),
                                  Q_ARG(bool, closable));
        return;
    }

    // 如果报警窗口不存在，则创建它
    if (!m_alarmWidget) {
        qDebug() << "创建新的报警窗口...";

        // 创建报警窗口
        m_alarmWidget = new QWidget(nullptr);  // 使用nullptr作为父窗口，使其独立显示
        m_alarmWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                                      Qt::WindowStaysOnTopHint | Qt::Tool);
        m_alarmWidget->setObjectName("alarmWidget");

        // 创建布局
        QVBoxLayout *layout = new QVBoxLayout(m_alarmWidget);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        // 创建报警信息标签
        m_alarmLabel = new QLabel(m_alarmWidget);
        m_alarmLabel->setAlignment(Qt::AlignCenter);
        m_alarmLabel->setWordWrap(true);
        layout->addWidget(m_alarmLabel);

        // 创建清除报警按钮
        QPushButton *clearBtn = new QPushButton("清除报警", m_alarmWidget);
        clearBtn->setObjectName("clearAlarmBtn");
        layout->addWidget(clearBtn);

        connect(clearBtn, &QPushButton::clicked, this, &MainWindow::on_TBtn_RemoveWarning_clicked);

        // 设置固定大小
        m_alarmWidget->setFixedSize(350, 120);

        qDebug() << "报警窗口创建完成";
    }
    
    // 控制清除按钮的显示/隐藏
    QPushButton *clearBtn = m_alarmWidget->findChild<QPushButton*>("clearAlarmBtn");
    if (clearBtn) {
        clearBtn->setVisible(closable);
    }
    
    // 更新报警信息
    m_alarmLabel->setText(message);

    // 设置样式
    QString styleSheet = QString(
                             "#alarmWidget {"
                             "    background-color: rgba(30, 0, 0, 230);"
                             "    border: 3px solid %1;"
                             "    border-radius: 10px;"
                             "}"
                             "QLabel {"
                             "    color: %1;"
                             "    font-size: 18px;"
                             "    font-weight: bold;"
                             "    background-color: transparent;"
                             "}"
                             "#clearAlarmBtn {"
                             "    background-color: %1;"
                             "    color: #ffffff;"
                             "    border: 2px solid %2;"
                             "    border-radius: 6px;"
                             "    padding: 8px 16px;"
                             "    font-size: 14px;"
                             "    font-weight: bold;"
                             "    min-width: 100px;"
                             "}"
                             "#clearAlarmBtn:hover {"
                             "    background-color: %2;"
                             "    border-color: %1;"
                             "}"
                             ).arg(color)
                             .arg(color == "#ff5555" ? "#ff8888" : "#ffaa55");

    m_alarmWidget->setStyleSheet(styleSheet);

    // 计算显示位置（屏幕右下角）
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = screenGeometry.width() - m_alarmWidget->width() - 50;
    int y = screenGeometry.height() - m_alarmWidget->height() - 50;

    // 显示报警窗口
    m_alarmWidget->move(x, y);
    m_alarmWidget->show();
    m_alarmWidget->raise();
    m_alarmWidget->activateWindow();

    qDebug() << "报警窗口显示在位置: (" << x << "," << y << ")";
}
// 新增：隐藏报警
void MainWindow::hideAlarm()
{
    if (m_alarmWidget && m_alarmWidget->isVisible()) {
        m_alarmWidget->hide();
    }
}

// 新增：更新报警显示
void MainWindow::updateAlarmDisplay()
{
    // 如果有任何报警处于激活状态，显示相应的报警
    if (m_emergencyStopAlarm) {
        QString alarmMessage;
        if (m_emergencyStopColumnFlag && m_emergencyStopChassisFlag) {
            alarmMessage = "立柱触发急停报警\n底盘触发急停报警";
        } else if (m_emergencyStopColumnFlag) {
            alarmMessage = "立柱触发急停报警";
        } else {
            alarmMessage = "底盘触发急停报警";
        }
        showAlarm(alarmMessage, "#ff5555");
    } else if (m_forceLimitAlarm) {
        showAlarm("力控超限被触发，请手动移出超限位\n请手动移出超限位置", "#ff8800");
    } else {
        // 没有报警时隐藏窗口
        // 只有在没有转向切换报警时才隐藏
        if (!m_isSteeringAlarmActive) {
            hideAlarm();
        }
    }
}

void MainWindow::onTestAlarmButtonClicked()
{
    qDebug() << "=== 开始报警系统测试 ===";

    // 测试1：直接调用showAlarm函数，测试窗口是否能显示
    qDebug() << "测试1：直接调用showAlarm函数...";
    showAlarm("手动测试报警 - 紧急停止", "#ff5555");

    // 等待3秒
    QTimer::singleShot(3000, this, [this]() {
        qDebug() << "测试2：测试力控超限报警...";
        showAlarm("手动测试报警 - 力控超限", "#ff8800");
    });

    // 等待6秒，测试报警条件检查
    QTimer::singleShot(6000, this, [this]() {
        qDebug() << "测试3：通过设置标志位触发报警检查...";

        // 设置报警标志
        m_emergencyStopColumnFlag = true;
        m_emergencyStopChassisFlag = true;
        m_forceLimitFlag = true;
        m_forcecontrolMode = true;  // 模拟力控开启

        qDebug() << "立柱急停标志:" << m_emergencyStopColumnFlag;
        qDebug() << "底盘急停标志:" << m_emergencyStopChassisFlag;
        qDebug() << "力控超限标志:" << m_forceLimitFlag;
        qDebug() << "力控模式:" << m_forcecontrolMode;

        // 手动触发报警检查
        checkAlarmConditions();
    });

    // 等待9秒，清除报警
    QTimer::singleShot(9000, this, [this]() {
        qDebug() << "测试4：清除报警...";

        // 清除报警标志
        m_emergencyStopColumnFlag = false;
        m_emergencyStopChassisFlag = false;
        m_forceLimitFlag = false;

        // 触发报警检查
        checkAlarmConditions();

        qDebug() << "=== 报警系统测试完成 ===";
        ui->statusBar->showMessage("报警系统测试完成", 3000);
    });
}

void MainWindow::on_Btn_test_clicked()
{
    if (!m_poseProvider) return;

    static double angle = 0;
    static double distance = 0;
    
    // 模拟角度变化
    angle += 5.0;
    if (angle > 360.0) angle = 0;
    
    // 模拟平移变化 (往复运动)
    distance += 10.0;
    if (distance > 200.0) distance = -200.0;

    m_poseProvider->setX(distance);
    m_poseProvider->setY(distance * 0.8);
    m_poseProvider->setZ(distance * 0.5);

    m_poseProvider->setRoll(angle);
    m_poseProvider->setPitch(angle * 0.5);
    m_poseProvider->setYaw(angle * 1.2);

    qDebug() << "Test Pose Update - Angle:" << angle << "Distance:" << distance;
}

// ============ 转向模式切换报警逻辑 ============

void MainWindow::checkSteeringSwitchCompletion(int address, quint16 value)
{
    // 如果没有在等待或者地址不对，直接返回
    if (!m_isSwitchingSteeringMode || address != 50) {
        return;
    }

    if (m_targetSteeringWaitBit == 11 || m_targetSteeringWaitBit == 12) {
        bool targetBitSet = (value >> m_targetSteeringWaitBit) & 0x01;
        bool bit10Set = (value >> 10) & 0x01;
        
        // 只有当目标位和Bit 10都为1时才算完成
        if (targetBitSet && bit10Set) {
            qDebug() << "[转向切换] 检测到地址50满足条件: Bit10=1 且 Bit" << m_targetSteeringWaitBit << "=1，切换完成";
            m_isSwitchingSteeringMode = false;
            m_targetSteeringWaitBit = -1;
            
            // 隐藏报警
            m_isSteeringAlarmActive = false;
            hideAlarm();
        } else {
             // qDebug() << "[SteeringCheck] 等待条件: Bit10 && Bit" << m_targetSteeringWaitBit 
             //          << " 当前: Bit10=" << bit10Set << " Target=" << targetBitSet;
        }
    }
}
