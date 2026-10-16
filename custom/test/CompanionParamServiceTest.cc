#include "CompanionParamServiceTest.h"

#include <QtTest/QSignalSpy>
#include <QtCore/QTemporaryDir>
#include <QtCore/QFile>
#include <cstring>

#include "CompanionParamService.h"
#include "Fact.h"
#include "MAVLinkProtocol.h"
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
            QVERIFY(!map.value("defaultValue").isValid()); // 2026-10-14: no QGC-side default (this asserted 921600)
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
    // 2026-10-14: the port defaults this checked are gone; _testNoValueBeforeCompanionAnswers covers it.
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

// 2026-10-14: cc_parameters.json describes links 0/1 only; a CC_L2_* the CC reported became read-only and its SET was skipped.
void CompanionParamServiceTest::_testLinkNParamsCloneLinkZeroTemplate()
{
    CompanionParamService service;
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "CC_L2_BAUD", 57600, 3, 0)));
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "CC_L2_NOPE", 1, 3, 1)));  // no CC_L0_NOPE template
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "XCC_L2_BAUD", 1, 3, 2))); // not a link parameter
    QVariantMap link0, link2, noTemplate, notLink;
    for (const auto& item : service.paramList()) {
        const auto map = item.toMap();
        const auto name = map.value("name").toString();
        if (name == "CC_L0_BAUD") link0 = map;
        else if (name == "CC_L2_BAUD") link2 = map;
        else if (name == "CC_L2_NOPE") noTemplate = map;
        else if (name == "XCC_L2_BAUD") notLink = map;
    }
    QVERIFY(!link2.isEmpty());
    QVERIFY(!link2.value("readOnly").toBool());
    QCOMPARE(link2.value("group"), link0.value("group"));
    QCOMPARE(link2.value("options"), link0.value("options"));
    QCOMPARE(link2.value("label").toString(), QString("CC_L2_BAUD")); // its own name, not "Link 0 Baudrate"
    QCOMPARE(link2.value("defaultValue").toUInt(), 57600u);            // the value the CC reported, not link 0's default
    QVERIFY(noTemplate.value("readOnly").toBool());
    QVERIFY(notLink.value("readOnly").toBool());
    service.stageParameter("CC_L2_BAUD", 115200);
    QCOMPARE(service.modifiedCount(), 1);
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

// 2026-10-14: with the FC silent there is no Vehicle; the parameter read must still reach the CC (THACO_Drone-qx3).
void CompanionParamServiceTest::_testRequestGoesToTransportWithoutVehicle()
{
    CompanionParamService service;
    QList<mavlink_message_t> sent;
    service.requestParameters(); // no transport yet: nothing to send to
    QVERIFY(!service.isLoading());

    service.setTransport({7, 0, [&](const mavlink_message_t& m) { sent.append(m); return true; }});
    service.requestParameters();
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.first().msgid, uint32_t(MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST));
    QCOMPARE(sent.first().sysid, uint8_t(MAVLinkProtocol::instance()->getSystemId()));
    mavlink_param_ext_request_list_t request{};
    mavlink_msg_param_ext_request_list_decode(&sent.first(), &request);
    QCOMPARE(request.target_system, uint8_t(7));
    QCOMPARE(request.target_component, uint8_t(191));
    QVERIFY(service.isLoading());
}

// 2026-10-14: a REQUEST_LIST that could not be sent left the service "loading", which refused every operation for 6 s.
void CompanionParamServiceTest::_testFailedRequestSendDoesNotLoad()
{
    CompanionParamService service;
    service.setTransport({7, 0, [](const mavlink_message_t&) { return false; }});
    service.requestParameters();
    QVERIFY(!service.isLoading());
}

// 2026-10-14: QGC showed /dev/ttyAMA4, FC, 921600 before the CC answered; every value now comes from the CC.
void CompanionParamServiceTest::_testNoValueBeforeCompanionAnswers()
{
    CompanionParamService service;
    QVERIFY(!service.allFactNames().isEmpty());
    QVERIFY(!service.loadError());
    for (const auto& name : service.allFactNames()) {
        QVERIFY2(!service.stagedValue(name).isValid() || service.stagedValue(name).toString().isEmpty(), qPrintable(name));
        QVERIFY(!service.confirmedValue(name).isValid());
    }
    QVERIFY(service.allFactNames().contains("CC_SER_PORTS"));
    QVERIFY(service.isReadOnly("CC_SER_PORTS"));
    QVERIFY(service.options("CC_L0_BAUD").contains("57600"));

    // The CC's value is shown once it reports one, and nothing again after a reset (CC lost).
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "CC_L0_BAUD", 57600, 1, 0)));
    QCOMPARE(service.stagedValue("CC_L0_BAUD").toUInt(), 57600u);
    service.resetState();
    QVERIFY(!service.stagedValue("CC_L0_BAUD").isValid());
}

// 2026-10-14: a catalog that did not load fell back to a built-in table of links, ports and bauds.
void CompanionParamServiceTest::_testCatalogLoadErrorHasNoParameters()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile broken(dir.filePath("broken.json"));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("{ not json");
    broken.close();
    for (const QString& path : {dir.filePath("missing.json"), broken.fileName()}) {
        CompanionParamService service;
        service.loadTemplate(path);
        QVERIFY2(service.loadError(), qPrintable(path));
        QVERIFY2(service.allFactNames().isEmpty(), qPrintable(path));
        QVERIFY(service.paramList().isEmpty());
        QVERIFY(service.options("CC_L0_BAUD").isEmpty());
    }
}

// 2026-10-16: every PARAM_EXT_VALUE counted toward the LIST (repeats, another client's LIST, values pushed after an
// ACK), so the load could end with parameters still missing; on the vehicle a second LIST re-sent 17 values to all.
void CompanionParamServiceTest::_testListCompletesOnDistinctIndicesOnly()
{
    CompanionParamService service;
    service.setTransport({1, 0, [](const mavlink_message_t&) { return true; }});
    service.requestParameters();
    QVERIFY(service.isLoading());
    for (int i = 0; i < 3; ++i)
        QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "CC_L0_BAUD", 460800, 2, 0)));
    QVERIFY(service.isLoading()); // index 1 has not arrived yet
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "CC_L1_BAUD", 115200, 2, 1)));
    QVERIFY(!service.isLoading());
}

// 2026-10-16: an ACK the CC sends without a value (it had none confirmed) carries zeros; QGC took them as the live value.
void CompanionParamServiceTest::_testRefusedAckKeepsTheKnownValue()
{
    CompanionParamService service;
    QVERIFY(service.handleMavlinkMessage(packParamExtValue(1, 191, "CC_L0_BAUD", 460800, 16, 1)));
    QVERIFY(service.handleMavlinkMessage(packParamExtAck(1, 191, "CC_L0_BAUD", 0, 2 /* FAILED */)));
    QCOMPARE(service.confirmedValue("CC_L0_BAUD").toUInt(), 460800u);
    QVERIFY(service.handleMavlinkMessage(packParamExtAck(1, 191, "CC_L0_BAUD", 921600, 0 /* ACCEPTED */)));
    QCOMPARE(service.confirmedValue("CC_L0_BAUD").toUInt(), 921600u);
}
