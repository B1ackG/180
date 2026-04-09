#ifndef MAINWINDOW_H
/**
 * @file mainwindow.h
 * @brief 主窗口的声明，包含主要 UI 控件与界面逻辑的入口类。
 *
 * 详细说明: 定义 `MainWindow`（或类似入口窗口）的界面元素、信号与槽，负责应用主要交互流程。
 *
 * 使用示例:
 * @code
 * #include "mainwindow.h"
 * MainWindow *w = new MainWindow;
 * w->show();
 * @endcode
 */
#define MAINWINDOW_H


#include <QMainWindow>
#include "techpushbutton.h"
#include "techspeedgauge.h"
#include "techslideredit.h"
#include "techvirtualkeyboard.h"
#include "operationrecorder.h"
#include "mappingconfig.h"
#include "matrixkeymonitor.h"
#include "techspeeddialsimple.h"
#include "matrixkeythreadmanager.h"
#include "modbusthreadmanager.h"
#include "techsliderlabel.h"
#include "techarcgauge.h"
#include "speedmodeselector.h"
#include "modbusvariables.h"
#include "agvmodbusmanager.h"
#include "steeringmodeselector.h"
#include "enablebuttonworker.h"


#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator> // 验证器需要这个
#include <QCheckBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QQuickWidget>
#include <QQuickItem>
#include <QQmlContext>
#include <QSet>
#include "poseprovider.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
/*****************
    #include <QListWidget>
    #include <QDateTime>

    struct ClickRecord {
        QDateTime timestamp;    // 点击时间
        QString widgetName;     // 控件名称
        QString widgetType;     // 控件类型
        QString description;    // 描述信息
    };
    *****************/

class AGVModbusManager;
class FeatureSwitchManager;
class FeatureSwitchWidget;

