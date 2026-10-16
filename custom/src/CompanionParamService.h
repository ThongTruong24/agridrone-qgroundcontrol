#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <functional>

#include "ITelemetryHandler.h"

class Fact;

/// Where PARAM_EXT_* goes: the CC's system id, the MAVLink channel of the link its heartbeat came in on, and the send
/// function. No Vehicle: the CC must be configurable while the FC is silent.
struct CompanionTransport {
    int systemId{-1};
    uint8_t channel{0};
    std::function<bool(const mavlink_message_t&)> send;
    bool valid() const { return systemId > 0 && bool(send); }
};

struct CCParamMeta {
    QString name;
    QString label;
    QString group;
    QString description;
    uint8_t type = 0;        // MAV_PARAM_EXT_TYPE_*
    QString typeStr;         // "string", "uint32", "real32", "int32"
    QVariant defaultValue;
    QStringList options;     // Choices for ComboBox
    double minVal = 0.0;
    double maxVal = 0.0;
    bool hasRange = false;
    QString units;
    bool requiresReboot = false;
    bool readOnly = false;
    bool isSecret = false;
    QString applyMode;       // "staged", "live", "boot_only"
};

/**
 * @brief Service responsible for MAVLink PARAM_EXT extended parameter management
 *        for Companion Computer (CompID 191).
 */
class CompanionParamService : public QObject, public ITelemetryHandler
{
    Q_OBJECT

    Q_PROPERTY(QVariantList paramList READ paramList NOTIFY paramListChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(int modifiedCount READ modifiedCount NOTIFY modifiedCountChanged)
    Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)

public:
    explicit CompanionParamService(QObject* parent = nullptr);
    ~CompanionParamService() override = default;

    // ITelemetryHandler interface
    bool handleMavlinkMessage(const mavlink_message_t& message) override;
    void resetState() override;
    /// The bundled catalog; tests load another file. No values or defaults: each value comes from the CC (spec §6).
    void loadTemplate(const QString& path = QStringLiteral(":/custom/params/cc_parameters.json"));
    /// The catalog did not load: there are no parameters, and nothing can be configured.
    bool loadError() const { return _loadError; }

    QVariantList paramList() const;
    bool isLoading() const { return _isLoading; }
    int modifiedCount() const { return _modifiedCount; }
    QStringList groups() const { return _groups; }

    void setTransport(CompanionTransport t) { _transport = std::move(t); }
    void requestParameters();
    void stageParameter(const QString& name, const QVariant& value);
    void resetParameter(const QString& name);
    void resetAllModified();
    /// Sends PARAM_EXT_SET for every modified parameter and returns the names it sent (their ACKs arrive via parameterSaved).
    QStringList saveModifiedParameters();
    /// Parameters the CC has not yet reported; read one by one after a LIST because the CC omits what it cannot read.
    void readMissing();
    int syncedCount() const;
    bool isModified(const QString& name) const { return _isModified.value(name, false); }
    QStringList allFactNames() const { return _order; }
    QVariant stagedValue(const QString& name) const { return _stagedValues.value(name); }
    QString stagedText(const QString& name) const { return _meta.value(name).isSecret ? QStringLiteral("<hidden>") : _stagedValues.value(name).toString(); }
    /// ConfigManager subsystem that needs an APPLY command for this staged parameter (1 links, 3 hotspot); 0 = takes effect on SET.
    int stagedSubsystem(const QString& name) const;
    /// Facts for the Parameters editor, in catalog order. Edits made on them are staged here.
    QList<Fact*> allFacts() const;
    /// false only when the PARAM_EXT_SET could not be sent (no transport, or the send failed); a SET it skips counts as sent.
    bool sendSingleParamSet(const QString& name, const QVariant& value);

    Fact* parameterFact(const QString& name) const { return _facts.value(name, nullptr); }
    bool isSecret(const QString& name) const { return _meta.value(name).isSecret; }
    bool isReadOnly(const QString& name) const { return _meta.value(name).readOnly; }
    QStringList options(const QString& name) const { return _meta.value(name).options; }
    QVariant confirmedValue(const QString& name) const { return _isLiveSynced.value(name) ? _liveValues.value(name) : QVariant(); }
    /// confirmedValue() is invalid again until the CC reports the parameter anew.
    void forgetConfirmed(const QString& name) { _isLiveSynced[name] = false; }

    bool exportParameters(const QString& filePath);
    bool importParameters(const QString& filePath);

signals:
    void paramListChanged();
    void isLoadingChanged();
    void modifiedCountChanged();
    void groupsChanged();
    void parameterSaved(const QString& name, bool success, const QString& message);
    void parameterAckInProgress(const QString& name);
    void logMessage(const QString& category, const QString& direction, const QString& message, int severity);

private slots:
    void _onLoadingTimeout();

private:
    void _syncFacts();
    QMap<QString, Fact*> _facts;
    void _updateModifiedCount();
    QVariant _decodeValue(const char* raw, uint8_t type) const;
    void _encodeValue(const QVariant& val, uint8_t type, char* outRaw) const;

    bool _isLoading = false;
    bool _loadError = false;
    int _modifiedCount = 0;
    uint16_t _expectedCount = 0;
    QSet<uint16_t> _receivedIndices; // a LIST is done when every index arrived, whatever else is resent
    QTimer* _loadingTimeoutTimer = nullptr;

    QStringList _groups;
    QStringList _order;
    QMap<QString, CCParamMeta> _meta;

    // Live authoritative values received from CC
    QMap<QString, QVariant> _liveValues;
    QMap<QString, bool> _isLiveSynced;
    // Current / staged values edited in QGC UI
    QMap<QString, QVariant> _stagedValues;
    QMap<QString, bool> _isModified;
    QMap<QString, bool> _isPending;
    CompanionTransport _transport;
};
