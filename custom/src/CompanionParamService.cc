#include "CompanionParamService.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "LinkManager.h"
#include "VehicleLinkManager.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <cstring>

static constexpr uint8_t kCompanionComponentId = 191; // MAV_COMP_ID_ONBOARD_COMPUTER

CompanionParamService::CompanionParamService(QObject* parent)
    : QObject(parent)
{
    _loadingTimeoutTimer = new QTimer(this);
    _loadingTimeoutTimer->setSingleShot(true);
    _loadingTimeoutTimer->setInterval(6000); // 6s timeout for param stream
    connect(_loadingTimeoutTimer, &QTimer::timeout, this, &CompanionParamService::_onLoadingTimeout);

    _loadTemplate();
}

void CompanionParamService::_loadTemplate()
{
    _groups.clear();
    _order.clear();
    _meta.clear();
    _liveValues.clear();
    _isLiveSynced.clear();
    _stagedValues.clear();
    _isModified.clear();

    QFile file(":/custom/params/cc_parameters.json");
    QByteArray data;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        data = file.readAll();
        file.close();
    } else {
        // Fallback to local filesystem path if running in tests / pre-qrc
        QFile localFile("/home/lnh/THACO_Drone/THACOGroundControl/custom/res/cc_parameters.json");
        if (localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            data = localFile.readAll();
            localFile.close();
        }
    }

    if (!data.isEmpty()) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject rootObj = doc.object();

            // Groups
            QJsonArray grpArr = rootObj.value("groups").toArray();
            for (const auto& gVal : grpArr) {
                QString gId = gVal.toObject().value("id").toString();
                if (!gId.isEmpty() && !_groups.contains(gId)) {
                    _groups.append(gId);
                }
            }

            // Parameters
            QJsonArray paramArr = rootObj.value("parameters").toArray();
            for (const auto& pVal : paramArr) {
                QJsonObject pObj = pVal.toObject();
                CCParamMeta meta;
                meta.name = pObj.value("name").toString();
                meta.label = pObj.value("label").toString(meta.name);
                meta.group = pObj.value("group").toString("General");
                meta.description = pObj.value("description").toString();
                meta.units = pObj.value("units").toString();
                meta.requiresReboot = pObj.value("reboot").toBool(false);
                meta.typeStr = pObj.value("type").toString("string");

                if (meta.typeStr == "uint32") {
                    meta.type = MAV_PARAM_EXT_TYPE_UINT32;
                    meta.defaultValue = static_cast<quint32>(pObj.value("default").toInt());
                } else if (meta.typeStr == "int32") {
                    meta.type = MAV_PARAM_EXT_TYPE_INT32;
                    meta.defaultValue = pObj.value("default").toInt();
                } else if (meta.typeStr == "real32") {
                    meta.type = MAV_PARAM_EXT_TYPE_REAL32;
                    meta.defaultValue = pObj.value("default").toDouble();
                } else {
                    meta.type = MAV_PARAM_EXT_TYPE_CUSTOM;
                    meta.defaultValue = pObj.value("default").toString();
                }

                if (pObj.contains("min")) {
                    meta.minVal = pObj.value("min").toDouble();
                    meta.hasRange = true;
                }
                if (pObj.contains("max")) {
                    meta.maxVal = pObj.value("max").toDouble();
                    meta.hasRange = true;
                }

                if (pObj.contains("options")) {
                    QJsonArray optArr = pObj.value("options").toArray();
                    for (const auto& o : optArr) {
                        meta.options.append(o.isString() ? o.toString() : QString::number(o.toDouble()));
                    }
                }

                _order.append(meta.name);
                _meta[meta.name] = meta;
                _stagedValues[meta.name] = meta.defaultValue;
                _isModified[meta.name] = false;
                _isLiveSynced[meta.name] = false;

                if (!_groups.contains(meta.group)) {
                    _groups.append(meta.group);
                }
            }
        }
    }

    // Safety fallback if file loading failed
    if (_meta.isEmpty()) {
        CCParamMeta fcp;
        fcp.name = "CC_FC_PORT"; fcp.label = "FC Serial Port"; fcp.group = "Telemetry"; fcp.type = MAV_PARAM_EXT_TYPE_CUSTOM;
        fcp.typeStr = "string"; fcp.defaultValue = "/dev/ttyTHS1"; fcp.description = "UART connected to FC";
        _order.append(fcp.name); _meta[fcp.name] = fcp; _stagedValues[fcp.name] = fcp.defaultValue;

        CCParamMeta fcb;
        fcb.name = "CC_FC_BAUD"; fcb.label = "FC Baudrate"; fcb.group = "Telemetry"; fcb.type = MAV_PARAM_EXT_TYPE_UINT32;
        fcb.typeStr = "uint32"; fcb.defaultValue = 921600; fcb.units = "bps";
        fcb.options = {"57600", "115200", "230400", "460800", "921600", "1500000"};
        fcb.description = "Baudrate for FC link";
        _order.append(fcb.name); _meta[fcb.name] = fcb; _stagedValues[fcb.name] = fcb.defaultValue;

        _groups = {"Telemetry", "Camera", "Vision", "Network"};
    }

    _updateModifiedCount();
    emit groupsChanged();
    emit paramListChanged();
}