/**
 * @class MainWindow
 * @brief 主窗口类，管理整个应用的 UI、设备通信与业务逻辑。
 *
 * 详细说明：
 * `MainWindow` 负责构建与维护主界面，初始化并连接各个子模块（如 Modbus 管理、
 * 仪表盘、历史记录、按键监控等），处理顶层信号与槽，协调 UI 与硬件之间的数据交互。
 * 本类包含启动写入寄存器、轮询 Modbus、报警显示、力传感器读取、历史记录管理等主要功能。
 *
 * 使用示例:
 * @code
 * #include <QApplication>
 * #include "mainwindow.h"
 *
 * int main(int argc, char *argv[])
 * {
 *     QApplication a(argc, argv);
 *     MainWindow w;
 *     // 写入开机需要的寄存器初始值
 *     w.performStartupWrites();
 *     // 显示主窗口
 *     w.show();
 *     return a.exec();
 * }
 * @endcode
 *
 * 另一个常见用例：在运行时更新速度显示
 * @code
 * MainWindow *w = new MainWindow();
 * w->updateSpeed(2.5); // 将速度仪表更新为 2.5
 * @endcode
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // ==========================================
    // 0. 类型定义 (Type Definitions)
    // ==========================================
    enum ForceDisplayMode { ForceDisplayBig = 0, ForceDisplaySmall = 1 };
    enum class UserRole { Operator = 0, Engineer = 1, Admin = 2, Manufacturer = 3 };
    enum ControlMode { WIRED_MODE = 128, WIRELESS_MODE = 0 };

    // ==========================================
    // 1. 生命周期与核心初始化 (Life Cycle & Core)
    // ==========================================
    /**
     * @brief 构造 MainWindow 并初始化内部状态与 UI
     * @param parent 父窗口指针，默认为 nullptr
     */
    MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构 MainWindow，释放资源并停止后台任务
     *
     * 使用示例:
     * @code
     * MainWindow *w = new MainWindow;
     * delete w; // 触发析构并释放资源
     * @endcode
     */
    ~MainWindow();

    /**
     * @brief 执行开机时需要写入的 Modbus 寄存器初始值
     *
     * 使用示例:
     * @code
     * MainWindow w;
     * w.performStartupWrites();
     * @endcode
     */
    void performStartupWrites();

    // ==========================================
    // 2. UI 框架、背景与绘制 (UI Framework & Rendering)
    // ==========================================
    /** @brief 自定义绘制主窗口背景 */
    void paintEvent(QPaintEvent *event) override;
    /** @brief 事件过滤器处理特定的 UI 交互 */
    bool eventFilter(QObject *obj, QEvent *event) override;
    /** @brief 焦点变化处理回调 */
    void onFocusChanged(QWidget *old, QWidget *now);
    /** @brief 加载并缓存主窗口背景图片 */
    void loadBackgroundImage();
    
    // UI 子模块初始化
    /** @brief 初始化 UI 布局与控件 */
    void initializeUI();
    /** @brief 建立信号与槽的连接 */
    void setupConnections();
    /** @brief 应用界面样式 */
    void setupStyles();
    /** @brief 刷新功能开关组按钮视觉状态 */
    void updateFunctionSwitchVisuals();
    /** @brief 初始化并管理界面动画 */
    void setupAnimations();
    /** @brief 设置技术按键边框样式 */
    void setupTechBorders();

    // ==========================================
    // 3. 报警系统 (Alarm System)
    // ==========================================
    /** @brief 启动报警子系统 */
    void startAlarmSystem();
    /** @brief 配置报警系统 */
    void setupAlarmSystem();
    /** @brief 检查并处理报警条件 */
    void checkAlarmConditions();
    /** @brief 显示报警信息 */
    void showAlarm(const QString &message, const QString &color, bool closable = true);
    /** @brief 隐藏报警显示 */
    void hideAlarm();
    /** @brief 刷新报警显示 */
    void updateAlarmDisplay();

    // 提示信息系统
    /** @brief 更新提示内容标签 */
    void updateStatusTip(const QString &message);

    // ==========================================
    // 4. Modbus & 设备通信 (Modbus & Communication)
    // ==========================================
    /** @brief 初始化 Modbus 相关配置 */
    void modbusInit();
    /** @brief 创建并配置 Modbus 管理器 */
    void setupModbusManager();
    /** @brief 启动 Modbus 轮询 */
    void startModbusPolling();
    /** @brief 立即轮询 Modbus 变量 */
    void pollModbusVariables();
    /** @brief 初始化 Modbus 变量映射 */
    void setupModbusVariables();
    /** @brief 创建并绑定 Modbus 标签 */
    void setupModbusLabels();
    /** @brief 向主控设备写入寄存器 */
    void writeToMainDevice(int address, int value);
    
    /** @brief 初始化 AGV Modbus 子系统 */
    void setupAGVModbus();
    /** @brief 初始化 AGV 相关 UI */
    void setupAGVUI();
    /** @brief 向 AGV 写寄存器 */
    void writeToAGVDevice(int address, int value);
    /** @brief 按位更新 AGV 寄存器并写回，自动保留未修改位 */
    bool writeAGVRegisterBits(int address, const QList<QPair<int, bool>> &bitUpdates, const QString &scene = QString());
    
    /** @brief 配置浮点寄存器读取 */
    void setupModbusFloatReading();
    /** @brief 读取所有浮点寄存器缓存 */
    void readAllFloatRegisters();
    /** @brief 将两个寄存器转换为 float */
    float registersToFloat(quint16 high, quint16 low);
    /** @brief 按 DCBA FEHG 字节顺序将4个寄存器转换为 double */
    double registersToDoubleDCBAFEHG(quint16 reg1, quint16 reg2, quint16 reg3, quint16 reg4);

    // ==========================================
    // 5. 传感器与周边硬件 (Sensors & Peripherals)
    // ==========================================
    /** @brief 创建并绑定大力值显示标签 */
    void setupBigForceLabels();
    /** @brief 创建并绑定小力值显示标签 */
    void setupSmallForceLabels();
    /** @brief 配置力传感器读取 */
    void setupForceReading();
    /** @brief 配置大量程力读取 */
    void setupBigForceReading();
    /** @brief 设置清除力值按钮 */
    void setupForceClearButton();
    /** @brief 设置力值显示模式切换按钮 */
    void setupForceDisplayModeButtons();
    /** @brief 读取大力值寄存器 */
    void readBigForceRegisters();
    /** @brief 读取小力值寄存器 */
    void readSmallForceRegisters();
    /** @brief 更新大力值标签 */
    void updateBigForceLabel(const QString& labelName, float value);
    /** @brief 更新小力值标签 */
    void updateSmallForceLabel(const QString& labelName, float value);
    /** @brief 设置力值显示模式 */
    void setForceDisplayMode(ForceDisplayMode mode);

    /** @brief 更新仿真数据（开发/测试用） */
    void updateSimulation(); 

    /** @brief 初始化虚拟键盘 */
    void setupVirtualKeyboard();
    /** @brief 初始化按键管理器 */
    void setupKeyManager();
    /** @brief 初始化线程监控 UI */
    void setupThreadMonitorUI();
    /** @brief 更新线程状态显示 */
    void updateThreadStatus();
    /** @brief 初始化使能按键线程与逻辑 */
    void setupEnableButton();
    /** @brief 轮询使能按键状态 */
    void pollEnableButton();
    /** @brief 处理使能按键状态变化 */
    void processEnableButton(bool enabled);
    /** @brief 使能按键通过套接字激活时回调 */
    void onEnableButtonActivated(int socket);

    // ==========================================
    // 6. 机器人运动控制与配置 (Motion Control & Config)
    // ==========================================
    /** @brief 设置转向模式控制 */
    void setupSteeringModeControl();
    /** @brief 初始化速度模式选择器 */
    void initSpeedModeSelector();
    /** @brief 初始化 AGV OA 控制 */
    void setupAGVOAControl();
    /** @brief 初始化 AGV 移动速度控制 */
    void setupAGVMoveSpeedControl();
    /** @brief 初始化 AGV 角度控制 */
    void setupAGVAngleControl();
    /** @brief 初始化步进移动控制 */
    void setupStepMoveControl();
    /** @brief 配置步进移动行编辑 */
    void setupStepMoveLineEdits();
    /** @brief 将步进设置写入寄存器 */
    void writeStepMoveRegisters();
    /** @brief 清除步进寄存器 */
    void clearStepMoveRegisters();
    
    // ==========================================
    // 7. 历史记录与日志 (History & Logging)
    // ==========================================
    /** @brief 初始化历史记录 UI */
    void setupRecordUI();
    /** @brief 刷新历史记录显示 */
    void updateRecordDisplay();
    /** @brief 连接历史记录信号与槽 */
    void connectRecordSignals();
    /** @brief 初始化历史页面 */
    void initializeHistoryPage();
    /** @brief 配置 TCP 传输 UI */
    void setupTcpTransmissionUI();
    /** @brief 启用或禁用 TCP 传输 */
    void enableTcpTransmission(bool enabled);    /** @brief 更新TCP服务器IP（仅修改主机号） */
    void updateTcpServerHost(const QString &hostSuffix);
    /** @brief 更新模拟器IP主机号（192.168.1.XXX） */
    void updateSimulatorHost(const QString &hostSuffix);
    // ==========================================
    // 仪表盘辅助
    // ==========================================
    /** @brief 初始化速度仪表 UI */
    void initSpeedGaugeUI();
    /**
     * @brief 更新速度显示
     * @param newSpeed 新速度值
     *
     * 使用示例:
     * @code
     * MainWindow w;
     * w.updateSpeed(1.25);
     * @endcode
     */
    void updateSpeed(qreal newSpeed);
    /** @brief 初始化滑块编辑 UI */
    void initSliderEditUI();

