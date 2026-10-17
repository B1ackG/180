#include "modbusregistercodec.h"
#include "motionwriteplan.h"
#include "modbusthreadmanager.h"

#include <QtTest>

class TestMotionWritePlan : public QObject
{
    Q_OBJECT
private slots:
    void rejectsAxisOutsideOneToSix();
    void buildsThreeStepsInOrder();
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

QTEST_APPLESS_MAIN(TestMotionWritePlan)
#include "tst_motionwriteplan.moc"