void CompanionParamService::resetState()
{
    _isLoading = false;
    _loadingTimeoutTimer->stop();
    _expectedCount = 0;
    _receivedCount = 0;

    for (const QString& name : _order) {
        _isLiveSynced[name] = false;
        _isModified[name] = false;
        _stagedValues[name] = _meta[name].defaultValue;
    }

    _updateModifiedCount();
    emit isLoadingChanged();
    emit paramListChanged();
}

QVariantList CompanionParamService::paramList() const
{
    QVariantList list;
    for (const QString& name : _order) {
        if (!_meta.contains(name)) continue;
        const auto& m = _meta[name];
        QVariantMap map;
        map["name"] = m.name;
        map["label"] = m.label;
        map["group"] = m.group;
        map["description"] = m.description;
        map["type"] = m.typeStr;
        map["rawType"] = m.type;
        map["defaultValue"] = m.defaultValue;
        map["options"] = m.options;
        map["units"] = m.units;
        map["min"] = m.minVal;
        map["max"] = m.maxVal;
        map["hasRange"] = m.hasRange;
        map["reboot"] = m.requiresReboot;

        bool liveSynced = _isLiveSynced.value(name, false);
        map["isLiveSynced"] = liveSynced;
        map["liveValue"] = liveSynced ? _liveValues.value(name) : QStringLiteral("--");

        // Current value shown in editor
        map["value"] = _stagedValues.value(name, m.defaultValue);
        map["isModified"] = _isModified.value(name, false);

        list.append(map);
    }
    return list;
}

void CompanionParamService::_updateModifiedCount()
{
    int count = 0;
    for (auto it = _isModified.begin(); it != _isModified.end(); ++it) {
        if (it.value()) count++;
    }
    if (_modifiedCount != count) {
        _modifiedCount = count;
        emit modifiedCountChanged();
    }
}

void CompanionParamService::requestParameters(Vehicle* vehicle)
{
    if (!vehicle) {
        emit logMessage("PARAM", "ERR", "Cannot request CC params: Vehicle not connected", 3);
        return;
    }

    _isLoading = true;
    _receivedCount = 0;
    _expectedCount = 0;
    _loadingTimeoutTimer->start();
    emit isLoadingChanged();

    mavlink_message_t msg;
    mavlink_param_ext_request_list_t req;
    req.target_system = static_cast<uint8_t>(vehicle->id());
    req.target_component = kCompanionComponentId;

    SharedLinkInterfacePtr sharedLink;
    if (vehicle && vehicle->vehicleLinkManager()) {
        sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    }
    if (!sharedLink) {
        const auto links = LinkManager::instance()->links();
        for (const auto& l : links) {
            if (l && l->isConnected()) {
                sharedLink = l;
                break;
            }
        }
    }
    if (!sharedLink) return;

    mavlink_msg_param_ext_request_list_pack_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::instance()->getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        req.target_system,
        req.target_component);

    vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    emit logMessage("PARAM", "TX", QString("PARAM_EXT_REQUEST_LIST sent to Comp %1").arg(kCompanionComponentId), 6);
}

void CompanionParamService::stageParameter(const QString& name, const QVariant& value)
{
    if (!_meta.contains(name)) return;

    _stagedValues[name] = value;

    // Check if new value differs from live authoritative value (or default if not synced)
    QVariant refValue = _isLiveSynced.value(name, false) ? _liveValues.value(name) : _meta[name].defaultValue;
    bool modified = (value.toString() != refValue.toString());
    _isModified[name] = modified;

    _updateModifiedCount();
    emit paramListChanged();
}

