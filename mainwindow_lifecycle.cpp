#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featureswitchmanager.h"

#include <QApplication>
#include <QStackedWidget>
#include <QDebug>
#include <QMovie>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_poseProvider(new PoseProvider(this))
    , m_featureSwitchManager(FeatureSwitchManager::instance())
    , m_enableButtonThread(nullptr)
    , m_enableButtonWorker(nullptr)
    , m_enableButtonFd(-1)
    , m_enableButtonNotifier(nullptr)
    , m_lastEnableButtonState(false)
    , m_enablePollTimer(nullptr)
    , m_alarmWidget(nullptr)
    , m_alarmLabel(nullptr)
    , m_alarmCheckTimer(nullptr)
    , m_emergencyStopAlarm(false)
    , m_forceLimitAlarm(false)
    , m_emergencyStopColumnFlag(false)
    , m_emergencyStopChassisFlag(false)
    , m_forceLimitFlag(false)
    , m_modbusManager(nullptr)
    , m_modbusVariables(new ModbusVariables(this))
    , m_modbusPollTimer(new QTimer(this))
    , m_modbusReadTimer(nullptr)
    , m_floatRegisters()
    , m_floatLabels()
    , m_agvModbusManager(nullptr)
    , m_agvFaultListWidget(nullptr)
    , m_agvFaultsLabel(nullptr)
    , m_forceReadTimer(nullptr)
    , m_recorder(new OperationRecorder(this))
    , m_mappingConfig(new MappingConfig(this))
    , m_tcpTransmissionEnabled(false)
    , m_currentUserRole(UserRole::Operator)
    , m_stepModeEnabled(false)
    , m_virtualKeyboard(nullptr)
    , m_keyManager(new MatrixKeyThreadManager(this))
    , m_steeringModeSelector(nullptr)
    , m_speedModeSelector(nullptr)
    , m_verticalMovie(nullptr)
    , m_backgroundLoaded(false)
    , m_dataSimulator(nullptr)
    , m_threadStatusLabel(nullptr)
    , m_threadMonitorTimer(nullptr)
    , m_techButtons()
    , m_btnForceClear(nullptr)
{
    ui->setupUi(this);
    loadBackgroundImage();
    setupSliderLabelConfigs();
    loadSliderLabelRuntimeSettings(); // 加载运行时自定义的限制值

    qDebug() << "=== 初始化开始 ===";
    qDebug() << "StackedWidget初始页数:" << ui->StackedWidget->count();

    initializePageNames();
    loadPollingRuntimeSettings();
    initializeCorePagesAndUi();
    initializeCoreSubsystems();

    if (ui->sixAxisQuickWidget) {
        ui->sixAxisQuickWidget->rootContext()->setContextProperty("poseData", m_poseProvider);
        ui->sixAxisQuickWidget->setSource(QUrl("qrc:/PoseDisplay.qml"));
    }

    ui->StackedWidget->setCurrentIndex(0);
    ui->lab_Overall->setScaledContents(true);
    ui->lab_Overall->setPixmap(QPixmap(":/Picture/190overall6.png"));

    qApp->installEventFilter(this);
    m_isSteeringAlarmActive = false;

    initializeForceAndFloatSubsystem();
    scheduleStartupTasks();
    connectConstructorSignals();

    qDebug() << "=== 初始化完成 ===";
}

MainWindow::~MainWindow()
{
    qDebug() << "正在清理资源...";

    if (m_keyManager) {
        qDebug() << "停止键盘管理器...";
        m_keyManager->stop();
    }

    qDebug() << "停止使能按钮监控线程...";
    if (m_enableButtonWorker) {
        m_enableButtonWorker->stopPolling();
    }

    if (m_enableButtonThread) {
        m_enableButtonThread->quit();
        m_enableButtonThread->wait(1000);
        delete m_enableButtonThread;
        m_enableButtonThread = nullptr;
        qDebug() << "使能按钮监控线程已停止";
    }

    if (m_enableButtonNotifier) {
        m_enableButtonNotifier->setEnabled(false);
        delete m_enableButtonNotifier;
        m_enableButtonNotifier = nullptr;
        qDebug() << "使能按钮通知器已清理";
    }

    if (m_enablePollTimer) {
        m_enablePollTimer->stop();
        delete m_enablePollTimer;
        m_enablePollTimer = nullptr;
        qDebug() << "使能按钮轮询定时器已清理";
    }

    if (m_enableButtonFd >= 0) {
        ::close(m_enableButtonFd);
        m_enableButtonFd = -1;
        qDebug() << "使能按钮设备已关闭";
    }

    if (m_modbusManager) {
        m_modbusManager->disconnectFromDevice();
    }

    if (m_modbusReadTimer) {
        m_modbusReadTimer->stop();
        delete m_modbusReadTimer;
    }

    if (m_agvModbusManager) {
        m_agvModbusManager->disconnectFromDevice();
        delete m_agvModbusManager;
    }

    if (m_recorder) {
        m_recorder->enableTcpTransmission(false);
    }

    if (m_forceReadTimer) {
        m_forceReadTimer->stop();
        delete m_forceReadTimer;
        m_forceReadTimer = nullptr;
        qDebug() << "六维力读取定时器已清理";
    }

    m_bigForceOffsets.clear();
    m_smallForceOffsets.clear();
    m_bigForceCurrentValues.clear();
    m_smallForceCurrentValues.clear();
    m_bigForceLabels.clear();
    m_smallForceLabels.clear();

    if (m_alarmCheckTimer) {
        m_alarmCheckTimer->stop();
        delete m_alarmCheckTimer;
        m_alarmCheckTimer = nullptr;
        qDebug() << "报警检测定时器已清理";
    }

    if (m_alarmWidget) {
        m_alarmWidget->close();
        delete m_alarmWidget;
        m_alarmWidget = nullptr;
        qDebug() << "报警窗口已清理";
    }

    delete m_verticalMovie;
    delete ui;

    qDebug() << "资源清理完成";
}

