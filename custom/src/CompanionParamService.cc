#include "CompanionParamService.h"

#include "Fact.h"
#include "MAVLinkProtocol.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>
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

    loadTemplate();
    _syncFacts();
    connect(this, &CompanionParamService::paramListChanged, this, &CompanionParamService::_syncFacts);
}

void CompanionParamService::_syncFacts()
{
    for (auto it = _meta.cbegin(); it != _meta.cend(); ++it) {
        const auto &m = it.value();
        if (!_facts.contains(it.key())) {
            auto type = FactMetaData::valueTypeString;
            if (m.type == MAV_PARAM_EXT_TYPE_UINT32) type = FactMetaData::valueTypeUint32;
            else if (m.type == MAV_PARAM_EXT_TYPE_UINT8) type = FactMetaData::valueTypeUint8;
            else if (m.type == MAV_PARAM_EXT_TYPE_INT32) type = FactMetaData::valueTypeInt32;
            else if (m.type == MAV_PARAM_EXT_TYPE_REAL32) type = FactMetaData::valueTypeFloat;
            auto fact = new Fact(kCompanionComponentId, it.key(), type, this);
            auto metadata = new FactMetaData(type, it.key(), fact);
            metadata->setShortDescription(m.label);
            metadata->setLongDescription(m.description);
            metadata->setCategory(QStringLiteral("Developer"));
            metadata->setGroup(m.group);
            metadata->setReadOnly(m.readOnly);
            metadata->setRawUnits(m.units);
            if (m.hasRange) { metadata->setRawMin(m.minVal); metadata->setRawMax(m.maxVal); }
            QVariantList values;
            for (const auto &option : m.options) values.append(type == FactMetaData::valueTypeString ? QVariant(option) : QVariant(option.toDouble()));
            metadata->setEnumInfo(m.options, values);
            fact->setMetaData(metadata);
            _facts[it.key()] = fact;
            // The Parameters editor writes through the Fact: that is a staged edit, applied by the editor's Apply bar.
            connect(fact, &Fact::containerRawValueChanged, this, [this, name = it.key()](const QVariant& value) {
                stageParameter(name, value);
            });
        }
        // Show the staged edit while there is one, otherwise what the CC last confirmed. Secrets are never shown.
        const bool modified = _isModified.value(it.key());
        if (!m.isSecret && (modified || _isLiveSynced.value(it.key()))) {
            _facts[it.key()]->containerSetRawValue(modified ? _stagedValues.value(it.key()) : _liveValues.value(it.key()));
        }
    }
}

