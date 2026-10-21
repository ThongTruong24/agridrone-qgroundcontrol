#include "CompanionBandwidthTest.h"
#include "CompanionBandwidth.h"
#include "UDPLink.h"

#include <QtTest/QTest>

UT_REGISTER_TEST(CompanionBandwidthTest, TestLabel::Unit)

namespace {
mavlink_cc_bandwidth_t sample(float videoIn, uint8_t valid = CC_BANDWIDTH_VALID_ROUTER | CC_BANDWIDTH_VALID_CAMERA | CC_BANDWIDTH_VALID_NETWORK)
{
    mavlink_cc_bandwidth_t s{};
    s.video_in_kbps = videoIn;
    s.udp_tx_rate[0] = 1200.f;
    s.video_out_kbps[0] = videoIn;
    s.if_tx_kbps[0] = 2600.f;
    s.video_readers = 1;
    s.valid = valid;
    return s;
}
}

// 2026-10-20: keyframes land in different seconds on CC and QGC; 1 s numbers looked like loss (spec §Smoothing).
void CompanionBandwidthTest::_testMeanOfThreeCcSamples()
{
    CompanionBandwidth bw;
    bw.addCc(sample(1000.f), 0);
    bw.addCc(sample(2000.f), 1000);
    bw.addCc(sample(3000.f), 2000);
    bw.addCc(sample(6000.f), 3000);
    const QVariantMap s = bw.state(0, 3000, 0.4f);
    QCOMPARE(s["ccState"].toString(), QStringLiteral("ok"));
    QCOMPARE(s["video"].toMap()["inKbps"].toDouble(), (2000.0 + 3000.0 + 6000.0) / 3.0);
    QCOMPARE(s["telemetry"].toList()[0].toMap()["ccTx"].toDouble(), 1200.0);
    QCOMPARE(s["loss"].toDouble(), double(0.4f));
}

void CompanionBandwidthTest::_testClearedBitIsNoData()
{
    CompanionBandwidth bw;
    bw.addCc(sample(1000.f), 0);
    bw.addCc(sample(1000.f, CC_BANDWIDTH_VALID_ROUTER | CC_BANDWIDTH_VALID_NETWORK), 1000);
    const QVariantMap s = bw.state(0, 1000, -1.f);
    QVERIFY(!s["cameraValid"].toBool());
    QVERIFY(s["routerValid"].toBool());
    QVERIFY(s["networkValid"].toBool());
}

// 2026-10-20: an old CC (or a thaco.xml mismatch that drops 42016 on CRC_EXTRA) must not read as "all zero".
void CompanionBandwidthTest::_testStates()
{
    CompanionBandwidth bw;
    QCOMPARE(bw.state(-1, 0, -1.f)["ccState"].toString(), QStringLiteral("noCc"));
    QCOMPARE(bw.state(0, 1000, -1.f)["ccState"].toString(), QStringLiteral("waiting"));
    QCOMPARE(bw.state(0, 3000, -1.f)["ccState"].toString(), QStringLiteral("unsupported"));
    bw.addCc(sample(1000.f), 3500);
    QCOMPARE(bw.state(0, 4000, -1.f)["ccState"].toString(), QStringLiteral("ok"));
    QCOMPARE(bw.state(0, 6600, -1.f)["ccState"].toString(), QStringLiteral("unsupported"));
    bw.resetCc();
    QCOMPARE(bw.state(10000, 10500, -1.f)["ccState"].toString(), QStringLiteral("waiting"));
}

void CompanionBandwidthTest::_testLocalRatesAndPairing()
{
    CompanionBandwidth bw;
    bw.sampleLocal(0, 0, {{QStringLiteral("UDP Link"), 0, 0, 1}, {QStringLiteral("TCP"), -1, 0, 2}});
    bw.sampleLocal(1000, 125000, {{QStringLiteral("UDP Link"), 0, 12000, 1}, {QStringLiteral("TCP"), -1, 500, 2}});
    bw.addCc(sample(1000.f), 1000);
    const QVariantMap s = bw.state(0, 1000, -1.f);
    QCOMPARE(s["video"].toMap()["qgcRxKbps"].toDouble(), 1000.0);
    const QVariantMap row0 = s["telemetry"].toList()[0].toMap();
    QCOMPARE(row0["qgcLink"].toString(), QStringLiteral("UDP Link"));
    QCOMPARE(row0["qgcRx"].toDouble(), 12000.0);
    QVERIFY(!s["telemetry"].toList()[1].toMap().contains("qgcLink"));
    QCOMPARE(s["otherLinks"].toList()[0].toMap()["name"].toString(), QStringLiteral("TCP"));

    CompanionBandwidth noVideo;
    noVideo.sampleLocal(0, -1, {});
    noVideo.sampleLocal(1000, -1, {});
    QVERIFY(!noVideo.state(0, 1000, -1.f)["video"].toMap().contains("qgcRxKbps"));
}