void CompanionParamService::resetParameter(const QString& name)
{
    if (!_meta.contains(name)) return;

    QVariant refValue = _isLiveSynced.value(name, false) ? _liveValues.value(name) : _meta[name].defaultValue;
    _stagedValues[name] = refValue;
    _isModified[name] = false;

    _updateModifiedCount();
    emit paramListChanged();
}

void CompanionParamService::resetToDefault(const QString& name)
{
    if (!_meta.contains(name)) return;
    stageParameter(name, _meta[name].defaultValue);
}

void CompanionParamService::resetAllModified()
{
    for (const QString& name : _order) {
        if (_isModified.value(name, false)) {
            QVariant refValue = _isLiveSynced.value(name, false) ? _liveValues.value(name) : _meta[name].defaultValue;
            _stagedValues[name] = refValue;
            _isModified[name] = false;
        }
    }
    _updateModifiedCount();
    emit paramListChanged();
}

void CompanionParamService::saveModifiedParameters(Vehicle* vehicle)
{
    if (!vehicle) {
        emit logMessage("PARAM", "ERR", "Cannot save CC params: Vehicle not connected", 3);
        return;
    }

    int sent = 0;
    for (const QString& name : _order) {
        if (_isModified.value(name, false)) {
            sendSingleParamSet(vehicle, name, _stagedValues.value(name));
            sent++;
        }
    }

    if (sent > 0) {
        emit logMessage("PARAM", "TX", QString("Applying %1 modified CC parameter(s)...").arg(sent), 6);
    }
}

void CompanionParamService::sendSingleParamSet(Vehicle* vehicle, const QString& name, const QVariant& value)
{
    if (!vehicle || !_meta.contains(name)) return;

    const auto& meta = _meta[name];
    char paramIdBuf[16];
    std::memset(paramIdBuf, 0, sizeof(paramIdBuf));
    QByteArray idBytes = name.toUtf8();
    std::strncpy(paramIdBuf, idBytes.constData(), sizeof(paramIdBuf) - 1);

    char valBuf[128];
    _encodeValue(value, meta.type, valBuf);

    mavlink_message_t msg;
    SharedLinkInterfacePtr sharedLink;
    if (vehicle && vehicle->vehicleLinkManager()) {
        sharedLink = vehicle->vehicleLinkManager()->primaryLink().lock();
    }
    if (!sharedLink) {
        const auto links = LinkManager::instance()->links();
        for (const auto& l : links) {
            if (l && l->isConnected()) {
                sharedLink = l;
                break;
            }
        }
    }
    if (!sharedLink) return;

    mavlink_msg_param_ext_set_pack_chan(
        MAVLinkProtocol::instance()->getSystemId(),
        MAVLinkProtocol::instance()->getComponentId(),
        sharedLink->mavlinkChannel(),
        &msg,
        static_cast<uint8_t>(vehicle->id()),
        kCompanionComponentId,
        paramIdBuf,
        valBuf,
        meta.type);

    vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    emit logMessage("PARAM", "TX", QString("PARAM_EXT_SET: %1 = %2").arg(name, value.toString()), 6);
}