private slots:
    // ==========================================
    // 8. 信号处理槽函数 (Slot Handlers)
    // ==========================================
    // 核心/系统
    /** @brief 使能按键状态变更回调 */
    void onEnableButtonStateChanged(bool enabled);
    /** @brief 使能按键错误回调 */
    void onEnableButtonError(const QString &error);
    /** @brief 测试报警按钮点击回调 */
    void onTestAlarmButtonClicked();
    /** @brief 非 AGV 滑块编辑值变化回调
     *  @param changedSlider 被改变的滑块
     *  @param newValue 新值
     *  @param allNonAGVSliders 其它非 AGV 滑块列表
     */
    void onNonAGVSliderEditChanged(TechSliderEdit *changedSlider, double newValue, const QList<TechSliderEdit*> &allNonAGVSliders);
    /** @brief 检查转向切换完成状态（由 Modbus 值驱动） */
    void checkSteeringSwitchCompletion(int address, quint16 value);

    // 机器人步进与模式控制
    /** @brief 步进移动按钮点击 */
    void onStepMoveButtonClicked();
    /** @brief 使能按键在步进模式下按下 */
    void onEnableButtonPressedStepMode();
    /** @brief 使能按键在步进模式下释放 */
    void onEnableButtonReleasedStepMode();
    /** @brief J1 步进值变更 */
    void onJ1MoveStepChanged(const QString &text);
    /** @brief J2 步进值变更 */
    void onJ2MoveStepChanged(const QString &text);
    /** @brief J3 步进值变更 */
    void onJ3MoveStepChanged(const QString &text);
    /** @brief J4 步进值变更 */
    void onJ4MoveStepChanged(const QString &text);
    /** @brief 转向模式变更回调
     *  @param mode 新的转向模式
     *  @param modbusValue 与 Modbus 对应的值
     */
    void onSteeringModeChanged(SteeringMode mode, int modbusValue);
    /** @brief 移除警告按钮点击 */
    void on_TBtn_RemoveWarning_clicked();

    // 历史记录
    /** @brief 清除记录 */
    void onClearRecords();
    /** @brief 保存记录 */
    void onSaveRecords();
    /** @brief 导出记录 */
    void onExportRecords();
    /** @brief 过滤记录 */
    void onFilterRecords();
    /** @brief 发送所有记录（例如通过 TCP） */
    void onSendAllRecords();
    /** @brief TCP 连接状态变更回调 */
    void onTcpConnectionStatusChanged(bool connected);
    /** @brief TCP 传输完成回调 */
    void onTcpTransmissionComplete();
    /** @brief TCP 传输错误回调 */
    void onTcpTransmissionError(const QString &error);
    /** @brief 启用/禁用 TCP 传输复选框回调 */
    void onEnableTcpTransmission(bool checked);

    // 传感器与状态更新
    /** @brief 矩阵按键按下事件
     *  @param keyNumber 键位编号
     *  @param pressed 是否按下
     */
    void onMatrixKeyPressed(int keyNumber, bool pressed);
    /** @brief 力清除按钮按下 */
    void onForceClearPressed();
    /** @brief 力清除按钮释放 */
    void onForceClearReleased();

    // Modbus 状态反馈
    /** @brief Modbus 已连接 */
    void onModbusConnected();
    /** @brief Modbus 已断开 */
    void onModbusDisconnected();
    /** @brief Modbus 错误回调 */
    void onModbusError(const QString &error);
    /** @brief Modbus 寄存器值变化回调 */
    void onModbusRegisterValueChanged(int address, quint16 value);

    // AGV 状态反馈
    /** @brief AGV Modbus 已连接 */
    void onAGVModbusConnected();
    /** @brief AGV Modbus 已断开 */
    void onAGVModbusDisconnected();
    /** @brief AGV Modbus 错误 */
    void onAGVModbusError(const QString &error);
    /** @brief AGV 位变量变化 */
    void onAGVBitVariableChanged(int address, int bitPos, bool value);
    /** @brief AGV 字变量变化 */
    void onAGVWordVariableChanged(int address, quint16 value);
    /** @brief 更新 AGV 故障标签文本 */
    void onAGVUpdateFaultsLabel(const QString &text);
    /** @brief 更新 AGV 进度条 */
    void onAGVUpdateProgressBar(const QString &name, int value);
    /** @brief 更新 AGV 状态标签 */
    void onAGVUpdateStatusLabel(const QString &name, const QString &text);
    /** @brief 将故障码加入列表 */
    void onAGVAddFaultCodeToList(const QString &faultCode);
    /** @brief 清除 AGV 故障码列表 */
    void onAGVClearFaultCodes();
    /** @brief 收到 AGV 心跳 */
    void onAGVHeartbeatReceived();

    /** @brief 更新状态栏时间和日期 */
    void updateStatusBarTime();

    // 其他 UI 处理
    /** @brief 控制模式按钮点击 */
    void onControlModeClicked();
    /** @brief 切换力控制模式 */
    void toggleForceControl();
    /** @brief AGV OA 按钮点击 */
    void onAGVOABtnClicked();
    /** @brief AGV 驻车按钮点击 */
    void onAGVParkBtnClicked();
    /** @brief AGV 移动速度变化 */
    void onAGVMoveSpeedChanged(double value);
    /** @brief AGV 角度变化 */
    void onAGVAngleChanged(double value);
    /** @brief 特定按钮释放回调 */
    void on_TBtn_VeSupSec_Rise_released();

    void on_Btn_test_clicked();

