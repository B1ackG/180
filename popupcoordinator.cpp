#include "popupcoordinator.h"

namespace {

bool snapshotFlag(const PopupSnapshot &snapshot, PopupId id)
{
    switch (id) {
    case PopupId::EmergencyStop:
        return snapshot.emergencyStop;
    case PopupId::InclinometerTiltLock:
        return snapshot.inclinometerTiltLock;
    case PopupId::RobotWeightLock:
        return snapshot.robotWeightLock;
    case PopupId::RobotAxisSyncDeviation:
        return snapshot.robotAxisSyncDeviation;
    case PopupId::AgvDriveFault:
        return snapshot.agvDriveFault;
    case PopupId::AgvStationOffline:
        return snapshot.agvStationOffline;
    case PopupId::RobotWeightOverload:
        return snapshot.robotWeightOverload;
    case PopupId::ParkingLegAbnormal:
        return snapshot.parkingLegAbnormal;
    case PopupId::InclinometerTiltRisk:
        return snapshot.inclinometerTiltRisk;
    case PopupId::RobotInterlock:
        return snapshot.robotInterlockRequested;
    case PopupId::LegOpenPathCheck:
        return snapshot.legOpenPathCheckActive;
    case PopupId::SixAxisInitConfirm:
        return snapshot.sixAxisInitConfirmRequested;
    case PopupId::LegControl:
        return snapshot.legControlRequested;
    case PopupId::SteeringSwitchWait:
        return snapshot.steeringSwitchWaiting;
    case PopupId::ParkingSwitchWait:
        return snapshot.parkingSwitchWaiting;
    case PopupId::SixAxisPoseWait:
        return snapshot.sixAxisPoseWaiting;
    case PopupId::StepMotionWait:
        return snapshot.stepMotionWaiting;
    case PopupId::None:
        return false;
    }
    return false;
}

} // namespace

PopupId PopupCoordinator::choose(const PopupSnapshot &snapshot) const
{
    if (!snapshot.userPopupsAllowed) {
        return PopupId::None;
    }

    static const PopupId kPriority[] = {
        PopupId::EmergencyStop,
        PopupId::InclinometerTiltLock,
        PopupId::RobotWeightLock,
        PopupId::RobotAxisSyncDeviation,
        PopupId::AgvDriveFault,
        PopupId::AgvStationOffline,
        PopupId::RobotWeightOverload,
        PopupId::ParkingLegAbnormal,
        PopupId::InclinometerTiltRisk,
        PopupId::RobotInterlock,
        PopupId::LegOpenPathCheck,
        PopupId::SixAxisInitConfirm,
        PopupId::LegControl,
        PopupId::SteeringSwitchWait,
        PopupId::ParkingSwitchWait,
        PopupId::SixAxisPoseWait,
        PopupId::StepMotionWait,
    };

    for (PopupId id : kPriority) {
        if (snapshotFlag(snapshot, id)) {
            return id;
        }
    }
    return PopupId::None;
}

PopupTransition PopupCoordinator::reconcile(const PopupSnapshot &snapshot)
{
    const PopupId next = choose(snapshot);
    PopupTransition transition;
    if (next == m_current) {
        return transition;
    }
    transition.hide = m_current;
    transition.show = next;
    m_current = next;
    return transition;
}

PopupId PopupCoordinator::current() const
{
    return m_current;
}

void PopupCoordinator::reset()
{
    m_current = PopupId::None;
}