bool CompanionParamService::exportParameters(const QString& filePath)
{
    QString path = filePath;
    if (path.startsWith("file://")) {
        path = QUrl(filePath).toLocalFile();
    }
    if (path.isEmpty()) return false;

    QJsonObject rootObj;
    rootObj["fileType"] = "THACO_CC_PARAMETERS";
    rootObj["version"] = 1;
    rootObj["componentId"] = kCompanionComponentId;
    rootObj["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject paramsObj;
    for (const QString& name : _order) {
        QVariant v = _stagedValues.value(name, _meta[name].defaultValue);
        if (v.typeId() == QMetaType::Double) {
            paramsObj[name] = v.toDouble();
        } else if (v.typeId() == QMetaType::Int || v.typeId() == QMetaType::UInt || v.typeId() == QMetaType::LongLong) {
            paramsObj[name] = v.toLongLong();
        } else {
            paramsObj[name] = v.toString();
        }
    }
    rootObj["parameters"] = paramsObj;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit logMessage("PARAM", "ERR", QString("Export failed: cannot write to %1").arg(path), 3);
        return false;
    }

    QJsonDocument doc(rootObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit logMessage("PARAM", "SYS", QString("Exported %1 parameters to %2").arg(_order.size()).arg(QFileInfo(path).fileName()), 6);
    return true;
}

bool CompanionParamService::importParameters(const QString& filePath)
{
    QString path = filePath;
    if (path.startsWith("file://")) {
        path = QUrl(filePath).toLocalFile();
    }
    if (path.isEmpty()) return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logMessage("PARAM", "ERR", QString("Import failed: cannot read %1").arg(path), 3);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    int importedCount = 0;

    // Try parsing as JSON first
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject rootObj = doc.object();
        QJsonObject paramsObj = rootObj.contains("parameters") ? rootObj.value("parameters").toObject() : rootObj;

        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            QString name = it.key();
            if (_meta.contains(name)) {
                QVariant val = it.value().toVariant();
                stageParameter(name, val);
                importedCount++;
            }
        }
    } else {
        // Fallback: parse as KEY=VALUE text lines (.params style)
        QTextStream stream(data);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith("#") || line.startsWith("//")) continue;
            int eqIdx = line.indexOf('=');
            if (eqIdx > 0) {
                QString name = line.left(eqIdx).trimmed();
                QString val = line.mid(eqIdx + 1).trimmed();
                if (_meta.contains(name)) {
                    stageParameter(name, val);
                    importedCount++;
                }
            }
        }
    }

    if (importedCount > 0) {
        emit logMessage("PARAM", "SYS", QString("Imported %1 parameter(s) from %2").arg(importedCount).arg(QFileInfo(path).fileName()), 6);
        return true;
    }

    emit logMessage("PARAM", "ERR", QString("No valid CC parameters found in %1").arg(QFileInfo(path).fileName()), 4);
    return false;
}

bool CompanionParamService::handleMavlinkMessage(const mavlink_message_t& message)
{
    if (message.compid != kCompanionComponentId) {
        return false; // Not for/from CC component 191
    }

    if (message.msgid == MAVLINK_MSG_ID_PARAM_EXT_VALUE) {
        mavlink_param_ext_value_t pval;
        mavlink_msg_param_ext_value_decode(&message, &pval);

        char idBuf[17];
        std::memcpy(idBuf, pval.param_id, 16);
        idBuf[16] = '\0';
        QString paramId = QString::fromUtf8(idBuf).trimmed();

        QVariant val = _decodeValue(pval.param_value, pval.param_type);
        _liveValues[paramId] = val;
        _isLiveSynced[paramId] = true;

        // If not user-staged, also update current displayed value
        if (!_isModified.value(paramId, false)) {
            _stagedValues[paramId] = val;
        }

        // Dynamically add param if missing from template
        if (!_meta.contains(paramId)) {
            CCParamMeta dynMeta;
            dynMeta.name = paramId;
            dynMeta.label = paramId;
            dynMeta.group = "Other";
            dynMeta.type = pval.param_type;
            dynMeta.defaultValue = val;
            _order.append(paramId);
            _meta[paramId] = dynMeta;
            if (!_groups.contains("Other")) {
                _groups.append("Other");
                emit groupsChanged();
            }
        }

        _receivedCount++;
        _expectedCount = pval.param_count;

        // Reset timer on each incoming param
        if (_isLoading) {
            _loadingTimeoutTimer->start();
        }

        if (_expectedCount > 0 && _receivedCount >= _expectedCount) {
            _isLoading = false;
            _loadingTimeoutTimer->stop();
            emit isLoadingChanged();
            emit logMessage("PARAM", "RX", QString("Loaded all %1 CC parameters").arg(_expectedCount), 6);
        }

        emit paramListChanged();
        return true;
    }

    if (message.msgid == MAVLINK_MSG_ID_PARAM_EXT_ACK) {
        mavlink_param_ext_ack_t pack;
        mavlink_msg_param_ext_ack_decode(&message, &pack);

        char idBuf[17];
        std::memcpy(idBuf, pack.param_id, 16);
        idBuf[16] = '\0';
        QString paramId = QString::fromUtf8(idBuf).trimmed();

        QVariant ackVal = _decodeValue(pack.param_value, pack.param_type);
        _liveValues[paramId] = ackVal;
        _isLiveSynced[paramId] = true;

        bool success = (pack.param_result == 0 /* PARAM_ACK_ACCEPTED */);
        if (success) {
            _stagedValues[paramId] = ackVal;
            _isModified[paramId] = false;
            _updateModifiedCount();
            emit parameterSaved(paramId, true, QString("Parameter %1 applied successfully").arg(paramId));
            emit logMessage("PARAM", "RX", QString("PARAM_EXT_ACK: %1 = %2 (Accepted)").arg(paramId, ackVal.toString()), 6);
        } else {
            emit parameterSaved(paramId, false, QString("Parameter %1 rejected by CC (code %2)").arg(paramId).arg(pack.param_result));
            emit logMessage("PARAM", "ERR", QString("PARAM_EXT_ACK: %1 rejected (code %2)").arg(paramId).arg(pack.param_result), 3);
        }

        emit paramListChanged();
        return true;
    }

    return false;
}

