#include "mappingconfig.h"

#include "operationrecorder.h"

#include <QRegularExpression>

namespace {

/** 去掉「向某寄存器写入某值」类技术描述，仅保留面向用户的操作文案。 */
QString scrubOperationHistoryRegisterWriteText(QString s)
{
    const QString trimmed = s.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    QString t = trimmed;

    static const QRegularExpression reWholeLineWriteAddr(
        QStringLiteral(R"(^\s*写入地址\d+.*$)"),
        QRegularExpression::DotMatchesEverythingOption);
    if (reWholeLineWriteAddr.match(t).hasMatch()) {
        return QString();
    }

    static const QRegularExpression reWholeLineBatch(
        QStringLiteral(R"(^\s*批量写入地址.*$)"),
        QRegularExpression::DotMatchesEverythingOption);
    if (reWholeLineBatch.match(t).hasMatch()) {
        return QString();
    }

    // 如「○9点动按下：寄存器0 bit3=1」→ 去掉冒号后的寄存器位描述
    static const QRegularExpression reColonRegisterTail(
        QStringLiteral(R"([:：][^:：]*寄存器\s*\d+.*$)"),
        QRegularExpression::DotMatchesEverythingOption);
    t.remove(reColonRegisterTail);

    // 如「…，向主控(192.168.1.13)寄存器290写入1」
    static const QRegularExpression reMainCtrlRegWrite(
        QStringLiteral(R"([，,][^，。\n]*寄存器\s*\d+[^，。\n]*写入[^，。\n]*)"));
    t.remove(reMainCtrlRegWrite);

    // 如「…，地址124写入: 3」「地址500写入:1」
    static const QRegularExpression reAddrWriteSeg(
        QStringLiteral(R"([,，]?\s*地址\s*\d+\s*写入[^，;；]*)"));
    t.remove(reAddrWriteSeg);

    static const QRegularExpression reArrowAddr(
        QStringLiteral(R"(\s*->\s*地址\s*\d+\s*写入[^，;；]*)"));
    t.remove(reArrowAddr);

    static const QRegularExpression reParenModbus(
        QStringLiteral(R"([（(][^）\n]*Modbus[^）\n]*[）)])"));
    t.remove(reParenModbus);

    static const QRegularExpression reReadValueParen(
        QStringLiteral(R"(（读值[:：]\s*[^）]+）)"));
    t.remove(reReadValueParen);

    // 「示教写权限：主控寄存器8192与…」→ 去掉具体寄存器号
    static const QRegularExpression reMainRegNum(QStringLiteral(R"(主控寄存器\s*\d+)"));
    t.replace(reMainRegNum, QStringLiteral("主控"));

    t = t.trimmed();
    while (t.endsWith(QLatin1Char('：')) || t.endsWith(QLatin1Char(':')) || t.endsWith(QLatin1Char(','))
           || t.endsWith(QLatin1Char('，'))) {
        t.chop(1);
        t = t.trimmed();
    }
    return t;
}

bool historyTextLooksLikeRegisterWriteDetail(const QString &s)
{
    if (s.contains(QStringLiteral("寄存器"))) {
        return true;
    }
    if (s.contains(QStringLiteral("写入地址"))) {
        return true;
    }
    if (s.contains(QStringLiteral("批量写入"))) {
        return true;
    }
    static const QRegularExpression reAddrWrite(QStringLiteral(R"(地址\s*\d+\s*写入)"));
    return reAddrWrite.match(s).hasMatch();
}

QVariant scrubHistoryValueVariant(const QVariant &v)
{
    if (!v.isValid()) {
        return v;
    }
    switch (static_cast<int>(v.type())) {
    case QVariant::Bool:
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
    case QVariant::Double:
        return v;
    default:
        break;
    }
    const QString raw = v.toString();
    if (raw.isEmpty()) {
        return v;
    }
    const QString out = scrubOperationHistoryRegisterWriteText(raw);
    if (out.trimmed().isEmpty()) {
        return QVariant();
    }
    return QVariant(out);
}

} // namespace

MappingConfig* MappingConfig::s_instance = nullptr;

MappingConfig::MappingConfig(QObject *parent)
    : QObject{parent}
{
    initDefaultMappings();
}
MappingConfig* MappingConfig::instance()
{
    if (!s_instance) {
        s_instance = new MappingConfig();
    }
    return s_instance;
}

