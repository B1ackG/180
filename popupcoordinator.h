#ifndef POPUPCOORDINATOR_H
#define POPUPCOORDINATOR_H

#include <QtGlobal>

/**
 * @brief 居中阻塞弹窗编号。数值顺序不是优先级，优先级写在 PopupCoordinator::choose。
 */
enum class PopupId : quint8 {
    None,
    EmergencyStop,
    InclinometerTiltLock,
    RobotWeightLock,
    RobotAxisSyncDeviation,
    AgvDriveFault,
    AgvStationOffline,
    RobotWeightOverload,
    ParkingLegAbnormal,
    InclinometerTiltRisk,
    RobotInterlock,
    LegOpenPathCheck,
    SixAxisInitConfirm,
    LegControl,
    SteeringSwitchWait,
    ParkingSwitchWait,
    SixAxisPoseWait,
    StepMotionWait
};

/**
 * @brief 当前是否需要显示各居中弹窗。确认位和互斥条件由调用方先算好。
 */
struct PopupSnapshot {
    bool userPopupsAllowed = false;
    bool emergencyStop = false;
    bool inclinometerTiltLock = false;
    bool robotWeightLock = false;
    bool robotAxisSyncDeviation = false;
    bool agvDriveFault = false;
    bool agvStationOffline = false;
    bool robotWeightOverload = false;
    bool parkingLegAbnormal = false;
    bool inclinometerTiltRisk = false;
    bool robotInterlockRequested = false;
    bool legOpenPathCheckActive = false;
    bool sixAxisInitConfirmRequested = false;
    bool legControlRequested = false;
    bool steeringSwitchWaiting = false;
    bool parkingSwitchWaiting = false;
    bool sixAxisPoseWaiting = false;
    bool stepMotionWaiting = false;
};

/**
 * @brief 一次显示切换。hide 先执行，show 后执行。两个都是 None 表示不用动界面。
 */
struct PopupTransition {
    PopupId hide = PopupId::None;
    PopupId show = PopupId::None;
};

/**
 * @brief 只根据快照选择唯一居中弹窗，不创建控件、不读写设备。
 */
class PopupCoordinator
{
public:
    PopupId choose(const PopupSnapshot &snapshot) const;
    PopupTransition reconcile(const PopupSnapshot &snapshot);
    PopupId current() const;
    void reset();

private:
    PopupId m_current = PopupId::None;
};

#endif // POPUPCOORDINATOR_H