void MainWindow::startAlarmSystem()
{
    if (!isBigFeatureEnabled("alarm_system")) {
        qDebug() << "报警系统功能已关闭，跳过启动";
        return;
    }

    setupAlarmSystem();
}

void MainWindow::initializePageNames()
{
    m_pageNames[0] = "软件参数";
    m_pageNames[1] = "回转升降";
    m_pageNames[2] = "伸缩臂";
    m_pageNames[3] = "EOAT控制";
    m_pageNames[4] = "AGV控制";
    m_pageNames[5] = "管理员验证";
    m_pageNames[6] = "操作记录";
}

void MainWindow::initializeCorePagesAndUi()
{
    if (isFeatureEnabled("permission_system", "permission.admin_login")) {
        setupAdminPasswordPage();
    }

    if (isBigFeatureEnabled("operation_records")) {
        setupRecordUI();
    }

    initUI();
    setupSliderLabelCopies();
}

void MainWindow::initializeCoreSubsystems()
{
    if (isBigFeatureEnabled("modbus_agv")) {
        setupAGVModbus();
        setupAGVUI();
    }

    if (isBigFeatureEnabled("motion_control")) {
        setupAGVOAControl();
        setupAGVMoveSpeedControl();
        setupAGVAngleControl();
    }

    setupConnections();

    if (isFeatureEnabled("input_devices", "input.enable_button")) {
        setupEnableButton();
    }

    if (isBigFeatureEnabled("operation_records")) {
        connectRecordSignals();
    }

    if (isFeatureEnabled("ui_navigation", "ui.styles")) {
        setupStyles();
    }

    if (isFeatureEnabled("ui_navigation", "ui.animations")) {
        setupAnimations();
    }

    if (isFeatureEnabled("input_devices", "input.matrix_key")) {
        setupKeyManager();
    }

    if (isBigFeatureEnabled("modbus_main")) {
        modbusInit();
    }

    if (isFeatureEnabled("motion_control", "motion.step_mode")) {
        setupStepMoveControl();
        setupStepMoveLineEdits();
    }
}

void MainWindow::initializeRegisterCache(
    QMap<QPair<int, int>, QPair<quint16, quint16>> &cache,
    const QList<QPair<int, int>> &registerPairs,
    const QString &cacheName)
{
    cache.clear();
    for (const auto &pair : registerPairs) {
        cache[pair] = QPair<quint16, quint16>(0, 0);
        qDebug() << "初始化" << cacheName << "寄存器对:" << pair;
    }
}

void MainWindow::initializeForceAndFloatSubsystem()
{
    initializeRegisterCache(
        m_floatRegisters,
        {
            QPair<int, int>(201, 202),
            QPair<int, int>(203, 204),
            QPair<int, int>(205, 206),
            QPair<int, int>(207, 208)
        },
        "浮点");
}

void MainWindow::scheduleStartupTasks()
{
    // 移除 3 秒硬延迟，改为在 onModbusConnected 中按需启动任务
    // 或在特定子系统准备就绪后立即执行
    qDebug() << "启动延迟任务已调度（将由 Modbus 连接信号驱动）";
}

void MainWindow::connectConstructorSignals()
{
    connect(ui->StackedWidget, &QStackedWidget::currentChanged,
            this, [this](int index) {
                if (index == 6) {
                    updateRecordDisplay();
                }
            });

    connect(this, &MainWindow::modbusValueChangedForAlarm,
            this, &MainWindow::checkAlarmConditions, Qt::QueuedConnection);

    qDebug() << "=== 测试报警系统 ===";
    qDebug() << "报警检查定时器是否激活:" << (m_alarmCheckTimer && m_alarmCheckTimer->isActive());
    qDebug() << "报警检查定时器间隔:" << (m_alarmCheckTimer ? m_alarmCheckTimer->interval() : 0);
}

