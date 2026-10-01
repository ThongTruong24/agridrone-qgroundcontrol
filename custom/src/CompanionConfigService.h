#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>

#include "QGCMAVLink.h"

// Receive-side state for the CC_CONFIG protocol. Sending stays unavailable until
// the config agent defines the namespaced keys accepted by CC_CONFIG_SET.
class CompanionConfigService : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Unavailable,
        Idle,
        Beginning,
        Staging,
        Applying,
        WaitingConfirm,
        Committed,
        Rollback,
        Failed,
        Timeout
    };

    explicit CompanionConfigService(QObject* parent = nullptr);

    bool available() const { return false; }

    State state() const { return _state; }

    QString stateName() const;

    int progress() const { return _progress; }

    int errorCode() const { return _errorCode; }

    QString message() const { return _message; }

    QVariantMap lastAck() const { return _lastAck; }

    QVariantMap lastValue() const { return _lastValue; }

    QVariantMap lastStatus() const { return _lastStatus; }

    void reset();
    void processMessage(const mavlink_message_t& message, quint8 activeSystemId);
#ifdef QGC_UNITTEST_BUILD
    void setPendingForTest(quint32 requestId, quint32 transactionId = 0)
    {
        _pendingRequestId = requestId;
        _transactionId = transactionId;
        _state = State::Beginning;
        _timeout.start(3500);
    }
#endif

signals:
    void changed();

private:
    void _timedOut();

    State _state = State::Unavailable;
    QTimer _timeout;
    quint32 _pendingRequestId = 0;
    quint32 _transactionId = 0;
    int _progress = 0;
    int _errorCode = 0;
    QString _message = QStringLiteral("Configuration protocol unavailable");
    QVariantMap _lastAck;
    QVariantMap _lastValue;
    QVariantMap _lastStatus;
};
