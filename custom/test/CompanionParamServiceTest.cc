#include "CompanionParamServiceTest.h"

#include <QtTest/QSignalSpy>
#include <QtCore/QTemporaryDir>
#include <QtCore/QFile>
#include <cstring>

#include "CompanionParamService.h"
#include "Fact.h"
#include "QGCMAVLink.h"

UT_REGISTER_TEST(CompanionParamServiceTest, TestLabel::Unit)

namespace {
mavlink_message_t packParamExtValue(uint8_t sysId, uint8_t compId, const char* paramId, uint32_t val, uint16_t count, uint16_t index)
{
    mavlink_message_t msg{};
    char idBuf[16]{};
    std::memcpy(idBuf, paramId, std::min<size_t>(std::strlen(paramId), 16));

    char valBuf[128]{};
    std::memcpy(valBuf, &val, 4);

    mavlink_msg_param_ext_value_pack(sysId, compId, &msg, idBuf, valBuf, MAV_PARAM_EXT_TYPE_UINT32, count, index);
    return msg;
}

mavlink_message_t packParamExtAck(uint8_t sysId, uint8_t compId, const char* paramId, uint32_t val, uint8_t result)
{
    mavlink_message_t msg{};
    char idBuf[16]{};
    std::memcpy(idBuf, paramId, std::min<size_t>(std::strlen(paramId), 16));

    char valBuf[128]{};
    std::memcpy(valBuf, &val, 4);

    mavlink_msg_param_ext_ack_pack(sysId, compId, &msg, idBuf, valBuf, MAV_PARAM_EXT_TYPE_UINT32, result);
    return msg;
}
} // namespace

void CompanionParamServiceTest::_testInitialTemplateLoads()
{
    CompanionParamService service;
    QVariantList list = service.paramList();

    // Template defines 16 known CC parameters
    QVERIFY(list.count() >= 16);
    QCOMPARE(service.modifiedCount(), 0);

    QStringList groups = service.groups();
    QVERIFY(groups.contains("Telemetry"));
    QVERIFY(groups.contains("Camera"));
    QVERIFY(groups.contains("Vision"));
    QVERIFY(groups.contains("Network"));

    // Check one known parameter
    bool foundBaud = false;
    for (const auto& item : list) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_L0_BAUD") {
            foundBaud = true;
            QCOMPARE(map.value("defaultValue").toUInt(), 921600u);
            QVERIFY(map.value("options").toStringList().contains("921600"));
            break;
        }
    }
    QVERIFY(foundBaud);
}

void CompanionParamServiceTest::_testStageAndReset()
{
    CompanionParamService service;
    QSignalSpy modSpy(&service, &CompanionParamService::modifiedCountChanged);

    service.stageParameter("CC_L0_BAUD", 115200);
    QCOMPARE(service.modifiedCount(), 1);
    QVERIFY(modSpy.count() >= 1);

    service.stageParameter("CC_L1_BAUD", 57600);
    QCOMPARE(service.modifiedCount(), 2);

    service.resetParameter("CC_L0_BAUD");
    QCOMPARE(service.modifiedCount(), 1);

    service.resetAllModified();
    QCOMPARE(service.modifiedCount(), 0);
}

void CompanionParamServiceTest::_testParamExtValueDecode()
{
    CompanionParamService service;

    mavlink_message_t msg = packParamExtValue(1, 191, "CC_L0_BAUD", 460800, 16, 1);
    QVERIFY(service.handleMavlinkMessage(msg));

    QVariantList list = service.paramList();
    for (const auto& item : list) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_L0_BAUD") {
            QVERIFY(map.value("isLiveSynced").toBool());
            QCOMPARE(map.value("liveValue").toUInt(), 460800u);
            QCOMPARE(map.value("value").toUInt(), 460800u);
            break;
        }
    }
}

void CompanionParamServiceTest::_testParamExtAckHandling()
{
    CompanionParamService service;
    QSignalSpy saveSpy(&service, &CompanionParamService::parameterSaved);

    service.stageParameter("CC_L0_BAUD", 460800);
    QCOMPARE(service.modifiedCount(), 1);

    mavlink_message_t ack = packParamExtAck(1, 191, "CC_L0_BAUD", 460800, 0 /* ACCEPTED */);
    QVERIFY(service.handleMavlinkMessage(ack));

    QCOMPARE(service.modifiedCount(), 0);
    QVERIFY(saveSpy.count() == 1);
    QCOMPARE(saveSpy.first().at(1).toBool(), true);
}