void CompanionParamService::_onLoadingTimeout()
{
    if (_isLoading) {
        _isLoading = false;
        emit isLoadingChanged();
        emit logMessage("PARAM", "WARN", QString("Param request timed out (%1 received)").arg(_receivedCount), 4);
    }
}

QVariant CompanionParamService::_decodeValue(const char* raw, uint8_t type) const
{
    switch (type) {
    case MAV_PARAM_EXT_TYPE_UINT8:   return static_cast<uint8_t>(*reinterpret_cast<const uint8_t*>(raw));
    case MAV_PARAM_EXT_TYPE_INT8:    return static_cast<int8_t>(*reinterpret_cast<const int8_t*>(raw));
    case MAV_PARAM_EXT_TYPE_UINT16: { uint16_t v; std::memcpy(&v, raw, 2); return v; }
    case MAV_PARAM_EXT_TYPE_INT16:  { int16_t v; std::memcpy(&v, raw, 2); return v; }
    case MAV_PARAM_EXT_TYPE_UINT32: { uint32_t v; std::memcpy(&v, raw, 4); return v; }
    case MAV_PARAM_EXT_TYPE_INT32:  { int32_t v; std::memcpy(&v, raw, 4); return v; }
    case MAV_PARAM_EXT_TYPE_REAL32: { float v; std::memcpy(&v, raw, 4); return v; }
    case MAV_PARAM_EXT_TYPE_CUSTOM:
    default: {
        char buf[129];
        std::memcpy(buf, raw, 128);
        buf[128] = '\0';
        return QString::fromUtf8(buf).trimmed();
    }
    }
}

void CompanionParamService::_encodeValue(const QVariant& val, uint8_t type, char* outRaw) const
{
    std::memset(outRaw, 0, 128);
    switch (type) {
    case MAV_PARAM_EXT_TYPE_UINT8: {
        uint8_t v = static_cast<uint8_t>(val.toUInt());
        std::memcpy(outRaw, &v, 1);
        break;
    }
    case MAV_PARAM_EXT_TYPE_INT8: {
        int8_t v = static_cast<int8_t>(val.toInt());
        std::memcpy(outRaw, &v, 1);
        break;
    }
    case MAV_PARAM_EXT_TYPE_UINT16: {
        uint16_t v = static_cast<uint16_t>(val.toUInt());
        std::memcpy(outRaw, &v, 2);
        break;
    }
    case MAV_PARAM_EXT_TYPE_INT16: {
        int16_t v = static_cast<int16_t>(val.toInt());
        std::memcpy(outRaw, &v, 2);
        break;
    }
    case MAV_PARAM_EXT_TYPE_UINT32: {
        uint32_t v = static_cast<uint32_t>(val.toUInt());
        std::memcpy(outRaw, &v, 4);
        break;
    }
    case MAV_PARAM_EXT_TYPE_INT32: {
        int32_t v = static_cast<int32_t>(val.toInt());
        std::memcpy(outRaw, &v, 4);
        break;
    }
    case MAV_PARAM_EXT_TYPE_REAL32: {
        float v = val.toFloat();
        std::memcpy(outRaw, &v, 4);
        break;
    }
    case MAV_PARAM_EXT_TYPE_CUSTOM:
    default: {
        QByteArray bytes = val.toString().toUtf8();
        int len = std::min(static_cast<int>(bytes.size()), 127);
        std::memcpy(outRaw, bytes.constData(), len);
        break;
    }
    }
}