void MappingConfig::initDefaultMappings()
{
    // ==================== 控件名称映射 ====================
    // TechSliderEdit 控件
    m_controlNameMap["TechSliderEdit_VeSupSec_Roll_1"] = "回转速度";
    m_controlNameMap["TechSliderEdit_VeSupSec_Roll_2"] = "回转升降模块";
    m_controlNameMap["TechSliderEdit_VeSupSec_Angle"] = "角度设置";
    m_controlNameMap["TechSliderEdit_VeSupSec_Height"] = "高度控制";
    m_controlNameMap["TechSliderEdit_VeSupSec_LiftSpeed"] = "升降速度";

    // 页面2的滑块（如果有）
    m_controlNameMap["TechSliderEdit_HoSupSec_MotionSpeed"] = "运动速度";
    m_controlNameMap["TechSliderEdit_HoSupSec_Extend"] = "伸缩长度";
    m_controlNameMap["TechSliderEdit_HoriSupSec_MoveSpeed"] = "伸缩臂运动速度";
    m_controlNameMap["TechSliderEdit_HoriSupSec_RotationSpeed"] = "伸缩臂回转速度";
    m_controlNameMap["TechSliderEdit_EOAT_RotationSpeed"] = "末端执行器旋转速度";

    // TechPushButton 控件
    m_controlNameMap["Btn_Enable_LiftRolladjustment"] = "升降回转启用";
    m_controlNameMap["Btn_SetFoot"] = "支腿控制";
    m_controlNameMap["Btn_Test"] = "测试按钮";

    // QToolButton 控件
    m_controlNameMap["TBtn_VeSupSec_Rise"] = "垂直支撑上升";
    m_controlNameMap["TBtn_VeSupSec_Lower"] = "垂直支撑下降";
    m_controlNameMap["TBtn_UPLeft"] = "左上移动";
    m_controlNameMap["TBtn_UPRight"] = "右上移动";

    // EOAT 控制按钮
    m_controlNameMap["TBtn_EOAT_Grip"] = "末端夹紧";
    m_controlNameMap["TBtn_EOAT_Release"] = "末端释放";

        // ==================== 常用按钮/控件映射补充（避免显示英文对象名） ====================
        m_controlNameMap["TBtn_craft"] = "工艺切换按钮";
        m_controlNameMap["TBtn_RemoveWarning"] = "消除报警按钮";
        m_controlNameMap["TBtn_Interlocking"] = "上下示教器互锁切换按钮";
        m_controlNameMap["TBtn_HistoryRecord"] = "历史记录按钮";
        m_controlNameMap["TBtn_PageNavMenu"] = "系统管理菜单";
        m_controlNameMap["TBtn_DeviceControlMenu"] = "设备控制菜单";
        m_controlNameMap["TBtn_ControlModeMenu"] = "控制模式菜单";
        m_controlNameMap["TBtn_RobotControl"] = "机械臂控制";
        m_controlNameMap["TBtn_ChassisControl"] = "底盘控制";
        m_controlNameMap["TBtn_SixAxies"] = "六自由度";
        m_controlNameMap["TBtn_PermissionPage"] = "权限页面按钮";
        m_controlNameMap["TBtn_Stepmove"] = "步进模式按钮";
        m_controlNameMap["TBtn_MoveMode"] = "关节/坐标模式按钮";
        m_controlNameMap["TBtn_ControlMode"] = "控制模式按钮";

        // StepMove 控件名称映射
        m_controlNameMap["StepMove_悬臂组件(J1)"] = "悬臂组件";
        m_controlNameMap["StepMove_升降组件(J2)"] = "升降组件";
        m_controlNameMap["StepMove_伸缩臂(J3)"] = "伸缩臂";
        m_controlNameMap["StepMove_柔顺组件(J4)"] = "柔顺组件";

        // 主导航与切换按钮
        m_controlNameMap["Btn_SwitchHorizontalSupport"] = "切换到水平支撑";
        m_controlNameMap["Btn_SwitchVerticalSupport"] = "切换到回转升降";
        m_controlNameMap["Btn_SwitchAGV"] = "切换到AGV页面";
        m_controlNameMap["Btn_SwitchEOAT"] = "切换到末端执行器";

        // 回转升降与 EOAT 操作按钮
        m_controlNameMap["TBtn_VeSupSec_Rise"] = "回转升降上升按钮";
        m_controlNameMap["TBtn_VeSupSec_Drop"] = "回转升降下降按钮";
        m_controlNameMap["TBtn_VeSupSec_RotLeft"] = "回转左转按钮";
        m_controlNameMap["TBtn_VeSupSec_RotRight"] = "回转右转按钮";

        m_controlNameMap["Btn_VeSupSec_GoHighest"] = "回转升降到最高点按钮";
        m_controlNameMap["Btn_VeSupSec_GoLowest"] = "回转升降到最低点按钮";
        m_controlNameMap["Btn_VeSupSec_GoCenter"] = "回转升降回到中点按钮";

        // AGV 相关
        m_controlNameMap["TBtn_AGV_Forward"] = "AGV 前进按钮";
        m_controlNameMap["TBtn_AGV_Backward"] = "AGV 后退按钮";
        m_controlNameMap["techBtn_AGV_OA"] = "AGV OA 按钮";
        m_controlNameMap["techBtn_AGV_驻车"] = "AGV 驻车按钮";
        m_controlNameMap["techBtn_AGVStep_UpLeft"] = "底盘步进 · 左上";
        m_controlNameMap["techBtn_AGVStep_Up"] = "底盘步进 · 上";
        m_controlNameMap["techBtn_AGVStep_UpRight"] = "底盘步进 · 右上";
        m_controlNameMap["techBtn_AGVStep_Left"] = "底盘步进 · 左";
        m_controlNameMap["techBtn_AGVStep_Rotate"] = "底盘步进 · 原地旋转";
        m_controlNameMap["techBtn_AGVStep_Right"] = "底盘步进 · 右";
        m_controlNameMap["techBtn_AGVStep_DownLeft"] = "底盘步进 · 左下";
        m_controlNameMap["techBtn_AGVStep_Down"] = "底盘步进 · 下";
        m_controlNameMap["techBtn_AGVStep_DownRight"] = "底盘步进 · 右下";

        // 力控相关
        m_controlNameMap["btn_ForceClear"] = "力控清除按钮";
        m_controlNameMap["btn_ForceControl"] = "力控切换按钮";
        m_controlNameMap["Btn_bigForceControl"] = "大力控按钮";
        m_controlNameMap["Btn_smallForceControl"] = "小力控按钮";

        // 其他常见 pushButton
        m_controlNameMap["pushButton_5"] = "自定义按钮5";
        m_controlNameMap["pushButton_6"] = "自定义按钮6";
    // AGV 控制按钮
    m_controlNameMap["TBtn_AGV_Forward"] = "AGV前进";
    m_controlNameMap["TBtn_AGV_Backward"] = "AGV后退";

    // ==================== 操作类型映射 ====================
    m_operationMap["valueChanged"] = "值发生改变";
    m_operationMap["valueChangedWithRecord"] = "值调节";
    m_operationMap["clicked"] = "点击";
    m_operationMap["pressed"] = "按下";
    m_operationMap["released"] = "释放";
    m_operationMap["toggled"] = "切换状态";
    m_operationMap["login_attempt"] = "登录尝试";
    m_operationMap["login_success"] = "登录成功";
    m_operationMap["login_fail"] = "登录失败";
    m_operationMap["client_connected"] = "客户端连接";
    m_operationMap["client_disconnected"] = "客户端断开";
    m_operationMap["listening"] = "开启监听";
    m_operationMap["connected"] = "建立连接";
    m_operationMap["disconnected"] = "断开连接";
    m_operationMap["client_disconnected"] = "客户端断开";
    m_operationMap["listening"] = "开启监听";
    m_operationMap["connected"] = "建立连接";
    m_operationMap["disconnected"] = "断开连接";
    m_operationMap["error"] = "错误";
    m_operationMap["force_control_toggled"] = "力控切换";
    m_operationMap["force_clear_pressed"] = "传感器清零";
    m_operationMap["mode_switch"] = "模式切换";
    m_operationMap["step_mode_changed"] = "步进模式变更";
    m_operationMap["step_move_start"] = "开始步进移动";
    m_operationMap["step_move_end"] = "结束步进移动";
    m_operationMap["external_motion_start"] = "外部运动触发";
    m_operationMap["external_motion_stop"] = "外部运动停止";
    m_operationMap["agv_external_motion_start"] = "AGV外部联动开始";
    m_operationMap["agv_external_motion_end"] = "AGV外部联动结束";
    m_operationMap["clear_alarm"] = "清除报警";
    m_operationMap["报警触发"] = "报警触发";
    m_operationMap["报警解除"] = "报警解除";
    m_operationMap["互锁触发"] = "互锁激活";
    m_operationMap["互锁解除"] = "互锁解除";
    m_operationMap["限制触发"] = "限制触发";
    m_operationMap["限制解除"] = "限制解除";
    m_operationMap["切换开始"] = "切换开始";
    m_operationMap["切换完成"] = "切换完成";
    m_operationMap["external_motion_start"] = "外部运动触发";
    m_operationMap["external_motion_stop"] = "外部运动停止";
    m_operationMap["agv_external_motion_start"] = "AGV外部联动开始";
    m_operationMap["agv_external_motion_end"] = "AGV外部联动结束";
    m_operationMap["clear_alarm"] = "清除报警";
    m_operationMap["报警触发"] = "报警触发";
    m_operationMap["报警解除"] = "报警解除";
    m_operationMap["互锁触发"] = "互锁激活";
    m_operationMap["互锁解除"] = "互锁解除";
    m_operationMap["限制触发"] = "限制触发";
    m_operationMap["限制解除"] = "限制解除";
    m_operationMap["切换开始"] = "切换开始";
    m_operationMap["切换完成"] = "切换完成";
    m_operationMap["logout"] = "注销";
    m_operationMap["oa_mode_changed"] = "避障模式变更";
    m_operationMap["steering_mode_changed"] = "转向模式变更";
    m_operationMap["unknown"] = "未知状态";
    m_operationMap["mode_changed"] = "运行模式已变更";
    m_operationMap["move_speed_changed"] = "运动速度已调整";
    m_operationMap["angle_changed"] = "角度已调整";
    m_operationMap["状态变化"] = "设备状态更新";
    m_operationMap["用户确认"] = "用户已确认";
    m_operationMap["unknown"] = "未知状态";
    m_operationMap["mode_changed"] = "运行模式已变更";
    m_operationMap["move_speed_changed"] = "运动速度已调整";
    m_operationMap["angle_changed"] = "角度已调整";
    m_operationMap["状态变化"] = "设备状态更新";
    m_operationMap["用户确认"] = "用户已确认";

    // ==================== 控件类型映射 ====================
    m_controlTypeMap["TechSliderEdit"] = "滑块控件";
    m_controlTypeMap["TechPushButton"] = "科技按钮";
    m_controlTypeMap["QPushButton"] = "普通按钮";
    m_controlTypeMap["QToolButton"] = "工具按钮";
    m_controlTypeMap["QLineEdit"] = "输入框";
    m_controlTypeMap["LoginAttempt"] = "登录尝试";
    m_controlTypeMap["LoginSuccess"] = "登录成功";
    m_controlTypeMap["LoginFail"] = "登录失败";
    m_controlTypeMap["Network"] = "网络服务";
    m_controlTypeMap["Network"] = "网络服务";
    m_controlTypeMap["EnableButton"] = "使能按钮";
    m_controlTypeMap["StepMove"] = "步进运动";
    m_controlTypeMap["ModbusTCP"] = "Modbus连接";
    m_controlTypeMap["ForceClear"] = "力控清零";
    m_controlTypeMap["Logout"] = "注销";
    m_controlTypeMap["MatrixKey"] = "物理按键";
    m_controlTypeMap["提示窗口"] = "系统弹窗";
    m_controlTypeMap["互锁监控"] = "互锁系统";
    m_controlTypeMap["限位监控"] = "限位系统";
    m_controlTypeMap["模式控制"] = "底盘模式";
    m_controlTypeMap["MatrixKey"] = "物理按键";
    m_controlTypeMap["提示窗口"] = "系统弹窗";
    m_controlTypeMap["互锁监控"] = "互锁系统";
    m_controlTypeMap["限位监控"] = "限位系统";
    m_controlTypeMap["模式控制"] = "底盘模式";

    // ==================== 值映射 ====================
    m_valueMap["true"] = "开启/激活";
    m_valueMap["false"] = "关闭/释放";
    m_valueMap["Host unreachable"] = "无法连接主机";
    m_valueMap["Connection refused"] = "连接被拒绝";
    m_valueMap["Socket timeout"] = "连接超时";
    m_valueMap["Unknown error"] = "未知错误";
    m_valueMap["Network unreachable"] = "网络不可达";
    m_valueMap["The remote host closed the connection"] = "远端连接已关闭";
    m_valueMap["Network unreachable"] = "网络不可达";
    m_valueMap["The remote host closed the connection"] = "远端连接已关闭";
    m_valueMap["主页"] = "返回主页";
    m_valueMap["伸缩臂"] = "伸缩臂页面";
    m_valueMap["回转升降"] = "回转升降页面";
    m_valueMap["EOAT控制"] = "末端执行器页面";
    m_valueMap["AGV控制"] = "AGV页面";

    // ==================== 常用按钮/控件映射补充（避免显示英文对象名） ====================
    m_controlNameMap["loginButton"] = "登录按钮";
    m_controlNameMap["logoutButton"] = "注销按钮";
    m_controlNameMap["recordClearBtn"] = "清空记录按钮";
    m_controlNameMap["recordSaveBtn"] = "保存记录按钮";
    m_controlNameMap["recordExportBtn"] = "导出报告按钮";
    m_controlNameMap["recordRefreshBtn"] = "刷新按钮";
    m_controlNameMap["recordBackBtn"] = "返回按钮";
    m_controlNameMap["sendAllRecordsBtn"] = "一键发送按钮";
    m_controlNameMap["tcpTransmissionCheck"] = "TCP传输开关";
    m_controlNameMap["filterCombo"] = "筛选下拉框";
    m_controlNameMap["recordDisplay"] = "记录显示区";
    m_controlNameMap["totalStats"] = "总记录";
    m_controlNameMap["todayStats"] = "今日记录";
    m_controlNameMap["sliderStats"] = "滑块计数";
    m_controlNameMap["buttonStats"] = "按钮计数";
    m_controlNameMap["toolButtonStats"] = "工具按钮计数";
    m_controlNameMap["btnStepTargetSixAxis1"] = "六轴步进目标 · 轴1";
    m_controlNameMap["btnStepTargetSixAxis2"] = "六轴步进目标 · 轴2";
    m_controlNameMap["btnStepTargetSixAxis3"] = "六轴步进目标 · 轴3";
    m_controlNameMap["btnStepTargetSixAxis4"] = "六轴步进目标 · 轴4";
    m_controlNameMap["btnStepTargetSixAxis5"] = "六轴步进目标 · 轴5";
    m_controlNameMap["btnStepTargetSixAxis6"] = "六轴步进目标 · 轴6";
    m_controlNameMap["btnStepTargetSixAxis7"] = "六轴步进目标 · 轴7";
    m_controlNameMap["btnStepTargetAxis1"] = "单轴步进目标 · 轴1";
    m_controlNameMap["btnStepTargetAxis2"] = "单轴步进目标 · 轴2";
    m_controlNameMap["btnStepTargetAxis3"] = "单轴步进目标 · 轴3";
    m_controlNameMap["btnStepTargetAxis4"] = "单轴步进目标 · 轴4";
    m_controlNameMap["btnStepTargetAgv"] = "AGV 步进目标";
    m_controlNameMap["btnStepTargetCable"] = "线缆/附加步进目标";
    m_controlNameMap["passwordEdit"] = "密码输入框";
    m_controlNameMap["TCP接收器"] = "TCP服务";
    m_controlNameMap["AGV Modbus连接"] = "AGV通讯";
    m_controlNameMap["Modbus连接"] = "主系统通讯";
    m_controlNameMap["主控限位提示"] = "限位报警窗口";
    m_controlNameMap["正限位报警"] = "正向限位";
    m_controlNameMap["负限位报警"] = "负向限位";
    m_controlNameMap["btnStepTargetSixAxis1"] = "六轴步进目标 · 轴1";
    m_controlNameMap["btnStepTargetSixAxis2"] = "六轴步进目标 · 轴2";
    m_controlNameMap["btnStepTargetSixAxis3"] = "六轴步进目标 · 轴3";
    m_controlNameMap["btnStepTargetSixAxis4"] = "六轴步进目标 · 轴4";
    m_controlNameMap["btnStepTargetSixAxis5"] = "六轴步进目标 · 轴5";
    m_controlNameMap["btnStepTargetSixAxis6"] = "六轴步进目标 · 轴6";
    m_controlNameMap["btnStepTargetSixAxis7"] = "六轴步进目标 · 轴7";
    m_controlNameMap["btnStepTargetAxis1"] = "单轴步进目标 · 轴1";
    m_controlNameMap["btnStepTargetAxis2"] = "单轴步进目标 · 轴2";
    m_controlNameMap["btnStepTargetAxis3"] = "单轴步进目标 · 轴3";
    m_controlNameMap["btnStepTargetAxis4"] = "单轴步进目标 · 轴4";
    m_controlNameMap["btnStepTargetAgv"] = "AGV 步进目标";
    m_controlNameMap["btnStepTargetCable"] = "线缆/附加步进目标";
    m_controlNameMap["passwordEdit"] = "密码输入框";
    m_controlNameMap["TCP接收器"] = "TCP服务";
    m_controlNameMap["AGV Modbus连接"] = "AGV通讯";
    m_controlNameMap["Modbus连接"] = "主系统通讯";
    m_controlNameMap["主控限位提示"] = "限位报警窗口";
    m_controlNameMap["正限位报警"] = "正向限位";
    m_controlNameMap["负限位报警"] = "负向限位";

    // ==================== 页面名称映射 ====================
    // 使用页面索引作为key
    m_pageNameMap["0"] = "首页/控制";
    m_pageNameMap["1"] = "回转升降控制";
    m_pageNameMap["2"] = "伸缩臂控制";
    m_pageNameMap["3"] = "末端执行器控制";
    m_pageNameMap["4"] = "AGV小车控制";
    m_pageNameMap["5"] = "管理员验证";
    m_pageNameMap["6"] = "操作记录查看";

    // 也可以使用页面对象名作为key
    m_pageNameMap["softwareParamPage"] = "软件参数页面";
    m_pageNameMap["verticalSupportPage"] = "回转升降控制";
    m_pageNameMap["六自由度"] = "六自由度控制";
    m_pageNameMap["机械臂"] = "机械臂调试";
    m_pageNameMap["提示系统"] = "全局消息";
    m_pageNameMap["六自由度"] = "六自由度控制";
    m_pageNameMap["机械臂"] = "机械臂调试";
    m_pageNameMap["提示系统"] = "全局消息";
    m_pageNameMap["系统"] = "系统设置";
    m_pageNameMap["AGV控制"] = "AGV控制中心";
    m_pageNameMap["操作记录"] = "历史操作记录";
    m_pageNameMap["页面0"] = "控制主页";
    m_pageNameMap["未知页面"] = "通用/导航";
    m_pageNameMap["权限验证"] = "权限登录";
    // ... 其他页面
}