signals:
    void modbusValueChangedForAlarm();

private:
    // ==========================================
    // 9. 私有成员变量 (Private Data Members)
    // ==========================================
    Ui::MainWindow *ui;
    PoseProvider *m_poseProvider = nullptr;
    FeatureSwitchManager *m_featureSwitchManager = nullptr;
    FeatureSwitchWidget *m_featureSwitchWidget = nullptr;

    // ----- 核心与系统 -----
    QThread *m_enableButtonThread;
    EnableButtonWorker *m_enableButtonWorker;
    int m_enableButtonFd;
    QSocketNotifier *m_enableButtonNotifier;
    bool m_lastEnableButtonState;
    QTimer *m_enablePollTimer;
    
    // ----- 报警系统 -----
    QWidget *m_alarmWidget = nullptr;
    QLabel *m_alarmLabel = nullptr;
    QTimer *m_alarmCheckTimer = nullptr;
    bool m_emergencyStopAlarm = false;
    bool m_forceLimitAlarm = false;
    bool m_emergencyStopColumnFlag = false;
    bool m_emergencyStopChassisFlag = false;
    bool m_forceLimitFlag = false;
    bool m_isSteeringAlarmActive = false;
    bool m_isSwitchingSteeringMode = false;
    int m_targetSteeringWaitBit = -1;

    // ----- Modbus & 通信 (Main) -----
    ModbusThreadManager *m_modbusManager;
    ModbusVariables *m_modbusVariables;
    QMap<int, QLabel*> m_modbusLabels;
    QTimer *m_modbusPollTimer;
    QTimer *m_modbusReadTimer;
    QMap<QPair<int, int>, QPair<quint16, quint16>> m_floatRegisters;
    QVector<TechSliderLabel*> m_floatLabels;

    // ----- AGV 通信 & UI -----
    AGVModbusManager *m_agvModbusManager;
    QList<QLabel*> m_agvStatusLabels;
    QListWidget *m_agvFaultListWidget;
    QLabel *m_agvFaultsLabel;
    bool m_agvOaEnabled = true;
    bool m_agvParkingEnabled = false;
    QMap<int, quint16> m_agvRegisterShadow;

    // ----- 六维力 -----
    ForceDisplayMode m_forceDisplayMode = ForceDisplayBig;
    QMap<QString, float> m_bigForceOffsets;
    QMap<QString, float> m_smallForceOffsets;
    QMap<QString, float> m_bigForceCurrentValues;
    QMap<QString, float> m_smallForceCurrentValues;
    QMap<QPair<int, int>, QPair<quint16, quint16>> m_bigForceRegisters;
    QMap<QPair<int, int>, QPair<quint16, quint16>> m_smallForceRegisters;
    QMap<QString, QLabel*> m_bigForceLabels;
    QMap<QString, QLabel*> m_smallForceLabels;
    QLabel *m_labelBigFX = nullptr;
    QLabel *m_labelBigFY = nullptr;
    QLabel *m_labelBigFZ = nullptr;
    QLabel *m_labelBigMX = nullptr;
    QLabel *m_labelBigMY = nullptr;
    QLabel *m_labelBigMZ = nullptr;
    QLabel *m_labelSmallFX = nullptr;
    QLabel *m_labelSmallFY = nullptr;
    QLabel *m_labelSmallFZ = nullptr;
    QLabel *m_labelSmallMX = nullptr;
    QLabel *m_labelSmallMY = nullptr;
    QLabel *m_labelSmallMZ = nullptr;
    QTimer* m_forceReadTimer;
    bool m_isForcePeeled = false;

    // ----- 历史记录与日志 -----
    OperationRecorder *m_recorder;
    MappingConfig* m_mappingConfig;
    QMap<int, QString> m_pageNames;
    bool m_tcpTransmissionEnabled;
    QString m_lastNotificationMessage;
    qint64 m_lastNotificationMs = 0;
    QString m_lastTcpErrorNotification;
    qint64 m_lastTcpErrorNotificationMs = 0;
    QSet<int> m_agvDisconnectedWarnedAddresses;

    // ----- 机器人状态控制 -----
    UserRole m_currentUserRole = UserRole::Operator;
    
    ControlMode m_controlMode = WIRED_MODE;
    
    bool m_stepModeEnabled = false;
    bool m_stepModeUnknown = true;
    bool m_isJointMode = true;
    bool m_moveModeUnknown = true;
    bool m_forcecontrolMode = false;

    // ----- 通信轮询与重连参数（可持久化） -----
    int m_mainModbusPollIntervalMs = 500;
    int m_mainUiPollIntervalMs = 200;
    int m_mainReconnectIntervalMs = 5000;
    int m_agvPollIntervalMs = 200;
    int m_agvReconnectIntervalMs = 5000;
    QString m_agvHost = "192.168.1.88";
    quint16 m_agvPort = 502;

    // ----- 其他组件与 UI 指针缓存 -----
    TechVirtualKeyboard *m_virtualKeyboard;
    MatrixKeyThreadManager *m_keyManager;
    SteeringModeSelector *m_steeringModeSelector = nullptr;
    SteeringMode m_lastSteeringMode = STEER_FRONT_BACK;
    SpeedModeSelector *m_speedModeSelector;
    QQuickWidget *m_speedGaugeQml = nullptr;  // 使用 QML 版本的速度仪表
    QQuickWidget *m_historyListQml = nullptr;  // 使用 QML 版本操作记录列表
    QMovie* m_verticalMovie;
    QPixmap m_backgroundPixmap;
    bool m_backgroundLoaded = false;
    QTimer *m_dataSimulator;
    QLabel *m_threadStatusLabel;
    QTimer *m_threadMonitorTimer;

    // 控件指针缓存/列表
    QList<TechPushButton*> m_techButtons;
    QVector<TechSliderEdit*> m_sliders;
    QVector<TechSliderLabel*> m_sliderLabels;
    QMap<QString, TechSliderLabel*> m_sliderLabelInstances;
    QMap<QString, TechArcGauge*> m_arcGauges;
    QMap<QString, QVector<TechSliderLabel*>> m_pageSliders;
    QToolButton *m_btnStepMove = nullptr;
    QLineEdit *m_editJ1MoveStep = nullptr;
    QLineEdit *m_editJ2MoveStep = nullptr;
    QLineEdit *m_editJ3MoveStep = nullptr;
    QLineEdit *m_editJ4MoveStep = nullptr;
    TechPushButton *m_techBtnAGV_OA = nullptr;
    TechPushButton *m_techBtnAGV_Park = nullptr;
    TechSliderEdit *m_editAGV_MoveSpeed = nullptr;
    TechSliderEdit *m_editAGV_Angle = nullptr;
    TechPushButton *m_btnForceControl = nullptr;
    TechPushButton *m_btnBigForceControl = nullptr;
    TechPushButton *m_btnSmallForceControl = nullptr;
    QPushButton *m_btnForceClear = nullptr;
    QToolButton *m_controlModeBtn = nullptr;
    QToolButton *m_enableBtn = nullptr;
    TechPushButton *m_forcecontrolModebtn = nullptr;

    // ==========================================
    // 10. 内部辅助函数 (Helper Methods)
    // ==========================================
    /** @brief 初始化窗口 UI（内部） */
    void initUI();

    /** @brief 检查并修正 UI 状态 */
    void checkUI();

    /** @brief 初始化技术按键集合并绑定回调 */
    void initTechButtons();

    /** @brief 设置用于开发/测试的数据仿真（定时器等） */
    void setupDataSimulation();

    /** @brief 更新导航按钮样式，高亮当前激活按钮 */
    void updateNavButtonStyles(QPushButton* activeBtn = nullptr);

    /** @brief 初始化页面索引与名称映射 */
    void initializePageNames();

    /** @brief 初始化核心页面和基础 UI 模块 */
    void initializeCorePagesAndUi();

    /** @brief 初始化业务子系统（通信、线程、样式、控制） */
    void initializeCoreSubsystems();

    /** @brief 初始化寄存器缓存映射 */
    void initializeRegisterCache(
        QMap<QPair<int, int>, QPair<quint16, quint16>> &cache,
        const QList<QPair<int, int>> &registerPairs,
        const QString &cacheName);

    /** @brief 初始化浮点与六维力寄存器及标签 */
    void initializeForceAndFloatSubsystem();

    /** @brief 安排延迟启动任务（开机写寄存器、轮询等） */
    void scheduleStartupTasks();

    /** @brief 连接构造阶段公共信号 */
    void connectConstructorSignals();

