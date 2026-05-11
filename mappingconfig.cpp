#include "mappingconfig.h"

#include "operationrecorder.h"

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
        m_controlNameMap["TBtn_HomePage"] = "首页按钮";
        m_controlNameMap["TBtn_PermissionPage"] = "权限页面按钮";
        m_controlNameMap["TBtn_Stepmove"] = "步进模式按钮";
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
    m_operationMap["logout"] = "注销";
    m_operationMap["oa_mode_changed"] = "避障模式变更";
    m_operationMap["steering_mode_changed"] = "转向模式变更";
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

    // ==================== 值映射 ====================
    m_valueMap["true"] = "开启/激活";
    m_valueMap["false"] = "关闭/释放";
    m_valueMap["Host unreachable"] = "无法连接主机";
    m_valueMap["Connection refused"] = "连接被拒绝";
    m_valueMap["Socket timeout"] = "连接超时";
    m_valueMap["Unknown error"] = "未知错误";
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
    // 先查找精确匹配
    if (m_controlNameMap.contains(objectName)) {
        return m_controlNameMap[objectName];
    }
    
    // 特殊情况：包含 "steeringModeSelector" 的映射
    if (objectName.contains("steeringModeSelector")) {
        return "转向模式选择器";
    }

    // 如果没有精确匹配，尝试部分匹配（按前缀）
    // 例如：所有 VeSupSec 开头的控件都显示为"回转升降"
    if (objectName.startsWith("VeSupSec_")) {
        return QString("回转升降模块-%1").arg(objectName);
    }
    if (objectName.startsWith("HoSupSec_")) {
        return QString("伸缩臂模块-%1").arg(objectName);
    }
    if (objectName.startsWith("EOAT_")) {
        return QString("末端执行器-%1").arg(objectName);
    }
    if (objectName.startsWith("AGV_")) {
        return QString("AGV小车-%1").arg(objectName);
    }

    // ==================== 动态词条处理 ====================
    // 如果 newValue 包含 "Host unreachable" 等英文错误，这里无法直接在 mapControlName 处理，
    // 因为这是值，不是控件名或操作名。
    // 如果需要翻译 newValue，通常应在 UI 显示层的业务逻辑中进行。

    // 如果还是没有匹配，返回原始名称
    return objectName;
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
    record.pageName = mapPageName(record.pageName);
    record.controlName = mapControlName(record.controlName);
    record.operation = mapOperation(record.operation);
    record.controlType = mapControlType(record.controlType);
    record.oldValue = mapVariantForDisplay(record.oldValue);
    record.newValue = mapVariantForDisplay(record.newValue);
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