QString MappingConfig::mapControlName(const QString &objectName) const
{
    QString name = objectName;

    // 先查找精确匹配
    if (m_controlNameMap.contains(objectName)) {
        name = m_controlNameMap[objectName];
    } else if (objectName.contains("steeringModeSelector")) {
        // 特殊情况：包含 "steeringModeSelector" 的映射
        name = "转向模式选择器";
    } else if (objectName.startsWith("VeSupSec_")) {
        // 如果没有精确匹配，尝试部分匹配（按前缀）
        name = QString("回转升降模块-%1").arg(objectName);
    } else if (objectName.startsWith("HoSupSec_")) {
        name = QString("伸缩臂模块-%1").arg(objectName);
    } else if (objectName.startsWith("EOAT_")) {
        name = QString("末端执行器-%1").arg(objectName);
    } else if (objectName.startsWith("AGV_")) {
        name = QString("AGV小车-%1").arg(objectName);
    }

    // 全局移除 "按钮" 两个字
    name.remove("按钮");
    return name;
}

QString MappingConfig::mapOperation(const QString &operation) const
{
    return m_operationMap.value(operation, operation);
}

QString MappingConfig::mapControlType(const QString &controlType) const
{
    return m_controlTypeMap.value(controlType, controlType);
}

QString MappingConfig::mapPageName(const QString &pageKey) const
{
    return m_pageNameMap.value(pageKey, pageKey);
}

