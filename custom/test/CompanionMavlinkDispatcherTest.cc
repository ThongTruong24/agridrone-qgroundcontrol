#include "CompanionMavlinkDispatcherTest.h"

#include "CompanionMavlinkDispatcher.h"
#include "ITelemetryHandler.h"
#include "QGCMAVLink.h"

UT_REGISTER_TEST(CompanionMavlinkDispatcherTest, TestLabel::Unit)

namespace {
class FakeTelemetryHandler : public ITelemetryHandler
{
public:
    explicit FakeTelemetryHandler(bool handleResult) : _handleResult(handleResult) {}

    bool handleMavlinkMessage(const mavlink_message_t&) override
    {
        ++handleCount;
        return _handleResult;
    }

    void resetState() override { ++resetCount; }

    int handleCount = 0;
    int resetCount = 0;

private:
    bool _handleResult = true;
};

mavlink_message_t makeHeartbeat()
{
    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(1, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4, 0, 0, MAV_STATE_ACTIVE);
    return message;
}
}  // namespace

void CompanionMavlinkDispatcherTest::_testDispatchFansOutToAllHandlers()
{
    CompanionMavlinkDispatcher dispatcher;
    FakeTelemetryHandler consuming(true);
    FakeTelemetryHandler nonConsuming(false);
    dispatcher.registerHandler(&consuming);
    dispatcher.registerHandler(&nonConsuming);

    const mavlink_message_t message = makeHeartbeat();

    // Returns true because at least one handler reported handling the message...
    QVERIFY(dispatcher.dispatchMessage(message));
    // ...but every handler is still offered the message (no early-out).
    QCOMPARE(consuming.handleCount, 1);
    QCOMPARE(nonConsuming.handleCount, 1);
}

void CompanionMavlinkDispatcherTest::_testNoDuplicateRegistration()
{
    CompanionMavlinkDispatcher dispatcher;
    FakeTelemetryHandler handler(true);
    dispatcher.registerHandler(&handler);
    dispatcher.registerHandler(&handler);  // duplicate ignored

    dispatcher.dispatchMessage(makeHeartbeat());

    QCOMPARE(handler.handleCount, 1);
}

void CompanionMavlinkDispatcherTest::_testUnregisterStopsDelivery()
{
    CompanionMavlinkDispatcher dispatcher;
    FakeTelemetryHandler handler(true);
    dispatcher.registerHandler(&handler);
    dispatcher.unregisterHandler(&handler);

    QVERIFY(!dispatcher.dispatchMessage(makeHeartbeat()));
    QCOMPARE(handler.handleCount, 0);
}

void CompanionMavlinkDispatcherTest::_testResetAllHandlers()
{
    CompanionMavlinkDispatcher dispatcher;
    FakeTelemetryHandler first(true);
    FakeTelemetryHandler second(false);
    dispatcher.registerHandler(&first);
    dispatcher.registerHandler(&second);

    dispatcher.resetAllHandlers();

    QCOMPARE(first.resetCount, 1);
    QCOMPARE(second.resetCount, 1);
}

void CompanionMavlinkDispatcherTest::_testNullHandlerAndEmptyDispatch()
{
    CompanionMavlinkDispatcher dispatcher;
    // A null handler must be ignored, not stored.
    dispatcher.registerHandler(nullptr);
    // Dispatching with no live handlers is a safe no-op returning false.
    QVERIFY(!dispatcher.dispatchMessage(makeHeartbeat()));
    dispatcher.resetAllHandlers();  // must not crash
}
