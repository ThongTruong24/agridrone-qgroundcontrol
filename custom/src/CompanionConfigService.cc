#include "CompanionConfigService.h"

#include <algorithm>
#include <cstddef>

namespace {
constexpr quint8 kCompanionComponentId = 191;

template <std::size_t Size>
QString fixedMavlinkString(const char (&value)[Size])
{
    const auto end = std::find(value, value + Size, '\0');
    return QString::fromUtf8(value, static_cast<qsizetype>(end - value));
}
}  // namespace

CompanionConfigService::CompanionConfigService(QObject* parent) : QObject(parent)
{
    _timeout.setSingleShot(true);
    (void) connect(&_timeout, &QTimer::timeout, this, &CompanionConfigService::_timedOut);
}

QString CompanionConfigService::stateName() const
{
    switch (_state) {
        case State::Unavailable:
            return QStringLiteral("Unavailable");
        case State::Idle:
            return QStringLiteral("Idle");
        case State::Beginning:
            return QStringLiteral("Beginning");
        case State::Staging:
            return QStringLiteral("Staging");
        case State::Applying:
            return QStringLiteral("Applying");
        case State::WaitingConfirm:
            return QStringLiteral("WaitingConfirm");
        case State::Committed:
            return QStringLiteral("Committed");
        case State::Rollback:
            return QStringLiteral("Rollback");
        case State::Failed:
            return QStringLiteral("Failed");
        case State::Timeout:
            return QStringLiteral("Timeout");
    }
    return QStringLiteral("Unavailable");
}

void CompanionConfigService::reset()
{
    _timeout.stop();
    _pendingRequestId = 0;
    _transactionId = 0;
    _state = State::Unavailable;
    _progress = 0;
    _errorCode = 0;
    _message = QStringLiteral("Configuration protocol unavailable");
    _lastAck.clear();
    _lastValue.clear();
    _lastStatus.clear();
    emit changed();
}

void CompanionConfigService::processMessage(const mavlink_message_t& message, quint8 activeSystemId)
{
    if (message.sysid != activeSystemId || message.compid != kCompanionComponentId) {
        return;
    }

    switch (message.msgid) {
        case MAVLINK_MSG_ID_CC_CONFIG_ACK: {
            mavlink_cc_config_ack_t packet{};
            mavlink_msg_cc_config_ack_decode(&message, &packet);
            if (!_pendingRequestId || packet.request_id != _pendingRequestId ||
                (_transactionId && packet.transaction_id != _transactionId)) {
                return;
            }
            _lastAck = {{QStringLiteral("request_id"), packet.request_id},
                        {QStringLiteral("transaction_id"), packet.transaction_id},
                        {QStringLiteral("result"), packet.result},
                        {QStringLiteral("apply_type"), packet.apply_type},
                        {QStringLiteral("error_code"), packet.error_code},
                        {QStringLiteral("message"), fixedMavlinkString(packet.message)}};
            _transactionId = packet.transaction_id;
            _errorCode = packet.error_code;
            _message = fixedMavlinkString(packet.message);
            _pendingRequestId = 0;
            _timeout.stop();
            if (packet.result == 2 || packet.result == 3 || packet.result == 4) {
                _state = packet.result == 3 ? State::Timeout : (packet.result == 4 ? State::Rollback : State::Failed);
            } else {
                _state = State::Staging;
            }
            emit changed();
            break;
        }
        case MAVLINK_MSG_ID_CC_CONFIG_VALUE: {
            mavlink_cc_config_value_t packet{};
            mavlink_msg_cc_config_value_decode(&message, &packet);
            if (!_pendingRequestId || packet.request_id != _pendingRequestId) {
                return;
            }
            _lastValue = {{QStringLiteral("request_id"), packet.request_id},
                          {QStringLiteral("key"), fixedMavlinkString(packet.key)},
                          {QStringLiteral("value"), fixedMavlinkString(packet.value)}};
            _pendingRequestId = 0;
            _timeout.stop();
            emit changed();
            break;
        }
        case MAVLINK_MSG_ID_CC_CONFIG_STATUS: {
            mavlink_cc_config_status_t packet{};
            mavlink_msg_cc_config_status_decode(&message, &packet);
            if (!_transactionId || packet.transaction_id != _transactionId) {
                return;
            }
            _lastStatus = {{QStringLiteral("transaction_id"), packet.transaction_id},
                           {QStringLiteral("state"), packet.state},
                           {QStringLiteral("progress"), packet.progress},
                           {QStringLiteral("error_code"), packet.error_code},
                           {QStringLiteral("message"), fixedMavlinkString(packet.message)}};
            _progress = packet.progress;
            _errorCode = packet.error_code;
            _message = fixedMavlinkString(packet.message);
            switch (packet.state) {
                case 1:
                    _state = State::Staging;
                    break;
                case 2:
                    _state = State::Applying;
                    break;
                case 3:
                    _state = State::WaitingConfirm;
                    break;
                case 4:
                    _state = State::Committed;
                    break;
                case 5:
                    _state = State::Rollback;
                    break;
                default:
                    _state = State::Idle;
                    break;
            }
            emit changed();
            break;
        }
        default:
            break;
    }
}

void CompanionConfigService::_timedOut()
{
    _pendingRequestId = 0;
    _transactionId = 0;
    _state = State::Timeout;
    _message = QStringLiteral("Configuration request timed out");
    emit changed();
}
