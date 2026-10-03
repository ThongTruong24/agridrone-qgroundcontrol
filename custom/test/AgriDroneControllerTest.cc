#include "AgriDroneControllerTest.h"

#include <QtTest/QSignalSpy>

#include "AgriDroneController.h"
#include "Fact.h"
#include "FactMetaData.h"

UT_REGISTER_TEST(AgriDroneControllerTest, TestLabel::Unit, TestLabel::Vehicle)

namespace {
// A standalone Fact that mimics a THACO actuator-default parameter (uint8, 0..255).
Fact* makeActuatorFact(QObject* parent)
{
    Fact* const fact = new Fact(0, QStringLiteral("THACO_A1_DEF"), FactMetaData::valueTypeUint8, parent);
    fact->setRawValue(QVariant(0));
    return fact;
}

constexpr int kActuatorOnValue = 255;
}  // namespace

void AgriDroneControllerTest::_testEnabledToggle()
{
    AgriDroneController controller;
    QSignalSpy enabledSpy(&controller, &AgriDroneController::enabledChanged);

    QVERIFY(!controller.enabled());

    controller.setEnabled(true);
    QVERIFY(controller.enabled());
    QCOMPARE(enabledSpy.count(), 1);

    controller.setEnabled(true);  // idempotent: no duplicate signal
    QCOMPARE(enabledSpy.count(), 1);

    controller.setEnabled(false);
    QVERIFY(!controller.enabled());
    QCOMPARE(enabledSpy.count(), 2);
}

void AgriDroneControllerTest::_testDefaultActuatorStates()
{
    AgriDroneController controller;

    QVERIFY(!controller.anyActuatorAvailable());
    QVERIFY(!controller.actuatorAvailable());
    QVERIFY(!controller.actuatorOn());

    const QVariantList available = controller.actuatorAvailableStates();
    const QVariantList on = controller.actuatorOnStates();
    QCOMPARE(available.size(), 4);
    QCOMPARE(on.size(), 4);
    for (const QVariant& state : available) {
        QVERIFY(!state.toBool());
    }
    for (const QVariant& state : on) {
        QVERIFY(!state.toBool());
    }
}

void AgriDroneControllerTest::_testFactDrivenAvailability()
{
    AgriDroneController controller;
    QSignalSpy availableSpy(&controller, &AgriDroneController::actuatorAvailableChanged);

    Fact* const fact = makeActuatorFact(&controller);
    controller._setActuatorFact(0, fact);  // white-box injection via friend access

    QVERIFY(controller.actuatorAvailable());
    QVERIFY(controller.anyActuatorAvailable());
    QCOMPARE(availableSpy.count(), 1);

    controller._setActuatorFact(0, nullptr);
    QVERIFY(!controller.actuatorAvailable());
    QVERIFY(!controller.anyActuatorAvailable());
    QCOMPARE(availableSpy.count(), 2);
}

void AgriDroneControllerTest::_testFactDrivenOnState()
{
    AgriDroneController controller;
    QSignalSpy onSpy(&controller, &AgriDroneController::actuatorOnChanged);

    Fact* const fact = makeActuatorFact(&controller);
    controller._setActuatorFact(0, fact);
    QVERIFY(!controller.actuatorOn());

    fact->setRawValue(QVariant(kActuatorOnValue));
    QVERIFY(controller.actuatorOn());
    QVERIFY(onSpy.count() >= 1);

    fact->setRawValue(QVariant(0));
    QVERIFY(!controller.actuatorOn());
}

void AgriDroneControllerTest::_testSetActuatorOnGuardedWithoutVehicle()
{
    AgriDroneController controller;
    Fact* const fact = makeActuatorFact(&controller);
    controller._setActuatorFact(0, fact);
    controller.setEnabled(true);

    // No active vehicle, so setActuatorOn must be a safe no-op and leave the fact untouched.
    controller.setActuatorOn(0, true);
    QCOMPARE(fact->rawValue().toInt(), 0);
    QVERIFY(!controller.actuatorOn());

    // Out-of-range indices must also be ignored without crashing.
    controller.setActuatorOn(-1, true);
    controller.setActuatorOn(99, true);
    QVERIFY(!controller.actuatorOn());
}