void CompanionBandwidthTest::_testEndpointForUdpConfig()
{
    UDPConfiguration second(QStringLiteral("second"));
    second.addHost(QStringLiteral("127.0.0.1"), 14551);
    QCOMPARE(CompanionBandwidth::endpointFor(&second), 1);
    // 2026-10-20: endpoint 0 (target host 14550) was never asserted.
    UDPConfiguration first(QStringLiteral("first"));
    first.addHost(QStringLiteral("127.0.0.1"), 14550);
    QCOMPARE(CompanionBandwidth::endpointFor(&first), 0);
    UDPConfiguration listener(QStringLiteral("listener"));
    listener.setLocalPort(14550);
    QCOMPARE(CompanionBandwidth::endpointFor(&listener), 2);
    UDPConfiguration other(QStringLiteral("other"));
    other.setLocalPort(14560);
    QCOMPARE(CompanionBandwidth::endpointFor(&other), -1);
    QCOMPARE(CompanionBandwidth::endpointFor(nullptr), -1);
}

using L = CompanionBandwidth::Link;

// 2026-10-20: a link that vanished and came back under the same name showed its pre-disconnect rate as live.
void CompanionBandwidthTest::_testReconnectedLinkHasNoStaleRate()
{
    CompanionBandwidth bw;
    bw.sampleLocal(0, -1, {L{QStringLiteral("A"), 0, 0}});
    bw.sampleLocal(1000, -1, {L{QStringLiteral("A"), 0, 10000}});
    bw.sampleLocal(2000, -1, {L{QStringLiteral("A"), 0, 20000}});
    QCOMPARE(bw.state(0, 2000, -1.f)["telemetry"].toList()[0].toMap()["qgcRx"].toDouble(), 10000.0);
    bw.sampleLocal(3000, -1, {});
    bw.sampleLocal(4000, -1, {L{QStringLiteral("A"), 0, 0}});
    QVERIFY(!bw.state(0, 4000, -1.f)["telemetry"].toList()[0].toMap().contains("qgcRx"));
}

// 2026-10-20: a link with no rate sample yet showed 0 instead of "no data".
void CompanionBandwidthTest::_testLinkWithoutRateOmitsQgcRx()
{
    CompanionBandwidth bw;
    bw.sampleLocal(0, -1, {L{QStringLiteral("A"), 0, 5, 1}, L{QStringLiteral("T"), -1, 5, 2}});
    const QVariantMap s = bw.state(0, 0, -1.f);
    const QVariantMap row0 = s["telemetry"].toList()[0].toMap();
    QCOMPARE(row0["qgcLink"].toString(), QStringLiteral("A"));
    QVERIFY(!row0.contains("qgcRx"));
    const QVariantMap other = s["otherLinks"].toList()[0].toMap();
    QCOMPARE(other["name"].toString(), QStringLiteral("T"));
    QVERIFY(!other.contains("qgcRx"));
}

// 2026-10-21: QGC allows duplicate link names; both rows showed one link's rate because rates were keyed by name.
void CompanionBandwidthTest::_testSameNameLinksKeepOwnRates()
{
    CompanionBandwidth bw;
    bw.sampleLocal(0, -1, {L{QStringLiteral("UDP"), 0, 0, 1}, L{QStringLiteral("UDP"), -1, 0, 2}});
    bw.sampleLocal(1000, -1, {L{QStringLiteral("UDP"), 0, 10000, 1}, L{QStringLiteral("UDP"), -1, 3000, 2}});
    const QVariantMap s = bw.state(0, 1000, -1.f);
    QCOMPARE(s["telemetry"].toList()[0].toMap()["qgcRx"].toDouble(), 10000.0);
    QCOMPARE(s["otherLinks"].toList().size(), 1);
    QCOMPARE(s["otherLinks"].toList()[0].toMap()["qgcRx"].toDouble(), 3000.0);
}