public:
    /** @brief 从配置文件加载通信轮询参数 */
    void loadPollingRuntimeSettings();

    /** @brief 将通信轮询参数保存到配置文件 */
    void savePollingRuntimeSettings() const;

    /** @brief 将当前通信轮询参数应用到运行中的管理器 */
    void applyPollingRuntimeSettings();

    /** @brief 加载 SliderLabel 的自定义配置 */
    void loadSliderLabelRuntimeSettings();

    /** @brief 保存 SliderLabel 的自定义配置 */
    void saveSliderLabelRuntimeSettings() const;

    /** @brief 应用 SliderLabel 的自定义配置到所有实例 */
    void applySliderLabelRuntimeSettings();

private:
    /** @brief 连接导航与页面切换相关信号 */
    void setupNavigationConnections();

    /** @brief 连接历史记录与权限相关信号 */
    void setupRecordAndPermissionConnections();

    /** @brief 连接控制模式与交互控件信号 */
    void setupControlConnections();

    /** @brief 连接子系统状态反馈信号 */
    void setupSubsystemConnections();

    /** @brief 配置管理员密码页面 */
    void setupAdminPasswordPage();

    /** @brief 显示临时通知（气泡/提示条） */
    void showNotification(const QString &message);
    
    /** @brief 获取当前页面名称 */
    QString getCurrentPageName() const;

    /** @brief 根据对象返回控制类型名称（用于图标/文本） */
    QString getControlTypeName(QObject *obj) const;

    /** @brief 获取控件所在页面名 */
    QString getControlPageName(QWidget *widget);

    /** @brief 根据记录类型返回显示颜色 */
    QString getRecordColor(const OperationRecord &record);

    /** @brief 根据控件类型返回图标名或路径 */
    QString getControlIcon(const QString &controlType);

    /** @brief 判断记录是否应在当前过滤条件下显示 */
    bool shouldDisplayRecord(const OperationRecord &record, const QString &filter);

    /** @brief 获取指定滑块标签当前值 */
    double getSliderLabelValue(const QString &labelName);

    /** @brief 获取指定滑动编辑当前值 */
    double getSliderEditValue(const QString &sliderName);

    /** @brief 记录竖直支撑动作（用于历史记录） */
    void recordVerticalSupportAction(int keyNumber, bool pressed);

    /** @brief 记录水平支撑动作 */
    void recordHorizontalSupportAction(int keyNumber, bool pressed);

    /** @brief 记录水平支撑移动动作 */
    void recordHorizontalSupportMoveAction(int keyNumber, bool pressed);

    /** @brief 记录步进移动动作开始/持续状态
     *  @param jointName 关节名
     *  @param currentValue 当前值
     *  @param stepValue 步长字符串
     *  @param start 是否开始动作
     */
    void recordStepMoveAction(const QString &jointName, double currentValue, const QString &stepValue, bool start);

    /** @brief 记录步进移动结束（用于历史记录） */
    void recordStepMoveEnd(const QString &jointName, double currentValue);

    /** @brief 返回蓝色风格的 Widget 样式 */
    QString BlueWidgetStyle(const QString &WidgetType );

    /** @brief 返回深色 Widget 样式 */
    QString DarkWidgetStyle(const QString &WidgetType );

    /** @brief 返回透明风格的 Widget 样式 */
    QString TransparentWidgetStyle(const QString &WidgetType );

    /** @brief 应用一组 `QPushButton` 的样式 */
    void applyPushButtonStyles(const QList<QPushButton*> &buttons);

    /** @brief 应用一组 `QToolButton` 的样式 */
    void applyToolButtonStyles(const QList<QToolButton*> &buttons);

    /** @brief 应用一组 `QLineEdit` 的样式 */
    void applyLineEditStyles(const QList<QLineEdit*> &lineEdits);

    /** @brief 应用记录页面的通用样式 */
    void applyRecordPageStyle(QWidget *recordPage);

    struct SliderLabelConfig {
        QString labelText; QString unit; double minValue; double maxValue;
        double defaultValue; QString suffix;
        int modbusAddress1; int modbusAddress2; int modbusAddress3; int modbusAddress4;
        bool isMainPage; QStringList copyPages; int precision = 0;
        
        bool isSumMode = false;
        int sumAddress[4] = {-1, -1, -1, -1};
    };