void CompanionParamServiceTest::_testExportAndImport()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_cc_params.json");

    CompanionParamService service;
    service.stageParameter("CC_L0_BAUD", 460800);
    service.stageParameter("CC_AP_SSID", "CUSTOM_SSID");

    QVERIFY(service.exportParameters(filePath));
    QVERIFY(QFile::exists(filePath));

    // Reset service
    service.resetState();
    QCOMPARE(service.modifiedCount(), 0);

    // Import back
    QVERIFY(service.importParameters(filePath));
    QVERIFY(service.modifiedCount() >= 2);

    QVariantList list = service.paramList();
    for (const auto& item : list) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_L0_BAUD") {
            QCOMPARE(map.value("value").toUInt(), 460800u);
        } else if (map.value("name").toString() == "CC_AP_SSID") {
            QCOMPARE(map.value("value").toString(), QStringLiteral("CUSTOM_SSID"));
        }
    }
}

void CompanionParamServiceTest::_testRejectsWrongComponentId()
{
    CompanionParamService service;

    mavlink_message_t msg = packParamExtValue(1, 50, "CC_L0_BAUD", 460800, 16, 1);
    QVERIFY(!service.handleMavlinkMessage(msg));
}

void CompanionParamServiceTest::_testExact16ByteParamId()
{
    CompanionParamService service;

    // Parameter ID with exactly 16 bytes: "1234567890123456"
    const char* exact16Id = "1234567890123456";
    mavlink_message_t msg = packParamExtValue(1, 191, exact16Id, 42, 1, 0);
    QVERIFY(service.handleMavlinkMessage(msg));

    bool found = false;
    for (const auto& item : service.paramList()) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == QString::fromLatin1(exact16Id)) {
            found = true;
            QCOMPARE(map.value("liveValue").toUInt(), 42u);
            break;
        }
    }
    QVERIFY(found);
}

void CompanionParamServiceTest::_testParamAckInProgressRemainsPending()
{
    CompanionParamService service;
    QSignalSpy saveSpy(&service, &CompanionParamService::parameterSaved);

    service.stageParameter("CC_L0_BAUD", 460800);
    QCOMPARE(service.modifiedCount(), 1);

    // PARAM_ACK_IN_PROGRESS = 5
    mavlink_message_t ackProgress = packParamExtAck(1, 191, "CC_L0_BAUD", 460800, PARAM_ACK_IN_PROGRESS);
    QVERIFY(service.handleMavlinkMessage(ackProgress));

    // Must NOT be treated as failure or un-staged
    QCOMPARE(service.modifiedCount(), 1);
    QCOMPARE(saveSpy.count(), 0); // No final save signal yet

    // Parameter must show isPending == true
    bool foundPending = false;
    for (const auto& item : service.paramList()) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_L0_BAUD") {
            foundPending = true;
            QVERIFY(map.value("isPending").toBool());
            break;
        }
    }
    QVERIFY(foundPending);

    // Final ACK: PARAM_ACK_ACCEPTED = 0
    mavlink_message_t ackDone = packParamExtAck(1, 191, "CC_L0_BAUD", 460800, 0 /* ACCEPTED */);
    QVERIFY(service.handleMavlinkMessage(ackDone));

    QCOMPARE(service.modifiedCount(), 0);
    QCOMPARE(saveSpy.count(), 1);
    QCOMPARE(saveSpy.first().at(1).toBool(), true);
}

void CompanionParamServiceTest::_testUnavailableValuesNotReplacedByDefault()
{
    CompanionParamService service;
    QVariantList list = service.paramList();

    // Before receiving PARAM_EXT_VALUE, params are not live synced and not available
    for (const auto& item : list) {
        QVariantMap map = item.toMap();
        QVERIFY(!map.value("isLiveSynced").toBool());
        QCOMPARE(map.value("liveValue").toString(), QStringLiteral("--"));
        QVERIFY(!map.value("isAvailable").toBool());
    }
}

void CompanionParamServiceTest::_testNamedLinksReplaceLegacyLinks()
{
    CompanionParamService service;
    QVariantList list = service.paramList();

    QStringList names;
    for (const auto& item : list) {
        names.append(item.toMap().value("name").toString());
    }

    // Must contain named links
    QVERIFY(names.contains("CC_L0_NAME"));
    QVERIFY(names.contains("CC_L0_PORT"));
    QVERIFY(names.contains("CC_L0_BAUD"));
    QVERIFY(names.contains("CC_L1_NAME"));
    QVERIFY(names.contains("CC_L1_PORT"));
    QVERIFY(names.contains("CC_L1_BAUD"));

    // Must NOT contain legacy hardcoded links
    QVERIFY(!names.contains("CC_FC_PORT"));
    QVERIFY(!names.contains("CC_FC_BAUD"));
    QVERIFY(!names.contains("CC_SIYI_PORT"));
    QVERIFY(!names.contains("CC_SIYI_BAUD"));

    // Check defaults: L0 port must be /dev/ttyAMA4, NOT /dev/ttyTHS1
    for (const auto& item : list) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_L0_PORT") {
            QCOMPARE(map.value("defaultValue").toString(), QStringLiteral("/dev/ttyAMA4"));
        } else if (map.value("name").toString() == "CC_L1_PORT") {
            QCOMPARE(map.value("defaultValue").toString(), QStringLiteral("/dev/ttyAMA0"));
        }
    }
}