void CompanionParamService::loadTemplate(const QString& path)
{
    _groups.clear();
    _order.clear();
    _meta.clear();
    _liveValues.clear();
    _isLiveSynced.clear();
    _stagedValues.clear();
    _isModified.clear();

    QFile file(path);
    QJsonParseError err{};
    const QJsonDocument doc = file.open(QIODevice::ReadOnly | QIODevice::Text) ? QJsonDocument::fromJson(file.readAll(), &err)
                                                                              : QJsonDocument();
    // No built-in fallback: a missing or broken catalog leaves no parameters, and the popups say so.
    _loadError = err.error != QJsonParseError::NoError || !doc.isObject();
    if (_loadError) {
        emit logMessage("PARAM", "ERR", QString("Cannot load the CC parameter catalog %1: %2").arg(path,
                        file.error() != QFileDevice::NoError ? file.errorString() : err.errorString()), 3);
    } else {
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
            meta.readOnly = pObj.value("readOnly").toBool(false);
            meta.isSecret = pObj.value("secret").toBool(false);
            meta.applyMode = pObj.value("applyMode").toString("staged");
            meta.typeStr = pObj.value("type").toString("string");

            if (meta.typeStr == "uint32") {
                meta.type = MAV_PARAM_EXT_TYPE_UINT32;
            } else if (meta.typeStr == "uint8") {
                meta.type = MAV_PARAM_EXT_TYPE_UINT8;
            } else if (meta.typeStr == "int32") {
                meta.type = MAV_PARAM_EXT_TYPE_INT32;
            } else if (meta.typeStr == "real32") {
                meta.type = MAV_PARAM_EXT_TYPE_REAL32;
            } else {
                meta.type = MAV_PARAM_EXT_TYPE_CUSTOM;
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
            _stagedValues[meta.name] = QVariant(); // empty until the CC reports it
            _isModified[meta.name] = false;
            _isLiveSynced[meta.name] = false;
            _isPending[meta.name] = false;

            if (!_groups.contains(meta.group)) {
                _groups.append(meta.group);
            }
        }
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
    _receivedIndices.clear();

    for (const QString& name : _order) {
        _isLiveSynced[name] = false;
        _isModified[name] = false;
        _isPending[name] = false;
        _stagedValues[name] = QVariant(); // nothing shown until the CC reports it again
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
        map["isAvailable"] = liveSynced;
        map["liveValue"] = liveSynced ? _liveValues.value(name) : QStringLiteral("--");

        // Current value shown in editor
        map["value"] = _stagedValues.value(name);
        map["isModified"] = _isModified.value(name, false);
        map["isPending"] = _isPending.value(name, false);
        map["readOnly"] = m.readOnly;
        map["secret"] = m.isSecret;
        map["applyMode"] = m.applyMode;

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

void CompanionParamService::requestParameters()
{
    if (!_transport.valid()) {
        emit logMessage("PARAM", "ERR", "Cannot request CC params: no Companion heartbeat yet", 3);
        return;
    }
    // A second LIST while one runs makes the CC send the whole list again (`another_pass`, mavlink_parameters.cpp:838), and
    // every popup open and every refused click would repeat that on the SIYI UART. A stuck load ends by the 6 s timeout.
    if (_isLoading) return;

    mavlink_message_t msg;
    mavlink_msg_param_ext_request_list_pack_chan(MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId(),
        _transport.channel, &msg, static_cast<uint8_t>(_transport.systemId), kCompanionComponentId);
    if (!_transport.send(msg)) {
        emit logMessage("PARAM", "ERR", "Cannot request CC params: send failed", 3);
        return; // not loading: nothing will answer, and loading refuses every operation
    }
    _isLoading = true;
    _receivedIndices.clear();
    _expectedCount = 0;
    _loadingTimeoutTimer->start();
    emit isLoadingChanged();
    emit logMessage("PARAM", "TX", QString("PARAM_EXT_REQUEST_LIST sent to Comp %1").arg(kCompanionComponentId), 6);
    // The CC leaves out parameters it cannot read at this moment; ask for those individually once the LIST is done.
    QTimer::singleShot(4000, this, [this]() { readMissing(); });
}

void CompanionParamService::stageParameter(const QString& name, const QVariant& value)
{
    if (!_meta.contains(name)) return;
    if (_meta[name].readOnly) return;

    _stagedValues[name] = value;

    // Modified against what the CC confirmed (nothing yet: any value is a change).
    bool modified = (value.toString() != confirmedValue(name).toString());
    _isModified[name] = modified;

    _updateModifiedCount();
    emit paramListChanged();
}

void CompanionParamService::resetParameter(const QString& name)
{
    if (!_meta.contains(name)) return;

    _stagedValues[name] = confirmedValue(name);
    _isModified[name] = false;

    _updateModifiedCount();
    emit paramListChanged();
}

void CompanionParamService::resetAllModified()
{
    for (const QString& name : _order) {
        if (_isModified.value(name, false)) {
            _stagedValues[name] = confirmedValue(name);
            _isModified[name] = false;
        }
    }
    _updateModifiedCount();
    emit paramListChanged();
}

QStringList CompanionParamService::saveModifiedParameters()
{
    QStringList sent;
    if (!_transport.valid()) {
        emit logMessage("PARAM", "ERR", "Cannot save CC params: no Companion heartbeat yet", 3);
        return sent;
    }

    for (const QString& name : _order) {
        if (_isModified.value(name, false) && !_meta[name].readOnly) {
            const QVariant confirmed = confirmedValue(name);
            if (!_isPending.value(name) && confirmed.isValid() && confirmed.toString() == _stagedValues.value(name).toString()) {
                _isModified[name] = false; // the CC already has this value: nothing to write, no ACK to wait for
                continue;
            }
            sendSingleParamSet(name, _stagedValues.value(name));
            sent.append(name);
        }
    }
    _updateModifiedCount();

    if (!sent.isEmpty()) {
        emit logMessage("PARAM", "TX", QString("Applying %1 modified CC parameter(s)...").arg(sent.size()), 6);
    }
    return sent;
}

int CompanionParamService::stagedSubsystem(const QString& name) const
{
    const auto meta = _meta.constFind(name);
    if (meta == _meta.cend() || meta->applyMode != QLatin1String("staged")) return 0;
    if (meta->group == QLatin1String("Telemetry")) return 1;
    if (meta->group == QLatin1String("Network")) return 3;
    return 0;
}

QList<Fact*> CompanionParamService::allFacts() const
{
    QList<Fact*> facts;
    for (const QString& name : _order) {
        if (Fact* fact = _facts.value(name, nullptr)) facts.append(fact);
    }
    return facts;
}

bool CompanionParamService::sendSingleParamSet(const QString& name, const QVariant& value)
{
    if (!_transport.valid()) return false;
    if (!_meta.contains(name) || _meta[name].readOnly) return true;
    // A lost ACK leaves a write uncertain; rollback must resend even the old readback value.
    if (!_isPending.value(name) && confirmedValue(name).isValid() && confirmedValue(name).toString() == value.toString()) return true;

    const auto& meta = _meta[name];
    char paramIdBuf[16];
    std::memset(paramIdBuf, 0, sizeof(paramIdBuf));
    QByteArray idBytes = name.toUtf8();
    std::memcpy(paramIdBuf, idBytes.constData(), std::min<size_t>(idBytes.size(), 16));

    char valBuf[128];
    _encodeValue(value, meta.type, valBuf);

    mavlink_message_t msg;
    mavlink_msg_param_ext_set_pack_chan(MAVLinkProtocol::instance()->getSystemId(), MAVLinkProtocol::getComponentId(),
        _transport.channel, &msg, static_cast<uint8_t>(_transport.systemId), kCompanionComponentId, paramIdBuf, valBuf, meta.type);

    if (!_transport.send(msg)) return false;
    _isPending[name] = true;
    emit logMessage("PARAM", "TX", QString("PARAM_EXT_SET: %1 = %2").arg(name, loggable(name, value)), 6);
    return true;
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
        if (_meta[name].isSecret) continue;
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
            // Every link the CC reports shares link 0's schema: the link count comes from the CC (spec §6).
            static const QRegularExpression linkParam(QStringLiteral("^CC_L\\d+_"));
            const QString linkTemplate = QString(paramId).replace(linkParam, QStringLiteral("CC_L0_"));
            CCParamMeta dynMeta;
            if (linkTemplate != paramId && _meta.contains(linkTemplate)) {
                dynMeta = _meta.value(linkTemplate);
            } else {
                dynMeta.group = "Other";
                dynMeta.type = pval.param_type;
                dynMeta.readOnly = true;
                dynMeta.applyMode = "boot_only";
            }
            dynMeta.name = paramId;
            dynMeta.label = paramId;
            dynMeta.defaultValue = val;
            _order.append(paramId);
            _meta[paramId] = dynMeta;
            if (!_groups.contains(dynMeta.group)) {
                _groups.append(dynMeta.group);
                emit groupsChanged();
            }
        }

        _receivedIndices.insert(pval.param_index);
        _expectedCount = pval.param_count;

        // Reset timer on each incoming param
        if (_isLoading) {
            _loadingTimeoutTimer->start();
        }

        if (_isLoading && _expectedCount > 0 && _receivedIndices.size() >= _expectedCount) {
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
        // Only an ACCEPTED ACK carries a value the CC confirmed; a refusal may carry zeros (CC had none) and changes nothing.
        if (pack.param_result == PARAM_ACK_ACCEPTED) {
            _liveValues[paramId] = ackVal;
            _isLiveSynced[paramId] = true;
        }

        if (pack.param_result == PARAM_ACK_IN_PROGRESS) {
            _isPending[paramId] = true;
            emit logMessage("PARAM", "RX", QString("PARAM_EXT_ACK: %1 in progress").arg(paramId), 6);
            emit parameterAckInProgress(paramId);
        } else if (pack.param_result == 0 /* PARAM_ACK_ACCEPTED */) {
            _isPending[paramId] = false;
            _stagedValues[paramId] = ackVal;
            _isModified[paramId] = false;
            _updateModifiedCount();
            emit parameterSaved(paramId, true, QString("Parameter %1 applied successfully").arg(paramId));
            emit logMessage("PARAM", "RX", QString("PARAM_EXT_ACK: %1 = %2 (Accepted)").arg(paramId, loggable(paramId, ackVal)), 6);
        } else {
            _isPending[paramId] = false;
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
        emit logMessage("PARAM", "WARN", QString("Param request timed out (%1 received)").arg(_receivedIndices.size()), 4);
        readMissing(); // the load ended by timeout, so the CC left values out; read those now (the 4 s timer ran while the LIST was still arriving)
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
        return QString::fromUtf8(buf);
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

int CompanionParamService::syncedCount() const
{
    int count = 0;
    for (auto it = _isLiveSynced.cbegin(); it != _isLiveSynced.cend(); ++it) count += it.value() ? 1 : 0;
    return count;
}

void CompanionParamService::readMissing()
{
    if (!_transport.valid()) return;
    if (_backgroundHold && _backgroundHold()) return;
    if (_isLoading) return; // the running LIST brings the values; a LIST longer than 4 s ends in _onLoadingTimeout() or the next LIST, which run readMissing()

    int requested = 0;
    for (const QString& name : _order) {
        if (_isLiveSynced.value(name) || _meta.value(name).isSecret) continue;
        char idBuf[16] = {};
        const QByteArray id = name.toUtf8();
        std::memcpy(idBuf, id.constData(), std::min<size_t>(id.size(), sizeof(idBuf)));
        mavlink_message_t msg;
        mavlink_msg_param_ext_request_read_pack_chan(MAVLinkProtocol::instance()->getSystemId(),
            MAVLinkProtocol::getComponentId(), _transport.channel, &msg,
            static_cast<uint8_t>(_transport.systemId), kCompanionComponentId, idBuf, -1);
        _transport.send(msg);
        ++requested;
    }
    if (requested) emit logMessage("PARAM", "TX", QString("PARAM_EXT_REQUEST_READ for %1 parameter(s) missing after LIST").arg(requested), 6);
}
