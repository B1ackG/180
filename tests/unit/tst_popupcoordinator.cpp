#include "popupcoordinator.h"

#include <QtTest>

class TestPopupCoordinator : public QObject
{
    Q_OBJECT
private slots:
    void noPopupBeforeInitialization();
    void emergencyHasHighestPriority();
    void onlyOnePopupIsSelected();
    void lowerPriorityRestoresAfterEmergency();
    void axisDeviationDoesNotOverlapEmergency();
    void unchangedSnapshotProducesNoTransition();
    void switchHidesOldBeforeShowingNew();
    void acknowledgedConditionIsExcludedBySnapshot();
    void operationPopupDoesNotRestoreAfterRequestCleared();
};

void TestPopupCoordinator::noPopupBeforeInitialization()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = false;
    snapshot.emergencyStop = true;
    snapshot.agvStationOffline = true;
    QCOMPARE(coordinator.choose(snapshot), PopupId::None);
}

void TestPopupCoordinator::emergencyHasHighestPriority()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.emergencyStop = true;
    snapshot.inclinometerTiltLock = true;
    snapshot.robotWeightLock = true;
    snapshot.robotAxisSyncDeviation = true;
    snapshot.agvDriveFault = true;
    snapshot.agvStationOffline = true;
    snapshot.stepMotionWaiting = true;
    QCOMPARE(coordinator.choose(snapshot), PopupId::EmergencyStop);
}

void TestPopupCoordinator::onlyOnePopupIsSelected()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.emergencyStop = true;
    snapshot.inclinometerTiltLock = true;
    snapshot.robotWeightLock = true;
    snapshot.robotAxisSyncDeviation = true;
    snapshot.agvDriveFault = true;
    snapshot.agvStationOffline = true;
    snapshot.robotWeightOverload = true;
    snapshot.parkingLegAbnormal = true;
    snapshot.inclinometerTiltRisk = true;
    snapshot.robotInterlockRequested = true;
    snapshot.legOpenPathCheckActive = true;
    snapshot.sixAxisInitConfirmRequested = true;
    snapshot.legControlRequested = true;
    snapshot.steeringSwitchWaiting = true;
    snapshot.parkingSwitchWaiting = true;
    snapshot.sixAxisPoseWaiting = true;
    snapshot.stepMotionWaiting = true;
    QCOMPARE(coordinator.choose(snapshot), PopupId::EmergencyStop);
}

void TestPopupCoordinator::lowerPriorityRestoresAfterEmergency()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.agvStationOffline = true;

    const PopupTransition shown = coordinator.reconcile(snapshot);
    QCOMPARE(shown.show, PopupId::AgvStationOffline);

    snapshot.emergencyStop = true;
    const PopupTransition covered = coordinator.reconcile(snapshot);
    QCOMPARE(covered.hide, PopupId::AgvStationOffline);
    QCOMPARE(covered.show, PopupId::EmergencyStop);

    snapshot.emergencyStop = false;
    const PopupTransition restored = coordinator.reconcile(snapshot);
    QCOMPARE(restored.hide, PopupId::EmergencyStop);
    QCOMPARE(restored.show, PopupId::AgvStationOffline);
}

void TestPopupCoordinator::axisDeviationDoesNotOverlapEmergency()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.robotAxisSyncDeviation = true;
    snapshot.emergencyStop = true;
    QCOMPARE(coordinator.choose(snapshot), PopupId::EmergencyStop);

    snapshot.emergencyStop = false;
    QCOMPARE(coordinator.choose(snapshot), PopupId::RobotAxisSyncDeviation);
}

void TestPopupCoordinator::unchangedSnapshotProducesNoTransition()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.agvDriveFault = true;
    coordinator.reconcile(snapshot);

    const PopupTransition repeat = coordinator.reconcile(snapshot);
    QCOMPARE(repeat.hide, PopupId::None);
    QCOMPARE(repeat.show, PopupId::None);
    QCOMPARE(coordinator.current(), PopupId::AgvDriveFault);
}

void TestPopupCoordinator::switchHidesOldBeforeShowingNew()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.agvStationOffline = true;
    coordinator.reconcile(snapshot);

    snapshot.emergencyStop = true;
    const PopupTransition transition = coordinator.reconcile(snapshot);
    QCOMPARE(transition.hide, PopupId::AgvStationOffline);
    QCOMPARE(transition.show, PopupId::EmergencyStop);
}

void TestPopupCoordinator::acknowledgedConditionIsExcludedBySnapshot()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.robotWeightOverload = false;
    snapshot.inclinometerTiltRisk = true;
    QCOMPARE(coordinator.choose(snapshot), PopupId::InclinometerTiltRisk);
}

void TestPopupCoordinator::operationPopupDoesNotRestoreAfterRequestCleared()
{
    PopupCoordinator coordinator;
    PopupSnapshot snapshot;
    snapshot.userPopupsAllowed = true;
    snapshot.legControlRequested = true;
    QCOMPARE(coordinator.reconcile(snapshot).show, PopupId::LegControl);

    snapshot.emergencyStop = true;
    snapshot.legControlRequested = false;
    const PopupTransition covered = coordinator.reconcile(snapshot);
    QCOMPARE(covered.show, PopupId::EmergencyStop);

    snapshot.emergencyStop = false;
    const PopupTransition after = coordinator.reconcile(snapshot);
    QCOMPARE(after.hide, PopupId::EmergencyStop);
    QCOMPARE(after.show, PopupId::None);
    QCOMPARE(coordinator.choose(snapshot), PopupId::None);
}

QTEST_APPLESS_MAIN(TestPopupCoordinator)
#include "tst_popupcoordinator.moc"
