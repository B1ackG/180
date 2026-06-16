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
#include <QButtonGroup>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator> // 验证器需要这个
#include <QCheckBox>
#include <QGroupBox>
#include <QVector>
#include <QVBoxLayout>

class QIntValidator;
class QResizeEvent;
#include <QProgressBar>
#include <QQuickWidget>
#include <QQuickItem>
#include <QQmlContext>
#include <QElapsedTimer>
#include <QSet>
#include <QHash>
#include <functional>
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
    /** @brief AGV 寄存器150 bit0~10 急停来源（与 192.168.1.88 设备约定一致） */
    QStringList agvChassisEstopSourcesFromRegister150(quint16 reg150) const;
    /** @brief 主设备(192.168.1.13)寄存器150 bit4/bit5 示教器急停；bit6 主副轴位置偏差过大 */
    QStringList robotArmTeachPendantEstopFromRegister150(quint16 reg150) const;
    /** @brief 处理 AGV 51 地址提示/报警位（bit0/bit1） */
    void handleAGVRegister51Alerts(quint16 value);
    /** @brief 显示站掉线报警窗（51.bit1=1） */
    void showAgvStationOfflineAlarm();
    /** @brief 隐藏站掉线报警窗 */
    void hideAgvStationOfflineAlarm();
    /** @brief 显示驱动故障报警窗（51.bit2=1） */
    void showAgvDriveFaultAlarm();
    /** @brief 隐藏驱动故障报警窗 */
    void hideAgvDriveFaultAlarm();
    /** @brief 显示低电量提示窗（51.bit0=1） */
    void showAgvBatteryLowDialog();
    /** @brief 隐藏低电量提示窗 */
    void hideAgvBatteryLowDialog();
    /** @brief 外部按键触发的首页操作提示窗 */
    void showRobotOperationHintDialog(const QString &message);
    /** @brief 隐藏首页操作提示窗 */
    void hideRobotOperationHintDialog();
    /** @brief 首页外部按键在速度为0时的提示窗（与互锁提示窗解耦） */
    void showZeroSpeedOperationHintDialog();
    /** @brief 隐藏速度为0提示窗 */
    void hideZeroSpeedOperationHintDialog();
    /** @brief 步进模式下外部按键在步进值未设置时的提示窗 */
    void showUnconfiguredStepValueHintDialog();
    /** @brief 隐藏步进值未设置提示窗 */
    void hideUnconfiguredStepValueHintDialog();
    /** @brief 示教写门禁触发时的提示窗（样式仿高度互锁提示） */
    void showTeachingWriteGateDeniedDialog();
    /** @brief 隐藏示教写门禁提示窗 */
    void hideTeachingWriteGateDeniedDialog();
    /** @brief 无线模式下 AGV 写入前的确认提示窗 */
    void showWirelessModeWarningDialog();
    /** @brief 隐藏无线模式提示窗 */
    void hideWirelessModeWarningDialog();
    /** @brief 校验示教写门禁，未通过时弹出互锁提示并返回 false */
    bool verifyTeachingWriteGateOrShowDialog();
    /** @brief 显示负载超限预警窗（150.bit3=1） */
    void showRobotWeightOverloadDialog();
    /** @brief 隐藏负载超限预警窗（150.bit3=0） */
    void hideRobotWeightOverloadDialog();
    /** @brief 显示负载超重锁定窗（150.bit7=1） */
    void showRobotWeightLockDialog();
    /** @brief 隐藏负载超重锁定窗（150.bit7=0） */
    void hideRobotWeightLockDialog();
    /** @brief 150.bit7 锁定态是否生效（拦截外部键/转向/控制模式/驻车/底盘当前角度） */
    bool isRobotWeightLockGateActive() const;
    /** @brief bit7 锁定态下拦截操作并再次弹出锁定窗 */
    void blockRobotWeightLockOperation(const QString &hint);
    /** @brief 显示主副轴位置偏差提示窗（150.bit6=1），急停全屏清理时不隐藏 */
    void showRobotAxisSyncDeviationDialog();
    /** @brief 隐藏主副轴位置偏差提示窗（150.bit6=0） */
    void hideRobotAxisSyncDeviationDialog();
    /** @brief 显示主控限位 Toast（102.bit2/bit3=1） */
    void showRobotLimitReachedDialog(bool positiveLimit);
    /** @brief 隐藏主控限位提示窗 */
    void hideRobotLimitReachedDialog();
    /** @brief 显示驻车切换等待提示窗（与报警窗解耦） */
    void showParkingSwitchHintDialog(const QString &message);
    /** @brief 隐藏驻车切换等待提示窗 */
    void hideParkingSwitchHintDialog();
    /** @brief 显示支腿异常驻车操作弹窗（51.bit7=1 时） */
    void showParkingLegAbnormalDialog();
    /** @brief 隐藏支腿异常驻车操作弹窗 */
    void hideParkingLegAbnormalDialog();
    /** @brief 按 bit7 与驻车切换状态更新支腿异常弹窗显隐（与切换提示窗互斥） */
    void updateParkingLegAbnormalDialogVisibility();
    /** @brief 显示预计负载为空提示窗（确认后关闭） */
    void showExpectedLoadEmptyDialog();
    /** @brief 主页面预计负载输入是否为空 */
    bool isEstimatedWeightEmpty() const;
    /** @brief 显示倾覆风险提示窗（倾角 0.8°~1°） */
    void showInclinometerTiltRiskDialog();
    /** @brief 隐藏倾覆风险提示窗 */
    void hideInclinometerTiltRiskDialog();
    /** @brief 显示高倾覆风险锁定窗（倾角 >1°，需管理员密码解锁） */
    void showInclinometerTiltLockDialog();
    /** @brief 隐藏高倾覆风险锁定窗 */
    void hideInclinometerTiltLockDialog();
    /** @brief 以模态居中方式展示倾角锁定窗（待输入密码） */
    void presentInclinometerTiltLockModal();
    /** @brief 以非模态右下角方式展示已解锁的倾角锁定窗 */
    void presentInclinometerTiltLockUnlocked();

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
    /** @brief 向 AGV 写寄存器 @return 是否写入成功（未连接或写失败为 false） */
    bool writeToAGVDevice(int address, int value, bool bypassWirelessWarning = false);
    /** @brief 按位更新 AGV 寄存器并写回，自动保留未修改位 */
    bool writeAGVRegisterBits(int address,
                              const QList<QPair<int, bool>> &bitUpdates,
                              const QString &scene = QString(),
                              bool bypassWirelessWarning = false);
    /** @brief 主页面预计负载输入：应用 0~500 范围与校验器 */
    void applyEstimatedWeightRuntimeSettings();
    /** @brief 驻车伸出触发长度输入：从 config.ini 应用允许范围与校验器 */
    void applyParkOutTriggerLengthRuntimeSettings();
    /** @brief 按功能控制台配置更新管理员负载阈值输入范围 */
    void applyWeightThresholdRuntimeSettings();
    /** @brief 连续写 AGV 保持寄存器并更新 m_agvRegisterShadow */
    bool writeAgvHoldingRegisterBlock(int startAddress, const QVector<quint16> &words);
    
    /** @brief 配置浮点寄存器读取 */
    void setupModbusFloatReading();
    /** @brief 读取所有浮点寄存器缓存 */
    void readAllFloatRegisters();
    /** @brief 读取主设备控制同步寄存器组（模式位等） */
    void readMainControlSyncRegisters();
    /** @brief 将两个寄存器转换为 float */
    float registersToFloat(quint16 high, quint16 low);
    /** @brief 按 CDAB 字节顺序将两个寄存器转换为 float */
    float registersToFloatCDAB(quint16 regA, quint16 regB);
    /** @brief 按 DCBA FEHG 字节顺序将4个寄存器转换为 double */
    double registersToDoubleDCBAFEHG(quint16 reg1, quint16 reg2, quint16 reg3, quint16 reg4);

    // ==========================================
    // 5. 传感器与周边硬件 (Sensors & Peripherals)
    // ==========================================
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
    /** @brief 获取当前选中的步进目标寄存器（500~504） */
    int selectedStepTargetRegister() const;
    /** @brief 获取当前选中的步进目标名称 */
    QString selectedStepTargetName() const;
    /** @brief 根据模式与当前页刷新步进控制分组可用态 */
    void updateStepMoveGroupBoxState();
    /** @brief 根据模式启用或禁用步进目标按钮 */
    void updateStepTargetButtonsState();
    /** @brief 按当前页面使用寄存器缓存同步步进模式UI */
    void syncStepModeUiByCurrentPage();
    /** @brief 将步进值按双字浮点格式写入 502~505 */
    void writeStepValueDoubleToMainDevice(double value);
    /** @brief 将步进设置写入寄存器 */
    void writeStepMoveRegisters();
    /** @brief 清除步进寄存器 */
    void clearStepMoveRegisters();
    /** @brief 首页步进：当 ○1~○10 均已松开时清空统一步进输入框 */
    void maybeClearFirstPageStepValueIfAllExternalKeysReleased();

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
    /** @brief 从主控 Modbus 寄存器加载已累计的设备总运行时间（秒） */
    void loadPersistedDeviceTotalRuntime();
    /** @brief 将当前会话计入总运行时间并写入主控 Modbus 寄存器 */
    void savePersistedDeviceTotalRuntime();
    /** @brief 刷新历史记录页上的运行时间显示 */
    void updateHistoryListRuntimeDisplay();
    static QString formatUptimeSeconds(qint64 totalSeconds);
    /** @brief 配置 TCP 传输 UI */
    void setupTcpTransmissionUI();
    /** @brief 启用或禁用 TCP 传输 */
    void enableTcpTransmission(bool enabled);    /** @brief 更新TCP服务器IP（仅修改主机号） */
    void updateTcpServerHost(const QString &hostSuffix);
    /** @brief 更新远程模拟器 IP（192.168.x.xx） */
    void updateSimulatorHost(const QString &subnetOctet, const QString &hostOctet);
    // ==========================================
    // 仪表盘辅助
    // ==========================================
    /** @brief 初始化速度仪表 UI */
    void initSpeedGaugeUI();
    /** @brief 初始化设备坐标面板（保持寄存器 103~118，双精度） */
    void initDeviceCoordPanel();
    /**
     * @brief 初始化倾角 X + 总功率 + 倾角 Y 横向组合条（左 X、中 QML 总功率、右 Y）
     */
    void initInclinometerAndRobotPowerStrip();
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
    /** @brief 更新机器人总功率显示（寄存器134） */
    void updateRobotTotalPower(quint16 powerValue);
    /** @brief 更新倾角显示（AGV 151/152，寄存器值÷100） */
    void updateInclinometerValue(bool isXAxis, quint16 rawValue);
    /** @brief 根据 X/Y 倾角刷新倾角条颜色与倾覆风险/锁定提示窗 */
    void refreshInclinometerTiltPresentation();
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
    /** @brief 负载超限预警窗「确认」：写主控290=1并进入已确认门禁态 */
    void onRobotWeightOverloadConfirmClicked();
    /** @brief 负载超重锁定窗「确认」：写主控290=1并隐藏弹窗 */
    void onRobotWeightLockConfirmClicked();
    /** @brief 主副轴偏差窗「开始同步」：主控290=1，527.bit5=1（读改写） */
    void onRobotAxisSyncStartClicked();
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
    void on_TBtn_Interlocking_clicked();

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

    /** @brief 根据寄存器51同步驻车按钮与状态栏 */
    void syncAGVParkingStateFromRegister51(quint16 value);

    /** @brief 根据寄存器155同步转向模式按钮与状态栏 */
    void syncAGVSteeringModeFromRegister155(quint16 value);

    /** @brief 更新状态栏时间和日期 */
    void updateStatusBarTime();

    // 其他 UI 处理
    /** @brief 控制模式按钮点击 */
    void onControlModeClicked();
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

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class ToastKind : quint8 { Info, Success, Warning };
    static constexpr int kToastWidth = 420;
    static constexpr int kToastRightMargin = 32;
    static constexpr int kToastBottomMargin = 32;
    static constexpr int kToastSpacing = 10;
    static constexpr int kToastMaxCount = 4;
    static constexpr int kToastDuplicateWindowMs = 1200;
    static constexpr int kToastDefaultDurationMs = 3000;
    static constexpr int kToastWarningDurationMs = 5000;
    static constexpr int kToastMinHeight = 72;
    static constexpr int kToastMaxHeight = 132;

    struct ToastEntry {
        QWidget *widget = nullptr;
        QTimer *timer = nullptr;
        QString message;
    };

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
    bool m_emergencyStopColumnFlag = false;
    bool m_emergencyStopChassisFlag = false;
    bool m_robotArmEmergency150Flag = false;
    bool m_agvChassisEmergency51Bit5Flag = false;
    bool m_isSteeringAlarmActive = false;
    bool m_isSwitchingSteeringMode = false;
    int m_targetSteeringWaitBit = -1;
    bool m_agvStationOffline51Bit1Flag = false;
    bool m_agvDriveFault51Bit2Flag = false;
    bool m_agvBatteryLow51Bit0Flag = false;
    bool m_robotWeightOverload150Bit3Flag = false;
    bool m_robotWeightLock150Bit7Flag = false;
    bool m_robotAxisSyncDeviation150Bit6Flag = false;
    bool m_robotPositiveLimit102Bit2Flag = false;
    bool m_robotNegativeLimit102Bit3Flag = false;
    bool m_robotHeightInterlock150Bit1Flag = false;
    bool m_robotLengthInterlock150Bit2Flag = false;
    bool m_agvBatteryLowAcked = false;
    QDialog *m_parkingSwitchHintDialog = nullptr;
    QLabel *m_parkingSwitchHintLabel = nullptr;
    QDialog *m_parkingLegAbnormalDialog = nullptr;
    QLineEdit *m_parkingLegAbnormalLengthEdit = nullptr;
    QDialog *m_expectedLoadEmptyDialog = nullptr;
    QWidget *m_agvStationOfflineAlarmWidget = nullptr;
    QLabel *m_agvStationOfflineAlarmLabel = nullptr;
    QWidget *m_agvDriveFaultAlarmWidget = nullptr;
    QLabel *m_agvDriveFaultAlarmLabel = nullptr;
    QDialog *m_agvBatteryLowDialog = nullptr;
    QDialog *m_wirelessModeWarningDialog = nullptr;
    QWidget *m_robotWeightOverloadWidget = nullptr;
    QLabel *m_robotWeightOverloadLabel = nullptr;
    QPushButton *m_robotWeightOverloadConfirmBtn = nullptr;
    /** @brief 150.bit3 仍为超载时用户已点确认关闭提示窗，用于拦截后续外部操作直至超载解除 */
    bool m_robotWeightOverloadUserAckedWhileActive = false;
    QWidget *m_robotWeightLockWidget = nullptr;
    QLabel *m_robotWeightLockLabel = nullptr;
    QPushButton *m_robotWeightLockConfirmBtn = nullptr;
    /** @brief 150.bit7 仍为锁定时用户已点确认关窗，被禁操作尝试时重置并再次弹出 */
    bool m_robotWeightLockUserAckedWhileActive = false;
    QWidget *m_robotAxisSyncDeviationWidget = nullptr;
    QLabel *m_robotAxisSyncDeviationLabel = nullptr;
    QPushButton *m_robotAxisSyncDeviationStartBtn = nullptr;
    /** @brief 当前限位 Toast 控件（用于清理与确认后复位） */
    QWidget *m_robotLimitToastWidget = nullptr;
    /** @brief 当前限位 Toast 对应的 102 限位来源 */
    enum class RobotLimitDialogTrigger : quint8 { None, Positive, Negative };
    RobotLimitDialogTrigger m_robotLimitDialogTrigger = RobotLimitDialogTrigger::None;

    // ----- Modbus & 通信 (Main) -----
    ModbusThreadManager *m_modbusManager;
    ModbusVariables *m_modbusVariables;
    QMap<int, QLabel*> m_modbusLabels;
    QTimer *m_modbusPollTimer;
    QTimer *m_modbusReadTimer;
    QTimer *m_mainControlSyncTimer;
    QTimer *m_interlockingSyncTimer = nullptr;
    QMap<QPair<int, int>, QPair<quint16, quint16>> m_floatRegisters;
    QVector<TechSliderLabel*> m_floatLabels;

    // ----- AGV 通信 & UI -----
    AGVModbusManager *m_agvModbusManager;
    QList<QLabel*> m_agvStatusLabels;
    QListWidget *m_agvFaultListWidget;
    QLabel *m_agvFaultsLabel;
    bool m_agvOaEnabled = true;
    bool m_agvParkingEnabled = false;
    bool m_agvLegAbnormal51Bit7Flag = false;
    QMap<int, quint16> m_agvRegisterShadow;
    QIntValidator *m_estimatedWeightValidator = nullptr;
    QIntValidator *m_parkOutTriggerLengthValidator = nullptr;
    QIntValidator *m_weightOverloadLimitValidator = nullptr;
    QIntValidator *m_weightLockLimitValidator = nullptr;
    bool m_mainRegister150Valid = false;
    quint16 m_mainRegister150Shadow = 0;

    // ----- 历史记录与日志 -----
    OperationRecorder *m_recorder;
    MappingConfig* m_mappingConfig;
    QMap<int, QString> m_pageNames;
    bool m_tcpTransmissionEnabled;
    QString m_lastNotificationMessage;
    qint64 m_lastNotificationMs = 0;
    QWidget *m_toastHost = nullptr;
    QVBoxLayout *m_toastLayout = nullptr;
    QVector<ToastEntry> m_toasts;
    QString m_lastToastMessage;
    qint64 m_lastToastMs = 0;
    qint64 m_lastBatteryLowToastMs = 0;
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
    QHash<int, bool> m_robotExternalKeyPressed;
    quint64 m_robotExternalWriteSeq = 0;
    int m_robotActiveKey = -1;
    QHash<int, bool> m_sixAxisExternalKeyPressed;
    quint64 m_sixAxisExternalWriteSeq = 0;
    int m_sixAxisActiveKey = -1;

    // ----- 通信轮询与重连参数（可持久化） -----
    int m_mainModbusPollIntervalMs = 500;
    int m_mainUiPollIntervalMs = 200;
    int m_mainDeviceStatusPollIntervalMs = 2000;
    int m_mainDeviceStatusStart = 0;
    int m_mainDeviceStatusCount = 85;
    int m_mainControlSyncStart = 125;
    int m_mainControlSyncCount = 6;
    bool m_uiStateSyncEnabled = true;
    int m_mainReconnectIntervalMs = 5000;
    int m_agvPollIntervalMs = 200;
    int m_agvReconnectIntervalMs = 5000;
    QString m_agvHost = "192.168.1.88";
    quint16 m_agvPort = 502;
    QString m_remoteSimulatorHost = "192.168.1.70";

    // ----- 其他组件与 UI 指针缓存 -----
    TechVirtualKeyboard *m_virtualKeyboard;
    MatrixKeyThreadManager *m_keyManager;
    SteeringModeSelector *m_steeringModeSelector = nullptr;
    SteeringMode m_lastSteeringMode = STEER_FRONT_BACK;
    SpeedModeSelector *m_speedModeSelector;
    QQuickWidget *m_speedGaugeQml = nullptr;  // 使用 QML 版本的速度仪表
    QQuickWidget *m_historyListQml = nullptr;  // 使用 QML 版本操作记录列表
    QElapsedTimer m_appSessionUptimeTimer;
    qint64 m_persistedTotalRuntimeSec = 0;
    qint64 m_lastSavedTotalRuntimeSec = -1;
    bool m_runtimeBaselineReady = false;
    QTimer *m_historyRuntimeUpdateTimer = nullptr;
    QQuickWidget *m_robotTotalPowerQml = nullptr;  // 使用 QML 版本总功率卡片
    QQuickWidget *m_deviceCoordPanelQml = nullptr; // 当前 X/Y/Z/AR（寄存器 103~118）
    QWidget *m_inclinometerXCard = nullptr;  // QWidget 版本 X 轴倾角卡片容器
    QWidget *m_inclinometerYCard = nullptr;  // QWidget 版本 Y 轴倾角卡片容器
    QLabel *m_inclinometerXValueLabel = nullptr;
    QLabel *m_inclinometerYValueLabel = nullptr;
    QLabel *m_inclinometerXThresholdLabel = nullptr;
    QLabel *m_inclinometerYThresholdLabel = nullptr;
    QWidget *m_inclinometerPowerStripWidget = nullptr;
    qreal m_inclinometerXDegree = 0.0;
    qreal m_inclinometerYDegree = 0.0;
    bool m_inclinometerTiltRiskInZone = false;
    bool m_inclinometerTiltRiskAcked = false;
    QDialog *m_inclinometerTiltRiskDialog = nullptr;
    bool m_inclinometerTiltLockInZone = false;
    bool m_inclinometerTiltLockUnlocked = false;
    QDialog *m_inclinometerTiltLockDialog = nullptr;
    QLabel *m_inclinometerTiltLockPasswordHint = nullptr;
    QLineEdit *m_inclinometerTiltLockPasswordEdit = nullptr;
    QLabel *m_inclinometerTiltLockErrorLabel = nullptr;
    QPushButton *m_inclinometerTiltLockConfirmBtn = nullptr;
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
    QButtonGroup *m_stepTargetGroup = nullptr;
    QButtonGroup *m_sixAxisStepTargetGroup = nullptr;
    QLineEdit *m_stepValueEdit = nullptr;
    QLineEdit *m_editJ1MoveStep = nullptr;
    QLineEdit *m_editJ2MoveStep = nullptr;
    QLineEdit *m_editJ3MoveStep = nullptr;
    QLineEdit *m_editJ4MoveStep = nullptr;
    TechPushButton *m_techBtnAGV_OA = nullptr;
    TechPushButton *m_techBtnAGV_Park = nullptr;
    TechSliderEdit *m_editAGV_MoveSpeed = nullptr;
    TechSliderEdit *m_editAGV_Angle = nullptr;
    QLineEdit *m_weightOverloadLimitEdit = nullptr;
    QLineEdit *m_weightLockLimitEdit = nullptr;
    QToolButton *m_controlModeBtn = nullptr;
    QToolButton *m_enableBtn = nullptr;

    // ==========================================
    // 10. 内部辅助函数 (Helper Methods)
    // ==========================================
    /** @brief 初始化示教互锁按钮（8192 寄存器同步与切换） */
    void setupInterlockingTeachingButton();
    /** @brief 根据主控 8192 同步联锁按钮文案 */
    void refreshInterlockingButtonText();

    /** @brief 将寄存器 126 反映到运动模式按钮与状态栏 */
    void applyMoveModeUiFromRegister126(quint16 value);
    /** @brief 将寄存器 130 反映到机器人速度滑块 */
    void applyRobotSpeedUiFromRegister130(quint16 value);
    /** @brief 从 g_registerCache 刷新步进/运动/速度等与主控同步的 UI（示教切换后等） */
    void applyCachedMainControlSyncRegistersToUi();

    /** @brief 将主控 103~118（四组 double）刷新到坐标 QML 面板 */
    void updateDeviceCoordPanelFromCache();

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

    /** @brief 初始化浮点寄存器缓存 */
    void initializeForceAndFloatSubsystem();

    /** @brief 安排延迟启动任务（开机写寄存器、轮询等） */
    void scheduleStartupTasks();

    /** @brief 连接构造阶段公共信号 */
    void connectConstructorSignals();

    /** @brief 获取指定轴在历史记录中的当前值 */
    double getAxisCurrentValue(int axisIndex) const;

    /** @brief 获取指定轴在历史记录中的显示名称 */
    QString getAxisHistoryName(int axisIndex) const;

    /** @brief 获取指定轴在历史记录中的单位 */
    QString getAxisHistoryUnit(int axisIndex) const;

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

    /** @brief 从 config.ini 刷新首页倾角卡片上的阈值说明文字 */
    void applyInclinometerDisplayRuntimeSettings();

    /** @brief 将驻车按钮更新为开启/关闭外观（支腿异常态下不更新主按钮） */
    void applyAGVParkingButtonUi(bool enabled);
    /** @brief 将驻车按钮与状态栏更新为支腿异常外观 */
    void applyAGVParkingLegAbnormalUi();
    /** @brief 将状态栏驻车标签更新为开/关 */
    void applyAGVParkingStatusBarUi(bool enabled);
    /** @brief 驻车切换失败后按当前异常态恢复 UI */
    void restoreParkingUiAfterFailure(bool enabled);
    /** @brief 执行驻车开启/关闭 Modbus 写入与等待确认；legLengthMm&lt;0 时从支腿异常弹窗输入框读取 */
    void executeAGVParkingSwitch(bool targetEnabled, int legLengthMm = -1);