QString MappingConfig::mapValue(const QString &value) const
{
    const QString trimmed = value.trimmed();
    const QString lower = trimmed.toLower();
    if (lower == QLatin1String("true")) {
        return m_valueMap.value(QStringLiteral("true"), trimmed);
    }
    if (lower == QLatin1String("false")) {
        return m_valueMap.value(QStringLiteral("false"), trimmed);
    }

    if (m_valueMap.contains(trimmed)) {
        return m_valueMap[trimmed];
    }

    // 支持模糊匹配
    if (trimmed.contains(QLatin1String("Host unreachable"))) {
        return QStringLiteral("主机不可达 (AGV失联)");
    }

    return value;
}

QVariant MappingConfig::mapVariantForDisplay(const QVariant &value) const
{
    if (!value.isValid()) {
        return value;
    }

    switch (value.type()) {
    case QVariant::Bool:
        return mapValue(value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
    case QVariant::Double:
        return value;
    default:
        return mapValue(value.toString());
    }
}

void MappingConfig::normalizeOperationRecord(OperationRecord &record) const
{
    // 1. 记录原始名称以便逻辑判断
    QString originalControlName = record.controlName;
    QString originalControlType = record.controlType;

    // 2. 映射基本文字
    record.pageName = mapPageName(record.pageName);
    record.controlName = mapControlName(record.controlName);
    record.operation = mapOperation(record.operation);
    record.controlType = mapControlType(record.controlType);
    record.oldValue = mapVariantForDisplay(record.oldValue);
    record.newValue = mapVariantForDisplay(record.newValue);

    // 3. 处理：有页面的显示“切换到XX页面”，没有页面的显示“切换到XX”
    // 判断逻辑：如果控件名包含“切换”且 newValue 有效
    if (record.controlName.contains("切换") && !record.newValue.toString().isEmpty()) {
        QString target = record.newValue.toString();
        if (target.contains("页面")) {
            record.operation = QString("切换到%1").arg(target);
        } else {
            record.operation = QString("切换到%1").arg(target); // 统一逻辑，如果值里没带页面就在映射逻辑里处理
            // 如果 newValue 已经是映射后的（如“伸缩臂页面”），则直接使用
        }
        // 既然已经揉到了操作详情，清空值显示避免重复
        record.oldValue = QVariant();
        record.newValue = QVariant();
    }

    // 4. 处理：groupBox 或 steeringModeSelector 等切换时不记录关闭动作
    // 如果是切换动作（newValue 为 true），保留；如果是关闭动作（newValue 为 false），则清空
    bool isSelector = originalControlName.contains("groupBox") || 
                      originalControlName.contains("steeringModeSelector") ||
                      originalControlName.contains("Btn_Switch");
    
    if (isSelector && record.newValue.toString() == mapValue("false")) {
        // 将此记录标记为“不显示” (在 QML 中 height 为 0)
        // 简单做法是清空内容
        record.controlName = "";
        record.operation = "IGNORE_LOG"; 
    }

    // 5. 历史列表：去掉「写某寄存器某值」等技术尾巴，保留用户操作描述
    record.oldValue = scrubHistoryValueVariant(record.oldValue);
    record.newValue = scrubHistoryValueVariant(record.newValue);
    if (historyTextLooksLikeRegisterWriteDetail(record.operation)) {
        const QString op = scrubOperationHistoryRegisterWriteText(record.operation);
        record.operation = op.trimmed().isEmpty() ? QString() : op;
    }
    if (historyTextLooksLikeRegisterWriteDetail(record.controlName)) {
        const QString cn = scrubOperationHistoryRegisterWriteText(record.controlName);
        if (!cn.trimmed().isEmpty()) {
            record.controlName = cn;
        }
    }
}

void MappingConfig::addControlMapping(const QString &objectName, const QString &displayName)
{
    m_controlNameMap[objectName] = displayName;
}

void MappingConfig::addOperationMapping(const QString &operation, const QString &displayText)
{
    m_operationMap[operation] = displayText;
}

void MappingConfig::addControlTypeMapping(const QString &controlType, const QString &displayText)
{
    m_controlTypeMap[controlType] = displayText;
}