void MainWindow::initUI()
{
    initializeUI();
    initSliderEditUI();
    initSliderLabelUI();
    initTechButtons();
    initSpeedGaugeUI();

    if (isFeatureEnabled("ui_navigation", "ui.virtual_keyboard")) {
        setupVirtualKeyboard();
    }

    if (isFeatureEnabled("motion_control", "motion.steering_mode")) {
        setupSteeringModeControl();
    }

    if (isBigFeatureEnabled("tcp_transmission")) {
        setupTcpTransmissionUI();
    }
}

void MainWindow::modbusInit()
{
    if (!isBigFeatureEnabled("modbus_main")) {
        qDebug() << "主控Modbus功能已关闭，跳过初始化";
        return;
    }

    setupModbusManager();
    setupModbusVariables();
    setupModbusLabels();
}

void MainWindow::checkUI()
{
    QList<SpeedModeSelector *> selectors = findChildren<SpeedModeSelector *>();
    qDebug() << "=== 详细检查所有 SpeedModeSelector 控件 ===";
    qDebug() << "总数：" << selectors.size() << "个";

    for (int i = 0; i < selectors.size(); ++i) {
        SpeedModeSelector *selector = selectors[i];
        qDebug() << "=== 控件" << i + 1 << "===";
        qDebug() << "对象名:" << selector->objectName();
        qDebug() << "类名:" << selector->metaObject()->className();
        QObject *parentObj = selector->parent();
        qDebug() << "父对象:" << parentObj;
        if (parentObj) {
            qDebug() << "父对象类名:" << parentObj->metaObject()->className();
            qDebug() << "父对象名:" << parentObj->objectName();
        } else {
            qDebug() << "父对象类名:" << "<null>";
            qDebug() << "父对象名:" << "<null>";
        }
        qDebug() << "位置:" << selector->pos();
        qDebug() << "尺寸:" << selector->size();
        qDebug() << "是否可见:" << selector->isVisible();
        qDebug() << "是否隐藏:" << selector->isHidden();
        qDebug() << "窗口标志:" << selector->windowFlags();
        qDebug() << "几何位置:" << selector->geometry();
        qDebug() << "指针地址:" << selector;

        QWidget *parentWidget = selector->parentWidget();
        while (parentWidget) {
            qDebug() << "  父级:" << parentWidget->objectName()
                     << "类:" << parentWidget->metaObject()->className()
                     << "可见:" << parentWidget->isVisible()
                     << "隐藏:" << parentWidget->isHidden();
            parentWidget = parentWidget->parentWidget();
        }
    }
    qDebug() << "=== 检查结束 ===";
}

void MainWindow::initializeUI()
{
    setFixedSize(1280, 800);
}

void MainWindow::updateNavButtonStyles(QPushButton *activeBtn)
{
    /* QList<QPushButton *> navButtons = {
        ui->Btn_SwitchVerticalSupport,
        ui->Btn_SwitchHorizontalSupport,
        ui->Btn_SwitchAGV,
        ui->Btn_SwitchEOAT
    }; */

    const QString activeStyle = "QPushButton { border: 2px solid #00FFFF; background-color: transparent; border-radius: 10px; }";
    const QString normalStyle = "QPushButton { border: none; background-color: transparent; }";

    /* for (QPushButton *btn : navButtons) {
        if (!btn) {
            continue;
        }
        btn->setStyleSheet(btn == activeBtn ? activeStyle : normalStyle);
    } */
    Q_UNUSED(activeBtn);
}



void MainWindow::updateStatusTip(const QString &message)
{
    if (ui && ui->label_StatusTip) {
        // 使用 QFontMetrics 计算省略文本，确保不拉伸 UI
        QFontMetrics metrics(ui->label_StatusTip->font());
        QString elidedText = metrics.elidedText(message, Qt::ElideRight, ui->label_StatusTip->width());
        ui->label_StatusTip->setText(elidedText);
        
        // 设置 ToolTip 方便查看完整内容
        ui->label_StatusTip->setToolTip(message);
        
        // 瞬间闪烁效果提示更新
        ui->label_StatusTip->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: bold; background: transparent; padding-bottom: 5px;");
        QTimer::singleShot(200, [this]() {
            if (ui && ui->label_StatusTip) {
                ui->label_StatusTip->setStyleSheet("color: #00FFFF; font-size: 14px; font-weight: bold; background: transparent; padding-bottom: 5px;");
            }
        });
    }
}