void CompanionParamServiceTest::_testReadOnlyAndBootOnlyCannotBeStaged()
{
    CompanionParamService service;

    // Check read-only / boot-only flags in catalog
    bool foundCamW = false;
    for (const auto& item : service.paramList()) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_CAM_W") {
            foundCamW = true;
            QVERIFY(map.value("readOnly").toBool());
            QCOMPARE(map.value("applyMode").toString(), QStringLiteral("boot_only"));
            break;
        }
    }
    QVERIFY(foundCamW);

    // Staging read-only parameter must be rejected
    service.stageParameter("CC_CAM_W", 1920);
    QCOMPARE(service.modifiedCount(), 0);

    service.stageParameter("CC_VIS_MODEL", "custom.pt");
    QCOMPARE(service.modifiedCount(), 0);
}

void CompanionParamServiceTest::_testUnknownParamsNotWritable()
{
    CompanionParamService service;

    mavlink_message_t msg = packParamExtValue(1, 191, "CC_UNKNOWN_VAR", 99, 1, 0);
    QVERIFY(service.handleMavlinkMessage(msg));

    // Unknown parameter must not be writable
    bool found = false;
    for (const auto& item : service.paramList()) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_UNKNOWN_VAR") {
            found = true;
            QVERIFY(map.value("readOnly").toBool());
            break;
        }
    }
    QVERIFY(found);

    service.stageParameter("CC_UNKNOWN_VAR", 100);
    QCOMPARE(service.modifiedCount(), 0);
}

void CompanionParamServiceTest::_testConfirmedFacts()
{
    CompanionParamService service;
    QVERIFY(!service.confirmedValue("CC_CAM_FPS").isValid());
    QVERIFY(service.parameterFact("CC_CAM_FPS"));
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(42, 191, "CC_CAM_FPS", 30, 1, 0)));
    QCOMPARE(service.confirmedValue("CC_CAM_FPS").toInt(), 30);
    QCOMPARE(service.parameterFact("CC_CAM_FPS")->rawValue().toInt(), 30);
    service.stageParameter("CC_CAM_FPS", 15);
    QCOMPARE(service.confirmedValue("CC_CAM_FPS").toInt(), 30);
    service.resetState();
    QVERIFY(!service.confirmedValue("CC_CAM_FPS").isValid());
}

void CompanionParamServiceTest::_testFactsAreDeveloperCategoryAndEditsAreStaged()
{
    CompanionParamService service;
    Fact* fact = service.parameterFact("CC_CAM_FPS");
    QVERIFY(fact);
    QCOMPARE(fact->category(), QStringLiteral("Developer"));
    QCOMPARE(fact->group(), QStringLiteral("Camera"));

    QVERIFY(service.handleMavlinkMessage(packParamExtValue(42, 191, "CC_CAM_FPS", 30, 1, 0)));
    QCOMPARE(service.modifiedCount(), 0);

    fact->setRawValue(15); // what the Parameters editor dialog does
    QCOMPARE(service.modifiedCount(), 1);
    QCOMPARE(service.confirmedValue("CC_CAM_FPS").toInt(), 30); // nothing is confirmed until the CC ACKs
    QCOMPARE(fact->rawValue().toInt(), 15);                     // the staged edit stays visible in the editor

    service.resetAllModified();
    QCOMPARE(service.modifiedCount(), 0);
    QCOMPARE(fact->rawValue().toInt(), 30);                     // Discard shows what the CC has
}

void CompanionParamServiceTest::_testEditorFactOrderAndStagedSubsystems()
{
    CompanionParamService service;
    const QList<Fact*> facts = service.allFacts();
    QCOMPARE(facts.size(), service.paramList().size());
    for (Fact* fact : facts) {
        QCOMPARE(fact->category(), QStringLiteral("Developer"));
        QCOMPARE(fact->componentId(), 191);
    }

    // Staged parameters need an APPLY for their ConfigManager subsystem; live ones take effect on SET.
    QCOMPARE(service.stagedSubsystem("CC_L0_BAUD"), 1);
    QCOMPARE(service.stagedSubsystem("CC_AP_SSID"), 3);
    QCOMPARE(service.stagedSubsystem("CC_CAM_FPS"), 0);
    QCOMPARE(service.stagedSubsystem("CC_NOT_A_PARAM"), 0);
}