private:
    void applyModbusAccessSwitches();
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
    /** @brief 显示右下角 Toast（可选 onDismissed 在确认关闭后调用） */
    void showToast(const QString &message,
                   ToastKind kind = ToastKind::Info,
                   int durationMs = 0,
                   const std::function<void()> &onDismissed = nullptr);
    /** @brief 按寄存器 500 生成正负限位 Toast 文案 */
    QString robotLimitToastMessage(bool positiveLimit) const;
    /** @brief 隐藏指定 Toast 并重排剩余项 */
    void dismissToast(QWidget *toast);
    /** @brief 按文案关闭匹配的 Toast */
    void dismissToastByMessage(const QString &message);
    /** @brief 关闭零速度/步进值/示教写门禁/互锁等操作提示 Toast */
    void dismissOperationHintToasts();
    /** @brief 确保 Toast 宿主容器已创建 */
    void ensureToastHost();
    /** @brief 将 Toast 宿主容器定位到主窗口右下角 */
    void repositionToastHost();
    /** @brief 根据 Toast 数量更新宿主容器显隐 */
    void updateToastHostVisibility();
    /** @brief 根据类型生成 Toast 样式 */
    QString toastStyleSheet(ToastKind kind) const;

    /** @brief 与「清除报警」按钮相同的 Modbus 写入（主控 290、403） */
    void sendRemoveWarningModbusWrites();

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
    /** @brief 急停弹窗显示前，隐藏仍可见的非急停类型弹窗（主副轴偏差窗除外） */
    void hideNonEmergencyPopups();

    /** @brief 记录步进移动动作开始/持续状态
     *  @param jointName 关节名
     *  @param currentValue 当前值
     *  @param stepValue 步长字符串
     *  @param start 是否开始动作
     */
    void recordStepMoveAction(const QString &jointName, double currentValue, const QString &stepValue, bool start);

    /** @brief 记录步进移动结束（用于历史记录） */
    void recordStepMoveEnd(const QString &jointName, double currentValue);

    /** @brief 第四页六自由度点动：○1～○12 对应 RX/RY/RZ/X/Y/Z，按下/松开记录当前角度或位置（展示方式与步进一致） */
    void recordSixAxisJogExternalKey(int keyNumber, bool pressed);

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
    /** @brief 首页外部按键按下且对应速度为0时弹出提示 */
    void maybeShowZeroSpeedHintForHomePageExternalKey(int keyNumber, bool pressed);
    /** @brief 步进模式下外部按键按下且步进值为空或0时弹出提示 */
    void maybeShowUnconfiguredStepValueHintForExternalKey(int keyNumber, bool pressed);

    /** @brief 处理第二套 AGV 按键动作 */
    void handleAGVKey2Action(int keyNumber, bool pressed);
    /** @brief 检查 ○9/○10、转向切换、驻车按钮用的高度/长度互锁提示语（空表示无互锁） */
    QString robotInterlockHintMessage() const;

    /** @brief 获取当前转向模式文本（用于记录） */
    QString currentSteeringModeText() const;

    /** @brief 记录AGV外部按键运动日志；stepValueFromLineEdit 非空时追加「步进值为：…」（来自 lineEdit_StepValue） */
    void appendAgvExternalKeyRecord(int keyNumber, bool pressed, const QString &stepValueFromLineEdit = QString());

    /** @brief 处理矩阵键动作 */
    void handleMatrixKeyAction(int keyNumber, bool pressed);

    /** @brief 获取按键对应的 1/2/4 地址值（内部映射） */
    int getValueFor124Address(int keyNumber, bool pressed);

    bool isBigFeatureEnabled(const QString &key) const;
    bool isSmallFeatureEnabled(const QString &key) const;
    bool isFeatureEnabled(const QString &bigKey, const QString &smallKey = QString()) const;
};
#endif // MAINWINDOW_H