public:
    QMap<QString, SliderLabelConfig> m_sliderLabelConfigs;
private:
    /** @brief 配置滑块标签的默认配置 */
    void setupSliderLabelConfigs();

    /** @brief 初始化滑块标签 UI */
    void initSliderLabelUI();

    /** @brief 设置滑块标签的复制页配置 */
    void setupSliderLabelCopies();

    /** @brief 为滑块标签分配 Modbus 地址 */
    void setupSliderModbusAddresses();

    /** @brief 更新指定滑块标签的数值显示 */
    void updateSliderLabelValue(const QString& labelName, float value);
    
    /** @brief 处理 AGV 按键动作 */
    void handleAGVKeyAction(int keyNumber, bool pressed);

    /** @brief 处理第二套 AGV 按键动作 */
    void handleAGVKey2Action(int keyNumber, bool pressed);

    /** @brief 获取当前转向模式文本（用于记录） */
    QString currentSteeringModeText() const;

    /** @brief 记录AGV外部按键运动日志 */
    void appendAgvExternalKeyRecord(int keyNumber, bool pressed);

    /** @brief 处理矩阵键动作 */
    void handleMatrixKeyAction(int keyNumber, bool pressed);

    /** @brief 获取按键对应的 1/2/4 地址值（内部映射） */
    int getValueFor124Address(int keyNumber, bool pressed);

    bool isBigFeatureEnabled(const QString &key) const;
    bool isSmallFeatureEnabled(const QString &key) const;
    bool isFeatureEnabled(const QString &bigKey, const QString &smallKey = QString()) const;
};
#endif // MAINWINDOW_H