// 2026-10-20: with no CC heartbeat session, a fresh sample still reported the owner flags valid.
void CompanionBandwidthTest::_testValidFlagsNeedCcSession()
{
    CompanionBandwidth bw;
    bw.addCc(sample(1000.f), 1000);
    const QVariantMap s = bw.state(-1, 1500, -1.f);
    QCOMPARE(s["ccState"].toString(), QStringLiteral("noCc"));
    QVERIFY(!s["routerValid"].toBool());
    QVERIFY(!s["cameraValid"].toBool());
    QVERIFY(!s["networkValid"].toBool());
}

// 2026-10-20: after a CC_BANDWIDTH dropout the first samples averaged with pre-gap data.
void CompanionBandwidthTest::_testGapClearsWindow()
{
    CompanionBandwidth bw;
    bw.addCc(sample(9000.f), 0);
    bw.addCc(sample(9000.f), 1000);
    bw.addCc(sample(1000.f), 5000);
    QCOMPARE(bw.state(0, 5000, -1.f)["video"].toMap()["inKbps"].toDouble(), 1000.0);
    bw.addCc(sample(3000.f), 6000);
    QCOMPARE(bw.state(0, 6000, -1.f)["video"].toMap()["inKbps"].toDouble(), 2000.0);
}

// 2026-10-20: one CC reached over two links delivers every 1 Hz CC_BANDWIDTH twice; the window double-counted.
void CompanionBandwidthTest::_testDuplicateDeliveryIgnored()
{
    CompanionBandwidth bw;
    bw.addCc(sample(1000.f), 0);
    bw.addCc(sample(1000.f), 20);
    bw.addCc(sample(4000.f), 1000);
    bw.addCc(sample(4000.f), 1030);
    QCOMPARE(bw.state(0, 1030, -1.f)["video"].toMap()["inKbps"].toDouble(), 2500.0);
}

// 2026-10-20: counter reset (reconnect, restart) must read 0 for that tick and rebaseline, never negative.
void CompanionBandwidthTest::_testCounterDecreaseIsZeroAndRebaselines()
{
    CompanionBandwidth bw;
    bw.sampleLocal(0, 0, {L{QStringLiteral("A"), 0, 0}});
    bw.sampleLocal(1000, 125000, {L{QStringLiteral("A"), 0, 3000}});
    bw.sampleLocal(2000, 1000, {L{QStringLiteral("A"), 0, 100}});
    bw.sampleLocal(3000, 126000, {L{QStringLiteral("A"), 0, 3100}});
    const QVariantMap s = bw.state(0, 3000, -1.f);
    QCOMPARE(s["video"].toMap()["qgcRxKbps"].toDouble(), (1000.0 + 0.0 + 1000.0) / 3.0);
    QCOMPARE(s["telemetry"].toList()[0].toMap()["qgcRx"].toDouble(), (3000.0 + 0.0 + 3000.0) / 3.0);
}

// 2026-10-20: a sample whose owner bit is cleared must not drag that owner's mean toward its zero payload.
void CompanionBandwidthTest::_testMeanSkipsClearedOwnerBit()
{
    CompanionBandwidth bw;
    bw.addCc(sample(1000.f), 0);
    bw.addCc(sample(9999.f, CC_BANDWIDTH_VALID_ROUTER | CC_BANDWIDTH_VALID_NETWORK), 1000);
    bw.addCc(sample(3000.f), 2000);
    QCOMPARE(bw.state(0, 2000, -1.f)["video"].toMap()["inKbps"].toDouble(), 2000.0);
}

// 2026-10-20: the QGC-side window of 3 was never exercised past its first fill.
void CompanionBandwidthTest::_testQgcWindowSlides()
{
    CompanionBandwidth bw;
    const quint64 bytes[] = {0, 125000, 375000, 750000, 1250000}; // 1000, 2000, 3000, 4000 kbps
    for (int i = 0; i < 5; ++i) {
        bw.sampleLocal(i * 1000, qint64(bytes[i]), {L{QStringLiteral("A"), 0, bytes[i]}});
    }
    const QVariantMap s = bw.state(0, 4000, -1.f);
    QCOMPARE(s["video"].toMap()["qgcRxKbps"].toDouble(), 3000.0);
    QCOMPARE(s["telemetry"].toList()[0].toMap()["qgcRx"].toDouble(), 3000.0 * 125.0);
}
