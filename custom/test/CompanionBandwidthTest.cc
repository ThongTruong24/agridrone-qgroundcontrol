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
    bw.sampleLocal(0, 0, {{QStringLiteral("UDP Link"), 0, 0}, {QStringLiteral("TCP"), -1, 0}});
    bw.sampleLocal(1000, 125000, {{QStringLiteral("UDP Link"), 0, 12000}, {QStringLiteral("TCP"), -1, 500}});
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
    UDPConfiguration listener(QStringLiteral("listener"));
    listener.setLocalPort(14550);
    QCOMPARE(CompanionBandwidth::endpointFor(&listener), 2);
    UDPConfiguration other(QStringLiteral("other"));
    other.setLocalPort(14560);
    QCOMPARE(CompanionBandwidth::endpointFor(&other), -1);
    QCOMPARE(CompanionBandwidth::endpointFor(nullptr), -1);
}
