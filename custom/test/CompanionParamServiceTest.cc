#include "CompanionParamServiceTest.h"

#include <QtTest/QSignalSpy>
#include <QtCore/QTemporaryDir>
#include <QtCore/QFile>
#include <cstring>

#include "CompanionParamService.h"
#include "QGCMAVLink.h"

UT_REGISTER_TEST(CompanionParamServiceTest, TestLabel::Unit)

namespace {
mavlink_message_t packParamExtValue(uint8_t sysId, uint8_t compId, const char* paramId, uint32_t val, uint16_t count, uint16_t index)
{
    mavlink_message_t msg{};
    char idBuf[16]{};
    std::strncpy(idBuf, paramId, sizeof(idBuf) - 1);

    char valBuf[128]{};
    std::memcpy(valBuf, &val, 4);

    mavlink_msg_param_ext_value_pack(sysId, compId, &msg, idBuf, valBuf, MAV_PARAM_EXT_TYPE_UINT32, count, index);
    return msg;
}

mavlink_message_t packParamExtAck(uint8_t sysId, uint8_t compId, const char* paramId, uint32_t val, uint8_t result)
{
    mavlink_message_t msg{};
    char idBuf[16]{};
    std::strncpy(idBuf, paramId, sizeof(idBuf) - 1);

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
        if (map.value("name").toString() == "CC_FC_BAUD") {
            foundBaud = true;
            QCOMPARE(map.value("defaultValue").toUInt(), 921600u);
            QVERIFY(map.value("options").toStringList().contains("921600"));
            QVERIFY(map.value("reboot").toBool());
            break;
        }
    }
    QVERIFY(foundBaud);
}

void CompanionParamServiceTest::_testStageAndReset()
{
    CompanionParamService service;
    QSignalSpy modSpy(&service, &CompanionParamService::modifiedCountChanged);

    service.stageParameter("CC_FC_BAUD", 115200);
    QCOMPARE(service.modifiedCount(), 1);
    QVERIFY(modSpy.count() >= 1);

    service.stageParameter("CC_SIYI_BAUD", 57600);
    QCOMPARE(service.modifiedCount(), 2);

    service.resetParameter("CC_FC_BAUD");
    QCOMPARE(service.modifiedCount(), 1);

    service.resetAllModified();
    QCOMPARE(service.modifiedCount(), 0);
}

void CompanionParamServiceTest::_testParamExtValueDecode()
{
    CompanionParamService service;

    mavlink_message_t msg = packParamExtValue(1, 191, "CC_FC_BAUD", 460800, 16, 1);
    QVERIFY(service.handleMavlinkMessage(msg));

    QVariantList list = service.paramList();
    for (const auto& item : list) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString() == "CC_FC_BAUD") {
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

    service.stageParameter("CC_FC_BAUD", 460800);
    QCOMPARE(service.modifiedCount(), 1);

    mavlink_message_t ack = packParamExtAck(1, 191, "CC_FC_BAUD", 460800, 0 /* ACCEPTED */);
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
    service.stageParameter("CC_FC_BAUD", 460800);
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
        if (map.value("name").toString() == "CC_FC_BAUD") {
            QCOMPARE(map.value("value").toUInt(), 460800u);
        } else if (map.value("name").toString() == "CC_AP_SSID") {
            QCOMPARE(map.value("value").toString(), QStringLiteral("CUSTOM_SSID"));
        }
    }
}

void CompanionParamServiceTest::_testRejectsWrongComponentId()
{
    CompanionParamService service;

    mavlink_message_t msg = packParamExtValue(1, 50, "CC_FC_BAUD", 460800, 16, 1);
    QVERIFY(!service.handleMavlinkMessage(msg));
}
