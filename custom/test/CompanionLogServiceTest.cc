#include "CompanionLogServiceTest.h"

#include <QtTest/QSignalSpy>

#include "CompanionLogService.h"

UT_REGISTER_TEST(CompanionLogServiceTest, TestLabel::Unit)

void CompanionLogServiceTest::_testLogAddAndSignals()
{
    CompanionLogService service;
    QSignalSpy addedSpy(&service, &CompanionLogService::logMessageAdded);
    QSignalSpy changedSpy(&service, &CompanionLogService::logEntriesChanged);

    service.logMavlink(QStringLiteral("FC"), QStringLiteral("RX"), QStringLiteral("hello"));  // default severity 6

    QCOMPARE(addedSpy.count(), 1);
    QVERIFY(changedSpy.count() >= 1);

    const QVariantList all = service.getLogHistory(QStringLiteral("ALL"));
    QCOMPARE(all.size(), 1);
    const QVariantMap entry = all.first().toMap();
    QCOMPARE(entry.value(QStringLiteral("category")).toString(), QStringLiteral("FC"));
    QCOMPARE(entry.value(QStringLiteral("direction")).toString(), QStringLiteral("RX"));
    QCOMPARE(entry.value(QStringLiteral("message")).toString(), QStringLiteral("hello"));
    QCOMPARE(entry.value(QStringLiteral("severity")).toInt(), 6);
}

void CompanionLogServiceTest::_testCategoryFilterIsCaseInsensitive()
{
    CompanionLogService service;
    service.logMavlink(QStringLiteral("FC"), QStringLiteral("RX"), QStringLiteral("a"));
    service.logMavlink(QStringLiteral("SIYI"), QStringLiteral("TX"), QStringLiteral("b"));

    QCOMPARE(service.getLogHistory(QStringLiteral("FC")).size(), 1);
    QCOMPARE(service.getLogHistory(QStringLiteral("siyi")).size(), 1);  // case-insensitive match
    QCOMPARE(service.getLogHistory(QStringLiteral("ALL")).size(), 2);
    QCOMPARE(service.getLogHistory(QStringLiteral("UNKNOWN")).size(), 0);
}

void CompanionLogServiceTest::_testClearSingleCategory()
{
    CompanionLogService service;
    service.logMavlink(QStringLiteral("FC"), QStringLiteral("RX"), QStringLiteral("a"));
    service.logMavlink(QStringLiteral("SIYI"), QStringLiteral("TX"), QStringLiteral("b"));

    service.clearLogHistory(QStringLiteral("FC"));

    const QVariantList remaining = service.getLogHistory(QStringLiteral("ALL"));
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining.first().toMap().value(QStringLiteral("category")).toString(), QStringLiteral("SIYI"));
}

void CompanionLogServiceTest::_testClearAll()
{
    CompanionLogService service;
    service.logMavlink(QStringLiteral("FC"), QStringLiteral("RX"), QStringLiteral("a"));
    service.logMavlink(QStringLiteral("SIYI"), QStringLiteral("TX"), QStringLiteral("b"));

    service.clearLogHistory(QStringLiteral("ALL"));

    QCOMPARE(service.getLogHistory(QStringLiteral("ALL")).size(), 0);
}

void CompanionLogServiceTest::_testRingBufferCap()
{
    CompanionLogService service;
    // kMaxLogEntries is 500; adding more must drop the oldest and cap the history.
    for (int i = 0; i < 520; ++i) {
        service.logMavlink(QStringLiteral("NET"), QStringLiteral("RX"), QStringLiteral("msg %1").arg(i));
    }

    QCOMPARE(service.getLogHistory(QStringLiteral("ALL")).size(), 500);
    // The oldest surviving entry is "msg 20" (0..19 were evicted).
    QCOMPARE(service.getLogHistory(QStringLiteral("ALL")).first().toMap().value(QStringLiteral("message")).toString(),
             QStringLiteral("msg 20"));
}
