#include "modbusregistercodec.h"
#include "motionwriteplan.h"
#include "modbusthreadmanager.h"
#include "sixaxisrunmode.h"

#include <QtTest>

class TestMotionWritePlan : public QObject
{
    Q_OBJECT
private slots:
    void rejectsAxisOutsideOneToSix();
    void buildsThreeStepsInOrder();
    void sixAxisRunModeCycle();
};

void TestMotionWritePlan::rejectsAxisOutsideOneToSix()
{
    SixAxisStepWritePlan plan;
    plan.steps.append(ModbusWriteCommand());
    QVERIFY(!buildSixAxisStepWritePlan(0, 1.0f, 3, plan));
    QVERIFY(plan.steps.isEmpty());
    QVERIFY(!buildSixAxisStepWritePlan(7, 1.0f, 3, plan));
    QVERIFY(plan.steps.isEmpty());
}

void TestMotionWritePlan::buildsThreeStepsInOrder()
{
    SixAxisStepWritePlan plan;
    QVERIFY(buildSixAxisStepWritePlan(3, 1.5f, 9, plan));
    QCOMPARE(plan.steps.size(), 3);

    QCOMPARE(plan.steps.at(0).start, 614);
    QCOMPARE(plan.steps.at(0).values, QVector<quint16>({3}));
    QCOMPARE(plan.steps.at(1).start, 601);
    const auto regs = ModbusRegisterCodec::floatToCdab(1.5f);
    QCOMPARE(plan.steps.at(1).values, QVector<quint16>({regs[0], regs[1]}));
    QCOMPARE(plan.steps.at(2).start, 615);
    QCOMPARE(plan.steps.at(2).values, QVector<quint16>({1}));

    for (const ModbusWriteCommand &step : plan.steps) {
        QCOMPARE(step.generation, quint64(9));
        QCOMPARE(step.generationChannel, ModbusThreadManager::kSixAxisExternalWriteChannel);
    }
}

void TestMotionWritePlan::sixAxisRunModeCycle()
{
    QCOMPARE(nextSixAxisRunMode(SixAxisRunMode::Jog), SixAxisRunMode::Step);
    QCOMPARE(nextSixAxisRunMode(SixAxisRunMode::Step), SixAxisRunMode::Diagonal);
    QCOMPARE(nextSixAxisRunMode(SixAxisRunMode::Diagonal), SixAxisRunMode::Jog);
    QCOMPARE(nextSixAxisRunMode(SixAxisRunMode::Unknown), SixAxisRunMode::Jog);
    QCOMPARE(sixAxisRunModeFromRegister(1), SixAxisRunMode::Jog);
    QCOMPARE(sixAxisRunModeFromRegister(2), SixAxisRunMode::Step);
    QCOMPARE(sixAxisRunModeFromRegister(3), SixAxisRunMode::Diagonal);
    QCOMPARE(sixAxisRunModeFromRegister(0), SixAxisRunMode::Unknown);
    QCOMPARE(sixAxisRunModeFromRegister(9), SixAxisRunMode::Unknown);
}

QTEST_APPLESS_MAIN(TestMotionWritePlan)
#include "tst_motionwriteplan.moc"
